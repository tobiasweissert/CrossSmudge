#include <Epub.h>
#include <Epub/converters/ImageDecoderFactory.h>
#include <Epub/converters/ImageToFramebufferDecoder.h>
#include <Epub/hyphenation/Hyphenator.h>
bool isExplicitHyphen(uint32_t cp) { return cp == '-' || cp == 0x2010; }
bool isSoftHyphen(uint32_t cp) { return cp == 0xad; }
std::vector<Hyphenator::BreakInfo> Hyphenator::breakOffsets(const std::string&, bool) { return {}; }
bool ImageDecoderFactory::isFormatSupported(const std::string& path) { return path.ends_with(".gif"); }
ImageToFramebufferDecoder* ImageDecoderFactory::getDecoder(const std::string&) { return nullptr; }

Epub::Epub(std::string path, const std::string& cacheDir) : filepath(std::move(path)), cachePath(cacheDir) {}
const std::string& Epub::getCachePath() const { return cachePath; }
const std::string& Epub::getLanguage() const {
  static const std::string value = "en";
  return value;
}
BookMetadataCache::SpineEntry Epub::getSpineItem(int) const { return {}; }
BookMetadataCache::TocEntry Epub::getTocItem(int) const { return {}; }
int Epub::getTocItemsCount() const { return 0; }
int Epub::getTocIndexForSpineIndex(int) const { return -1; }
bool Epub::readItemContentsToStream(const std::string&, Print&, size_t, bool) const { return false; }
bool Epub::getOptimizerImageDimensions(const std::string&, uint16_t&, uint16_t&) const { return false; }
void Hyphenator::setPreferredLanguage(const std::string&) {}
