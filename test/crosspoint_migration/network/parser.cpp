#include <ReleaseJsonParser.h>
#include <StreamingJsonParser.h>
#include <cassert>
#include <cstring>
#include <string>
#include <iostream>
static bool match(const char* name) { return std::strcmp(name, "crosscub-sticky.bin") == 0; }
int main() {
  const std::string digest(64, 'a');
  const std::string manifest = "{\"tag_name\":\"1.6.5.6\",\"assets\":[{\"name\":\"crosscub-sticky.bin\",\"size\":123,\"digest\":\"sha256:" + digest + "\",\"browser_download_url\":\"https://example.org/\\u00e9\\ud83d\\ude00.bin\"},{\"name\":\"other.bin\",\"size\":456,\"browser_download_url\":\"https://example.org/other.bin\"}]}";
  for (size_t chunk : {size_t(1), size_t(7), manifest.size()}) {
    ReleaseJsonParser parser(match);
    for (size_t i=0;i<manifest.size();i+=chunk) parser.feed(manifest.data()+i, std::min(chunk,manifest.size()-i));
    assert(parser.foundTag() && parser.foundFirmware());
    assert(std::string(parser.getTagName())=="1.6.5.6");
    assert(std::string(parser.getFirmwareSha256())==digest);
    assert(std::string(parser.getFirmwareUrl())=="https://example.org/\xc3\xa9\xf0\x9f\x98\x80.bin");
    assert(parser.getFirmwareSize()==123);
    parser.reset(); parser.setFirmwareAssetName("other.bin"); parser.feed(manifest.data(),manifest.size());
    assert(parser.foundFirmware() && parser.getFirmwareSize()==456);
    parser.reset(); parser.setFirmwareAssetName(""); parser.feed(manifest.data(),manifest.size());
    assert(!parser.foundFirmware());
  }
  ReleaseJsonParser incomplete(match);
  incomplete.feed(manifest.data(),manifest.size()-1);
  assert(!incomplete.foundFirmware());
  incomplete.feed(manifest.data()+manifest.size()-1,1); assert(incomplete.foundFirmware());
  ReleaseJsonParser nul(match);
  const std::string injected = "{\"assets\":[{\"name\":\"crosscub-sticky.bin\\u0000junk\",\"size\":1}]}";
  nul.feed(injected.data(),injected.size()); assert(!nul.foundFirmware());
  ReleaseJsonParser empty;
  empty.setFirmwareAssetName("");
  const std::string nameless="{\"assets\":[{\"size\":123}]}";
  empty.feed(nameless.data(),nameless.size()); assert(!empty.foundFirmware());
  ReleaseJsonParser invalidSize(match);
  const std::string fractional="{\"assets\":[{\"name\":\"crosscub-sticky.bin\",\"size\":65536.5}]}";
  invalidSize.feed(fractional.data(),fractional.size()); assert(!invalidSize.foundFirmware());
  JsonCallbacks cb{}; StreamingJsonParser sax(cb);
  const std::string deep(StreamingJsonParser::MAX_NESTING+1,'[');
  sax.feed(deep.data(),deep.size()); assert(sax.hasError());
  sax.reset(); sax.feed("{\"x\":truX}",11); assert(sax.hasError());
  std::cout << "release matcher/exact override/digest/unicode/chunking and SDK depth/error PASS\n";
}
