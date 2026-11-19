#include "SdCardFontSystem.h"

#include <FontCacheManager.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <MemoryBudget.h>
#include <TtfEpdFont.h>
#include <esp_heap_caps.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>

#include "CrossPointSettings.h"
#include "ReaderFontSizes.h"
#include "TtfRenderProfileStore.h"
#include "fontIds.h"

namespace {

#if CROSSPOINT_VECTOR_FONTS
// Stable, non-zero renderer font id for a vector family at a size (FNV-1a of
// name + size). 0 is the "not found" sentinel, so bump collisions to 1.
int computeTtfFontId(const char* familyName, uint8_t pointSize, uint32_t identity = 0) {
  uint32_t hash = 2166136261u;
  for (const char* p = familyName; p && *p; ++p) {
    hash ^= static_cast<uint8_t>(*p);
    hash *= 16777619u;
  }
  hash ^= identity;
  hash *= 16777619u;
  hash ^= pointSize;
  hash *= 16777619u;
  hash ^= 0x54544600u;  // "TTF\0" salt to avoid colliding with cpfont ids
  const int id = static_cast<int>(hash);
  return id != 0 ? id : 1;
}
#endif  // CROSSPOINT_VECTOR_FONTS

}  // namespace

// Out-of-line ctor/dtor: TtfEpdFont is complete here, so unique_ptr<TtfEpdFont>
// can be constructed/destroyed. (Declared in the header where it is only
// forward-declared.)
SdCardFontSystem::SdCardFontSystem() = default;
SdCardFontSystem::~SdCardFontSystem() = default;

namespace {

// Point the reader font size at a size the given family actually ships, and
// persist the change so the settings UI and the loaded font never disagree.
// Guarded by the value-change check: a no-op snap must not write SPIFFS.
void snapFontPointSizeTo(const uint8_t availablePointSize) {
  if (availablePointSize == 0 || availablePointSize == SETTINGS.fontPointSize) return;
  LOG_DBG("SDFS", "Font size %u unavailable, snapping to %u", SETTINGS.fontPointSize, availablePointSize);
  SETTINGS.fontPointSize = availablePointSize;
  SETTINGS.saveToFile();
}

// Built-in UI fonts and their physical point sizes (at 150 DPI, matching the
// SD-font converter). Each is paired with a same-size SD fallback so UI text
// in scripts the built-ins lack (CJK, Greek, Cyrillic, ...) matches the
// surrounding Latin. See SdCardFontSystem::setupUiFallbacks.
struct UiFontSize {
  int fontId;
  uint8_t pointSize;
};
constexpr UiFontSize kUiFontSizes[] = {
    {SMALL_FONT_ID, 8},
    {UI_10_FONT_ID, 10},
    {UI_12_FONT_ID, 12},
};

}  // namespace

void SdCardFontSystem::begin(GfxRenderer& renderer) {
  if (!TTF_RENDER_PROFILES.loadFromFile()) LOG_ERR("SDFS", "TTF profile store unreadable; not overwriting it");
  renderer_ = &renderer;
  cpfontVariants_.reserve(MAX_CPFONT_VARIANTS);
#if CROSSPOINT_VECTOR_FONTS
  ttfVariants_.reserve(MAX_TTF_VARIANTS);
  if (auto* fcm = renderer.getFontCacheManager()) {
    fcm->setReleaseCallback(
        [](void* ctx) {
          auto* self = static_cast<SdCardFontSystem*>(ctx);
          if (self->renderer_) self->clearTtfVariants(*self->renderer_);
        },
        this);
  }
#endif
  FontVariantProvider provider;
  provider.ctx = this;
  provider.resolve = [](void* ctx, const int fontId, const float scale) {
    return static_cast<SdCardFontSystem*>(ctx)->resolveVariant(fontId, scale);
  };
  provider.load = [](void* ctx, const int fontId) { return static_cast<SdCardFontSystem*>(ctx)->loadVariant(fontId); };
  renderer.setFontVariantProvider(provider);

  // Register this system as the SD font ID resolver in settings.
  // Uses a static trampoline since CrossPointSettings stores a plain function pointer.
  SETTINGS.sdFontIdResolver = [](void* ctx, const char* familyName, uint8_t pointSize) -> int {
    return static_cast<SdCardFontSystem*>(ctx)->resolveFontId(familyName, pointSize);
  };
  SETTINGS.sdFontResolverCtx = this;

  ensureLoaded(renderer);
  releaseRegistry();
}

void SdCardFontSystem::ensureLoaded(GfxRenderer& renderer) {
  const bool dirty = registryDirty_.load(std::memory_order_acquire) || fontReloadPending_;
  if (!SETTINGS.sdFontFamilyName[0]) {
    releaseLoadedFont(renderer);
    return;
  }
  ensureRegistry();
  if (registry_.lastDiscoveryFailed()) return;
  uint8_t size = SETTINGS.getSdFontTargetPointSize();
  if (SETTINGS.legacySdFontSizeStep != UINT8_MAX) {
    const uint8_t migrated = resolveLegacySizeStep(SETTINGS.sdFontFamilyName, SETTINGS.legacySdFontSizeStep);
    if (migrated) {
      size = migrated;
      SETTINGS.readerFontPointSize = size;
      SETTINGS.legacySdFontSizeStep = UINT8_MAX;
      persistSettingsChange();
    }
  }
  if (selectFamily(renderer, SETTINGS.sdFontFamilyName, size, dirty || loadedRegistryRevision_ != registry_.revision(),
                   true)) {
    const uint8_t actual =
#if CROSSPOINT_VECTOR_FONTS
        ttf_ ? ttfPointSize_ :
#endif
             manager_.currentPointSize();
    if (actual && SETTINGS.readerFontPointSize != actual) {
      SETTINGS.readerFontPointSize = actual;
      persistSettingsChange();
    }
    fontReloadPending_ = false;
    loadedRegistryRevision_ = registry_.revision();
  }
}
bool SdCardFontSystem::selectFamily(GfxRenderer& renderer, const char* name, uint8_t size, bool dirty, bool ui) {
  const auto* family = registry_.findFamily(name ? name : "");
  if (!family) return false;  // absence/IO never clears the user's saved selection
#if CROSSPOINT_VECTOR_FONTS
  if (family->vector) {
    if (!manager_.currentFamilyName().empty()) unloadCpfonts(renderer);
    loadTtfFamily(*family, renderer, dirty, size, ui);
    return ttf_ && ttfFamily_ == name && ttfFontId_ != 0;
  }
  if (ttf_) unloadTtf(renderer);
#endif
  const auto* file = family->findNearestSize(size);
  if (!file) return false;
  if (!dirty && manager_.currentFamilyName() == name && manager_.currentPointSize() == file->pointSize) {
    if (ui) setupUiFallbacks(renderer);
    return true;
  }
  unloadCpfonts(renderer);
  if (!manager_.loadFamily(*family, renderer, size)) return false;
  if (ui) setupUiFallbacks(renderer);
  return true;
}

void SdCardFontSystem::setupUiFallbacks(GfxRenderer& renderer) {
  const std::string& familyName = manager_.currentFamilyName();
  if (familyName.empty()) return;  // no SD family loaded — nothing to fall back to

  const auto* family = registry_.findFamily(familyName);
  if (!family) return;

  // Probe the already-loaded reader-size font before paying for the UI sizes:
  // resolveTextFontId only redirects on codepoints the built-in UI fonts lack,
  // so a family with no coverage beyond theirs can never act as a fallback and
  // its UI sizes would be dead weight in RAM.
  const auto readerIt = renderer.getFontMap().find(manager_.getFontId(familyName));
  if (readerIt == renderer.getFontMap().end()) return;
  // One representative codepoint per script the built-in fonts may lack,
  // matching the non-Latin interval presets the SD-font converter ships
  // (docs/sd-card-fonts.md).
  static constexpr uint32_t kFallbackProbes[] = {
      0x4E00,  // CJK Unified Ideographs (Han)
      0x3042,  // Hiragana
      0x30A2,  // Katakana
      0xAC00,  // Hangul Syllables
      0x03B1,  // Greek
      0x0430,  // Cyrillic
      0x05D0,  // Hebrew
      0x0627,  // Arabic
      0x0531,  // Armenian
      0x10D0,  // Georgian
      0x1200,  // Ethiopic
      0x13A0,  // Cherokee
      0x2D30,  // Tifinagh
      0x0259,  // IPA Extensions (ə)
  };
  bool hasFallbackScript = false;
  for (const uint32_t cp : kFallbackProbes) {
    if (readerIt->second.hasCodepoint(cp)) {
      hasFallbackScript = true;
      break;
    }
  }
  if (!hasFallbackScript) {
    LOG_DBG("SDFS", "%s has no fallback-script coverage - skipping UI fallback sizes", familyName.c_str());
    return;
  }

  for (const auto& ui : kUiFontSizes) {
    // Use the persisted CSS variant id so UI and reader text share one prewarm set.
    const int id = variantFontId(manager_.getFontId(familyName), ui.pointSize);
    const int sdFontId = manager_.loadFamilyExtraSize(*family, renderer, ui.pointSize, id);
    if (sdFontId != 0) {
      renderer.setFallbackFont(ui.fontId, sdFontId);
    } else {
      LOG_DBG("SDFS", "No %u pt SD glyphs for UI fallback in %s", ui.pointSize, familyName.c_str());
    }
  }
}

int SdCardFontSystem::variantFontId(const int readerFontId, const uint8_t pointSize) {
  uint32_t hash = 2166136261u;
  for (int shift = 0; shift < 32; shift += 8) {
    hash ^= static_cast<uint8_t>(static_cast<uint32_t>(readerFontId) >> shift);
    hash *= 16777619u;
  }
  hash ^= pointSize;
  hash *= 16777619u;
  hash ^= 0x56415200u;  // "VAR\0" salt keeps variants apart from reader and UI ids
  const int id = static_cast<int>(hash);
  return id != 0 ? id : 1;
}

void SdCardFontSystem::unloadCpfonts(GfxRenderer& renderer) {
  cpfontVariants_.clear();  // the manager owns and frees the fonts themselves
  manager_.unloadAll(renderer);
}

int SdCardFontSystem::resolveVariant(const int fontId, const float scale) {
  if (!renderer_ || !std::isfinite(scale) || scale <= 0) return 0;
  ensureRegistry();
  if (registry_.lastDiscoveryFailed()) return 0;
  static constexpr int ownerIds[][4] = {
      {LEXENDDECA_10_FONT_ID, LEXENDDECA_12_FONT_ID, LEXENDDECA_14_FONT_ID, LEXENDDECA_16_FONT_ID},
      {BITTER_10_FONT_ID, BITTER_12_FONT_ID, BITTER_14_FONT_ID, BITTER_16_FONT_ID}};
  constexpr uint8_t points[] = {10, 12, 14, 16};
  for (const auto& ids : ownerIds)
    for (size_t i = 0; i < 4; ++i)
      if (ids[i] == fontId) {
        size_t best = scale < 1 ? 0 : i;
        float target = points[i] * scale;
        for (size_t k = 0; k < 4; ++k)
          if (scale < 1 ? points[k] <= target : std::fabs(points[k] - target) < std::fabs(points[best] - target))
            best = k;
        return renderer_->getFontMap().count(ids[best]) ? ids[best] : 0;
      }
  if (const int builtin = builtinReaderFontVariant(fontId, scale); builtin != 0) {
    return renderer_->getFontMap().count(builtin) ? builtin : 0;
  }
#if CROSSPOINT_VECTOR_FONTS
  if (ttf_ && fontId == ttfFontId_) {
    // Vector faces render at any size.
    const int pointSize = std::clamp(static_cast<int>(ttfPointSize_ * scale + 0.5f), 6, 72);
    if (pointSize == ttfPointSize_) return fontId;
    const int id = variantFontId(fontId, static_cast<uint8_t>(pointSize));
    return loadTtfVariant(static_cast<uint8_t>(pointSize), id) ? id : 0;
  }
#endif
  const std::string& familyName = manager_.currentFamilyName();
  if (familyName.empty() || fontId != manager_.getFontId(familyName)) return 0;
  const auto* family = registry_.findFamily(familyName);
  if (!family) return 0;

  // Pick among the sizes installed for the family: shrinking takes the largest size at or
  // below the target so small print never rounds back up to body size; growing takes the
  // nearest.
  const uint8_t base = manager_.currentPointSize();
  const float target = base * scale;
  uint8_t chosen = base;
  if (scale < 1.0f) {
    uint8_t atOrBelow = 0;
    uint8_t smallest = base;
    for (const auto& file : family->files) {
      if (file.pointSize <= target && file.pointSize > atOrBelow) atOrBelow = file.pointSize;
      smallest = std::min(smallest, file.pointSize);
    }
    chosen = atOrBelow != 0 ? atOrBelow : smallest;
  } else {
    for (const auto& file : family->files) {
      if (std::fabs(file.pointSize - target) < std::fabs(chosen - target)) chosen = file.pointSize;
    }
  }
  if (chosen == base) return fontId;
  const int id = variantFontId(fontId, chosen);
  return loadCpfontVariant(*family, chosen, id) ? id : 0;
}

bool SdCardFontSystem::loadVariant(const int fontId) {
  ensureRegistry();
  if (registry_.lastDiscoveryFailed()) return false;
#if CROSSPOINT_VECTOR_FONTS
  if (ttf_) {
    for (int pointSize = 6; pointSize <= 72; ++pointSize) {
      if (variantFontId(ttfFontId_, static_cast<uint8_t>(pointSize)) == fontId) {
        return loadTtfVariant(static_cast<uint8_t>(pointSize), fontId);
      }
    }
    return false;
  }
#endif
  const std::string& familyName = manager_.currentFamilyName();
  if (familyName.empty()) return false;
  const auto* family = registry_.findFamily(familyName);
  if (!family) return false;
  const int readerFontId = manager_.getFontId(familyName);
  for (const auto& file : family->files) {
    if (variantFontId(readerFontId, file.pointSize) == fontId) {
      return loadCpfontVariant(*family, file.pointSize, fontId);
    }
  }
  return false;
}

bool SdCardFontSystem::loadCpfontVariant(const SdCardFontFamilyInfo& family, const uint8_t pointSize,
                                         const int fontId) {
  for (auto& variant : cpfontVariants_) {
    if (variant.fontId == fontId) {
      variant.lastUse = ++variantClock_;
      return true;
    }
  }
  // UI fallback sizes stay resident and must not enter the variant eviction list.
  if (manager_.getFontId(family.name, pointSize) == fontId) return true;
  // Each size keeps its interval tables resident (small for Latin, tens of KB for broad
  // CJK subsets), so skip it on a tight heap: the text then stays at the reader size.
  static constexpr size_t MIN_VARIANT_FREE_BLOCK = 48 * 1024;
  if (ESP.getMaxAllocHeap() < MIN_VARIANT_FREE_BLOCK) {
    LOG_DBG("SDFS", "Skipping %upt variant: largest free block %u", pointSize,
            static_cast<unsigned>(ESP.getMaxAllocHeap()));
    return false;
  }
  if (cpfontVariants_.size() >= MAX_CPFONT_VARIANTS) {
    const auto oldest = std::min_element(cpfontVariants_.begin(), cpfontVariants_.end(),
                                         [](const auto& a, const auto& b) { return a.lastUse < b.lastUse; });
    manager_.unloadFont(*renderer_, oldest->fontId);
    cpfontVariants_.erase(oldest);
  }
  if (!manager_.loadFamilyVariant(family, *renderer_, pointSize, fontId)) return false;
  cpfontVariants_.push_back({fontId, ++variantClock_});
  LOG_DBG("SDFS", "Loaded %s %upt variant (heap free %u)", family.name.c_str(), pointSize,
          static_cast<unsigned>(ESP.getFreeHeap()));
  return true;
}

int SdCardFontSystem::resolveFontId(const char* familyName, uint8_t /*pointSize*/) const {
#if CROSSPOINT_VECTOR_FONTS
  // A loaded vector (.ttf) family answers first — it isn't in the .cpfont manager.
  if (ttfFontId_ != 0 && familyName && ttfFamily_ == familyName) return ttfFontId_;
#endif
  // The manager holds exactly one reader-size font, already selected for
  // SETTINGS.fontPointSize, so the size argument is implicit — always return
  // that font's ID. ensureLoaded() must have run for the current settings first.
  return manager_.getFontId(familyName);
}

#if CROSSPOINT_VECTOR_FONTS

void SdCardFontSystem::freeTtfSources() {
  for (auto& s : ttfSources_) {
    s.bytes.clear();
    freeink::font::PsramVector<uint8_t>().swap(s.bytes);  // actually release
    if (s.file) s.file.close();
    s.streamed = false;
    s.size = 0;
    s.present = false;
  }
}

bool SdCardFontSystem::loadTtfVariant(const uint8_t pointSize, const int fontId) {
  if (!ttf_) return false;
  for (auto& variant : ttfVariants_) {
    if (variant.fontId == fontId) {
      variant.lastUse = ++variantClock_;
      return true;
    }
  }
  if (ttfVariants_.size() >= MAX_TTF_VARIANTS) {
    const auto oldest = std::min_element(ttfVariants_.begin(), ttfVariants_.end(),
                                         [](const auto& a, const auto& b) { return a.lastUse < b.lastUse; });
    renderer_->unregisterTtfFont(oldest->fontId);
    renderer_->removeFont(oldest->fontId);
    ttfVariants_.erase(oldest);
  }
  auto font = makeUniqueNoThrow<TtfEpdFont>();
  if (!font) {
    LOG_ERR("SDFS", "OOM: TtfEpdFont variant @%upt", pointSize);
    return false;
  }
  // Variants carry headings and inline runs, sharing ttf_'s FreeType faces.
  if (!font->loadVariantOf(*ttf_, pointSize, TTF_RENDER_PROFILES.profileFor(ttfFamily_.c_str()).twoBit,
                           /*glyphCacheBytes=*/64 * 1024, /*maxGlyphs=*/512))
    return false;
  font->build(" ");
  renderer_->insertFont(fontId, font->family());
  renderer_->registerTtfFont(fontId, font.get());
  ttfVariants_.push_back({fontId, ++variantClock_, std::move(font)});
  return true;
}

void SdCardFontSystem::clearTtfVariants(GfxRenderer& renderer) {
  for (const auto& variant : ttfVariants_) {
    renderer.unregisterTtfFont(variant.fontId);
    renderer.removeFont(variant.fontId);
  }
  ttfVariants_.clear();
}

void SdCardFontSystem::unloadTtf(GfxRenderer& renderer) {
  clearTtfVariants(renderer);  // they borrow ttfSources_
  // UI-size fallbacks first (they borrow ttfSources_).
  for (const int id : ttfUiIds_) {
    renderer.unregisterTtfFont(id);
    renderer.removeFont(id);
  }
  ttfUiIds_.clear();
  ttfUi_.clear();
  renderer.clearFallbackFonts();
  if (ttfFontId_ != 0) {
    renderer.unregisterTtfFont(ttfFontId_);
    renderer.removeFont(ttfFontId_);  // drop from the renderer's fontMap
  }
  ttf_.reset();  // frees the FT faces first (they read ttfSources_)
  freeTtfSources();
  ttfFamily_.clear();
  ttfFontId_ = 0;
  ttfPointSize_ = 0;
  ttfSourceIdentity_ = 0;
  ttfProfileIdentity_ = 0;
}

bool SdCardFontSystem::openTtfSource(const uint8_t style, const std::string& path) {
  HalFile digestFile = Storage.open(path.c_str());
  if (!digestFile || digestFile.isDirectory()) return false;
  uint32_t digest = 2166136261u;
  uint8_t chunk[128];
  size_t remaining = digestFile.size();
  while (remaining) {
    const size_t n = std::min(remaining, sizeof(chunk));
    if (digestFile.read(chunk, n) != static_cast<int>(n)) return false;
    for (size_t i = 0; i < n; ++i) digest = (digest ^ chunk[i]) * 16777619u;
    remaining -= n;
  }
  if (!digestFile.close()) return false;
  ttfSourceIdentity_ = (ttfSourceIdentity_ ^ digest ^ style) * 16777619u;

  if (style >= 4) return false;
  HalFile f = Storage.open(path.c_str());
  if (!f) {
    LOG_ERR("SDFS", "Failed to open TTF: %s", path.c_str());
    return false;
  }
  const size_t len = f.size();
  if (len == 0) {
    LOG_ERR("SDFS", "Empty TTF: %s", path.c_str());
    f.close();
    return false;
  }
  TtfSource& s = ttfSources_[style];
  s.file = std::move(f);  // kept open; prefixRead() reads it on demand
  s.streamed = true;
  // Cache the file's head in PSRAM: an sfnt's per-glyph-fault tables (cmap,
  // loca, hmtx) sit before the multi-MB glyf table, so serving the first
  // 1 MB from RAM turns each glyph fault's 4-6 scattered SD seeks into one
  // glyf read. Gated per source so a small-PSRAM board takes what fits.
  static constexpr size_t kStreamPrefix = 1024 * 1024;
  const size_t prefix = len < kStreamPrefix ? len : kStreamPrefix;
  if (heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) > prefix + 256 * 1024) {
    s.bytes.resize(prefix);
    if (s.file.seek(0) && static_cast<size_t>(s.file.read(s.bytes.data(), prefix)) == prefix) {
      LOG_DBG("SDFS", "Cached %u KB TTF prefix in PSRAM", static_cast<unsigned>(prefix / 1024));
    } else {
      s.bytes.clear();  // fall back to pure streaming
    }
  }
  LOG_DBG("SDFS", "Streaming TTF %s (%u KB) from SD", path.c_str(), static_cast<unsigned>(len / 1024));
  s.size = static_cast<unsigned long>(len);
  s.present = true;
  return true;
}

// Streamed-source read: serve from the PSRAM prefix cache when the range is
// there, hit SD only for the tail (glyf outlines). A read straddling the
// boundary splits across both.
unsigned long SdCardFontSystem::prefixRead(void* ctx, const unsigned long offset, unsigned char* buffer,
                                           const unsigned long count) {
  auto* s = static_cast<TtfSource*>(ctx);
  const unsigned long cached = s->bytes.size();
  if (offset < cached) {
    const unsigned long fromCache = (offset + count <= cached) ? count : cached - offset;
    if (count == 0) return 0;  // seek probe
    memcpy(buffer, s->bytes.data() + offset, fromCache);
    if (fromCache == count) return count;
    return fromCache +
           SdCardFontRegistry::halFileRead(&s->file, offset + fromCache, buffer + fromCache, count - fromCache);
  }
  return SdCardFontRegistry::halFileRead(&s->file, offset, buffer, count);
}

void SdCardFontSystem::addTtfSources(TtfEpdFont& font) {
  for (uint8_t st = 0; st < 4; ++st) {
    TtfSource& s = ttfSources_[st];
    if (!s.present) continue;
    if (s.streamed) {
      font.addStreamSource(st, &SdCardFontSystem::prefixRead, &s, s.size);
    } else {
      font.addResidentSource(st, s.bytes.data(), static_cast<uint32_t>(s.bytes.size()));
    }
  }
}

void SdCardFontSystem::setupTtfUiFallbacks(GfxRenderer& renderer) {
  if (!ttf_ || ttfFamily_.empty()) return;

  // Small caches: UI strings (titles/rows) are short. Each UI family is a sized
  // variant of ttf_, sharing its FreeType faces without parsing font files or
  // allocating separate FT_Face objects in DRAM.
  static constexpr size_t kUiFallbackMinInternalHeap = 24 * 1024;
  for (const auto& ui : kUiFontSizes) {
    if (heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) == 0) {
      const size_t internalFree = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
      if (internalFree < kUiFallbackMinInternalHeap) {
        LOG_DBG("SDFS", "Skipping TTF UI fallback @%upt (%u KB internal free)", ui.pointSize,
                static_cast<unsigned>(internalFree / 1024));
        continue;
      }
    }
    auto f = makeUniqueNoThrow<TtfEpdFont>();
    if (!f) {
      LOG_ERR("SDFS", "OOM: TtfEpdFont for UI fallback @%upt", ui.pointSize);
      continue;  // built-in bitmap UI fonts keep covering this size
    }
    const bool ok = f->loadVariantOf(*ttf_, ui.pointSize, TTF_RENDER_PROFILES.profileFor(ttfFamily_.c_str()).twoBit,
                                     /*glyphCacheBytes=*/16 * 1024, /*maxGlyphs=*/384);
    if (!ok) continue;
    LOG_DBG("SDFS", "TTF UI fallback @%upt loaded (heap free %u)", ui.pointSize, (unsigned)ESP.getFreeHeap());
    // Distinct id from the reader-size font: a UI size can equal the reader size
    // (e.g. both 12pt), which would collide on computeTtfFontId and be dropped
    // as a duplicate. Salt the UI family name to separate the id spaces.
    const int id =
        computeTtfFontId((ttfFamily_ + "\x01ui").c_str(), ui.pointSize, ttfSourceIdentity_ ^ ttfProfileIdentity_);
    renderer.insertFont(id, f->family());
    renderer.registerTtfFont(id, f.get());
    renderer.setFallbackFont(ui.fontId, id);
    ttfUiIds_.push_back(id);
    ttfUi_.push_back(std::move(f));
  }
}

void SdCardFontSystem::loadTtfFamily(const SdCardFontFamilyInfo& family, GfxRenderer& renderer,
                                     const bool registryWasDirty, uint8_t requestedSize, const bool ui) {
  const uint8_t size = snapToNearestPointSize(VECTOR_READER_POINT_SIZES, std::size(VECTOR_READER_POINT_SIZES),
                                              requestedSize ? requestedSize : SETTINGS.fontPointSize);
  const auto profile = TTF_RENDER_PROFILES.profileFor(family.name.c_str());
  const uint32_t profileKey = profile.identity();
  const bool unchanged = !registryWasDirty && profileKey == ttfProfileIdentity_;
  if (unchanged && ttf_ && ttfFamily_ == family.name && ttfPointSize_ == size) {
    if (ui && ttfUi_.empty()) setupTtfUiFallbacks(renderer);
    return;
  }
  // Reader-face glyph-cache budget (used by both the resize fast path and the
  // full load below): the default 32 KB holds ~90 CJK glyphs, but a CJK page
  // uses 300+, so the cache flush-cycles mid-page and every page turn
  // re-rasterizes the whole page through streamed SD reads (multi-second
  // turns). The arenas are PSRAM-backed (FontPsram); 1 MB / 4096 glyphs holds
  // a whole Japanese novel's working set (~3000 unique kanji+kana at ~350 B
  // each), so the flush-everything ceiling is never hit and warm page turns
  // are pure cache hits. Without PSRAM keep the internal-DRAM-safe default.
  const bool havePsram = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) > 0;
  const size_t cacheBytes = havePsram ? size_t(profile.cacheKiB) * 1024 : 32 * 1024;
  const uint16_t maxGlyphs = havePsram ? profile.maxGlyphs : std::min<uint16_t>(profile.maxGlyphs, 768);

  // Same family, only the reader size changed (size preview): the open style
  // sources and the size-independent UI fallbacks don't need rebuilding — just
  // re-drive the reader face at the new size, reusing the already-open files
  // instead of reopening all four and rebuilding every UI fallback.
  if (unchanged && ttf_ && ttfFamily_ == family.name) {
    clearTtfVariants(renderer);  // their ids derive from the old reader size
    renderer.unregisterTtfFont(ttfFontId_);
    renderer.removeFont(ttfFontId_);
    if (ttf_->load(size, profile.twoBit, cacheBytes, maxGlyphs)) {
      ttf_->build(" ");
      ttfFontId_ = computeTtfFontId(family.name.c_str(), size, ttfSourceIdentity_ ^ profileKey);
      renderer.insertFont(ttfFontId_, ttf_->family());
      renderer.registerTtfFont(ttfFontId_, ttf_.get());
      ttfPointSize_ = size;
      return;
    }
    // Resize failed: fall through to a clean full reload.
  }

  unloadTtf(renderer);

  if (family.files.empty()) {
    LOG_ERR("SDFS", "Vector family %s has no file", family.name.c_str());
    // Preserve saved selection on transient IO/catalog failure.
    return;
  }

  // Open each style source the family ships (0=regular, 1=bold, 2=italic,
  // 3=bold-italic). A single-file family (loose .ttf, or a folder with one file)
  // supplies only regular; TtfEpdFont then derives bold/italic from the wght axis
  // or an oblique shear. Extra files upgrade those styles to the real designs.
  char path[SdCardFontFamilyInfo::PATH_BUFFER_SIZE];
  for (const auto& file : family.files) {
    const uint8_t role = file.style < 4 ? file.style : 0;
    if (ttfSources_[role].present) continue;  // registry already deduped by role
    if (!family.filePath(file, path, sizeof(path))) {
      LOG_ERR("SDFS", "Vector font path too long in family %s", family.name.c_str());
      continue;
    }
    openTtfSource(role, path);
  }
  if (!ttfSources_[0].present) {
    // Possibly a transient SD read failure: keep the user's selection so the
    // next ensureLoaded() retries; this session falls back to the built-in.
    LOG_ERR("SDFS", "Vector family %s: regular file failed to open (keeping selection)", family.name.c_str());
    freeTtfSources();
    return;
  }

  ttf_ = makeUniqueNoThrow<TtfEpdFont>();
  if (!ttf_) {
    // Transient OOM: keep the user's selection (unlike a parse failure) so the
    // next ensureLoaded() can retry once heap pressure passes.
    LOG_ERR("SDFS", "OOM: TtfEpdFont for %s", family.name.c_str());
    freeTtfSources();
    return;
  }
  ttfProfileIdentity_ = profileKey;
  addTtfSources(*ttf_);
  const bool ok = ttf_->load(size, profile.twoBit, cacheBytes, maxGlyphs);
  if (!ok) {
    // init failure is ambiguous (corrupt font vs. transient OOM inside
    // FreeType): keep the selection and retry next ensureLoaded() rather than
    // silently reverting the user to the built-in font. A genuinely broken
    // font costs one failed load per reader entry, visible in the log.
    LOG_ERR("SDFS", "FreeInkFont could not parse %s (keeping selection)", family.name.c_str());
    ttf_.reset();
    freeTtfSources();
    return;
  }
  // Seed the regular face's glyph cache; other styles + glyphs fault on demand.
  ttf_->build(" ");

  ttfFontId_ = computeTtfFontId(family.name.c_str(), size, ttfSourceIdentity_ ^ profileKey);
  renderer.insertFont(ttfFontId_, ttf_->family());
  renderer.registerTtfFont(ttfFontId_, ttf_.get());
  ttfFamily_ = family.name;
  ttfPointSize_ = size;
  LOG_DBG("SDFS", "Reader TTF face loaded (heap free %u, max block %u)", (unsigned)ESP.getFreeHeap(),
          (unsigned)ESP.getMaxAllocHeap());
  if (ui) setupTtfUiFallbacks(renderer);  // CJK/script UI fallback at the built-in UI sizes
  LOG_DBG("SDFS", "Loaded TTF font: %s @ %upt (id %d)", family.name.c_str(), size, ttfFontId_);
}

#endif  // CROSSPOINT_VECTOR_FONTS

void SdCardFontSystem::persistSettingsChange() const {
  if (settingsPersistenceCallback_) {
    settingsPersistenceCallback_(settingsPersistenceContext_);
  } else {
    SETTINGS.saveToFile();
  }
}

void SdCardFontSystem::ensureRegistry() {
  const bool dirty = registryDirty_.exchange(false, std::memory_order_acq_rel);
  if (dirty) fontReloadPending_ = true;
  if (registryLoaded_ && !dirty && !registry_.needsRefresh()) return;
  if (dirty) LOG_DBG("SDFS", "Registry dirty — re-discovering fonts");
  registry_.loadNames();
  if (registry_.lastDiscoveryFailed()) {
    LOG_ERR("SDFS", "SD font registry scan ran out of memory (free=%u maxAlloc=%u)", ESP.getFreeHeap(),
            ESP.getMaxAllocHeap());
    registryDirty_.store(true, std::memory_order_release);
    return;
  }
  registryLoaded_ = true;
}

void SdCardFontSystem::releaseRegistry() {
  if (!registryLoaded_) return;
  LOG_DBG("SDFS", "Releasing SD font catalog (%d families)", registry_.getFamilyCount());
  registry_.clear();
  registryLoaded_ = false;
}

void SdCardFontSystem::markRegistryDirtyForPath(const char* path) {
  if (!path) return;
  for (const char* root : {SdCardFontRegistry::FONTS_DIR_HIDDEN, SdCardFontRegistry::FONTS_DIR_VISIBLE}) {
    const size_t length = std::strlen(root);
    if (strncasecmp(path, root, length) == 0 && (path[length] == '/' || path[length] == '\0')) {
      markRegistryDirty();
      return;
    }
  }
}

void SdCardFontSystem::releaseLoadedFont(GfxRenderer& renderer) {
#if CROSSPOINT_VECTOR_FONTS
  unloadTtf(renderer);
#endif
  unloadCpfonts(renderer);
}
void SdCardFontSystem::releaseForNetwork(GfxRenderer& renderer) {
  releaseLoadedFont(renderer);
  releaseRegistry();
}
uint8_t SdCardFontSystem::resolveLegacySizeStep(const char* name, uint8_t step) {
  ensureRegistry();
  if (registry_.lastDiscoveryFailed()) return 0;
  const auto* family = registry_.findFamily(name ? name : "");
  if (!family) return 0;
  auto sizes = family->availableSizes();
  if (sizes.empty()) return 0;
  return sizes[std::min<size_t>(step, sizes.size() - 1)];
}
bool SdCardFontSystem::changeReaderFontSize(bool larger, FontSizeStepMode mode) {
  if (SETTINGS.sdFontFamilyName[0]) {
    ensureRegistry();
    if (registry_.lastDiscoveryFailed()) return false;
    const auto* family = registry_.findFamily(SETTINGS.sdFontFamilyName);
    if (family) {
      if (family->vector)
        return changeReaderFontSizeStep(VECTOR_READER_POINT_SIZES, std::size(VECTOR_READER_POINT_SIZES),
                                        SETTINGS.readerFontPointSize, larger, mode);
      auto sizes = family->availableSizes();
      if (!sizes.empty())
        return changeReaderFontSizeStep(sizes.data(), sizes.size(), SETTINGS.readerFontPointSize, larger, mode);
    }
  }
  return SETTINGS.changeReaderFontSize(larger, mode);
}
DictionaryFontActivation SdCardFontSystem::activateDictionaryFont(GfxRenderer& renderer, const char* name,
                                                                  uint8_t size) {
  if ((!name || !*name) && size && SETTINGS.sdFontFamilyName[0]) name = SETTINGS.sdFontFamilyName;
  if (!name || !*name) return {restoreReaderFont(renderer), false};
  ensureRegistry();
  if (registry_.lastDiscoveryFailed()) return {restoreReaderFont(renderer), false};
  if (!registry_.findFamily(name)) {
    if (SETTINGS.dictionarySdFontFamilyName[0] && strcmp(name, SETTINGS.dictionarySdFontFamilyName))
      return activateDictionaryFont(renderer, SETTINGS.dictionarySdFontFamilyName, SETTINGS.dictionaryFontPointSize);
    return {restoreReaderFont(renderer), false};
  }
  if (!size) size = SETTINGS.getSdFontTargetPointSize();
  if (auto* caches = renderer.getFontCacheManager()) caches->releaseSdFontCaches();
  if (!MemoryBudget::hasHeapForDictionarySdFont(MemoryBudget::snapshot())) releaseLoadedFont(renderer);
  if (!MemoryBudget::hasHeapForDictionarySdFont(MemoryBudget::snapshot())) return {restoreReaderFont(renderer), false};
  if (selectFamily(renderer, name, size, false, false)) {
    const int id = resolveFontId(name, size);
    return {id, id != 0};
  }
  return {restoreReaderFont(renderer), false};
}
int SdCardFontSystem::restoreReaderFont(GfxRenderer& renderer) {
  ensureLoaded(renderer);
  const int id = resolveFontId(SETTINGS.sdFontFamilyName, SETTINGS.getSdFontTargetPointSize());
  return id ? id : SETTINGS.getBuiltInReaderFontId();
}
