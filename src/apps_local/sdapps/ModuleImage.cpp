#include "ModuleImage.h"
#if defined(FREEINK_DEVICE_STICKY)
#include "ImageBudget.h"
#include <CrossPlayElf.h>
#include <HalMemory.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <SHA2Builder.h>
#include <cstdio>
#include <cstring>
namespace sdapps {
struct ModuleImage::Impl {
  esp_elf_t image{};
  CpModuleV1 api{};
  bool initialized = false;
  ~Impl() { if (initialized) esp_elf_deinit(&image); }
};
ModuleImage::ModuleImage() = default;
ModuleImage::~ModuleImage() = default;
void ModuleImage::unload() { impl.reset(); }
const CpModuleV1& ModuleImage::module() const { return impl->api; }
bool ModuleImage::load(const char* root, const Package& package) {
  unload();
  if (!root || (strcmp(root, "/apps") && strcmp(root, "/games")) || !validPackageId(package.id)) return false;
  char path[96];
  const int n = snprintf(path, sizeof(path), "%s/%s/app.elf", root, package.id);
  if (n < 0 || static_cast<size_t>(n) >= sizeof(path)) return false;
  HalFile file;
  if (!Storage.openFileForRead("SDAPP", path, file) || file.isDirectory()) return false;
  const uint64_t length = file.fileSize64();
  if (length < 52 || length > MAX_IMAGE_BYTES) {
    LOG_ERR("SDAPP", "ELF size outside budget");
    return false;
  }
  // Immutable PSRAM snapshot, <=1 MiB. Streaming would allow the SD image to
  // change after validation. Stack/internal RAM cannot hold this working set.
  auto bytes = HalMemory::allocatePsram(length);
  if (!bytes) { LOG_ERR("SDAPP", "OOM: ELF snapshot %u bytes", unsigned(length)); return false; }
  if (file.read(bytes.get(), length) != static_cast<int>(length) || !imageWithinBudget(bytes.get(), length)) {
    LOG_ERR("SDAPP", "ELF read or resource admission failed");
    return false;
  }
  // SHA2Builder exceeds the stack budget; its cold-path lifetime ends here.
  auto hash = makeUniqueNoThrow<SHA256Builder>();
  if (!hash) { LOG_ERR("SDAPP", "OOM: SHA256 context"); return false; }
  uint8_t digest[32];
  hash->begin(); hash->add(bytes.get(), length); hash->calculate(); hash->getBytes(digest);
  if (memcmp(digest, package.digest, sizeof(digest))) {
    LOG_ERR("SDAPP", "ELF digest mismatch");
    return false;
  }
  hash.reset();
  // One loader context per open app, retained until all host jobs drain.
  auto loaded = makeUniqueNoThrow<Impl>();
  if (!loaded) { LOG_ERR("SDAPP", "OOM: loader context"); return false; }
  if (esp_elf_init(&loaded->image) != 0) { LOG_ERR("SDAPP", "ELF initialization failed"); return false; }
  loaded->initialized = true;
  if (esp_elf_relocate(&loaded->image, bytes.get(), length) != 0) {
    LOG_ERR("SDAPP", "ELF validation/relocation failed");
    return false;
  }
  bytes.reset();
  CpBootstrap bootstrap{sizeof(CpBootstrap), package.abi, {}};
  char* argv[] = {reinterpret_cast<char*>(&bootstrap)};
  if (esp_elf_request(&loaded->image, 0, 1, argv) != 0 || bootstrap.module.size != sizeof(CpModuleV1) ||
      bootstrap.module.version != package.abi || !bootstrap.module.start || !bootstrap.module.event ||
      !bootstrap.module.stop) {
    LOG_ERR("SDAPP", "Module ABI negotiation failed");
    return false;
  }
  const uintptr_t begin = esp_elf_runtime_base(&loaded->image);
  const uintptr_t span = loaded->image.sec[ELF_SEC_TEXT].size;
  const auto inText = [begin, span](uintptr_t address) { return address >= begin && address - begin < span; };
  if (!inText(reinterpret_cast<uintptr_t>(bootstrap.module.start)) ||
      !inText(reinterpret_cast<uintptr_t>(bootstrap.module.event)) ||
      !inText(reinterpret_cast<uintptr_t>(bootstrap.module.stop))) {
    LOG_ERR("SDAPP", "Module callback outside executable text");
    return false;
  }
  loaded->api = bootstrap.module;
  impl = std::move(loaded);
  return true;
}
}
#endif
