#include <Epub.h>
#include <Epub/converters/ImageDecoderFactory.h>
#include <Epub/converters/ImageToFramebufferDecoder.h>
#include <Epub/hyphenation/Hyphenator.h>
bool isExplicitHyphen(uint32_t cp) { return cp == '-' || cp == 0x2010; }
bool isSoftHyphen(uint32_t cp) { return cp == 0xad; }
std::vector<Hyphenator::BreakInfo> Hyphenator::breakOffsets(const std::string&, bool) { return {}; }
bool ImageDecoderFactory::isFormatSupported(const std::string& path) {
 return Epub::enableGifProbeForTest && path.ends_with(".gif");
}
ImageToFramebufferDecoder* ImageDecoderFactory::getDecoder(const std::string&) { return nullptr; }
