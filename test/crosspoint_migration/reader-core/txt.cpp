#include <HalStorage.h>
#include <Txt.h>

#include <cassert>
#include <iostream>
struct Sink : Print {
  std::string text;
  size_t write(uint8_t b) override {
    text += char(b);
    return 1;
  }
  size_t write(const uint8_t* p, size_t n) override {
    text.append(reinterpret_cast<const char*>(p), n);
    return n;
  }
};
int main() {
  Storage.put("/old.txt", {'o', 'l', 'd', '\n', 'b', 'o', 'o', 'k'});
  Txt legacy("/old.txt", "/.crosspoint");
  assert(legacy.load());
  assert(legacy.getFileSize() == 8);
  uint8_t b[4]{};
  assert(legacy.readContent(b, 4, 4));
  assert(std::string(reinterpret_cast<char*>(b), 4) == "book");
  const auto cache = legacy.getCachePath();
  assert(cache == "/.crosspoint/txt_" + std::to_string(std::hash<std::string>{}("/old.txt")));
  Storage.put(cache + "/progress", {7, 8, 9});
  Storage.put(cache + "/bookmarks", {3, 4});
  Storage.put(cache + "/reading_stats_off", {1});
  Storage.put(cache + "/cover.bmp", {0});
  Storage.put(cache + "/html", {});
  Storage.put(cache + "/html/0.html", {0});
  assert(legacy.clearCache());
  assert(Storage.bytes(cache + "/progress") == std::vector<uint8_t>({7, 8, 9}));
  assert(Storage.exists((cache + "/bookmarks").c_str()));
  assert(Storage.exists((cache + "/reading_stats_off").c_str()));
  assert(!Storage.exists((cache + "/cover.bmp").c_str()));
  assert(!Storage.exists((cache + "/html/0.html").c_str()));
  assert(Txt::isTxtOrMd("/new.md"));
  assert(Txt::isTxtOrMd("/old.txt"));
  assert(!Txt::isTxtOrMd("/book.epub"));
  Storage.put("/new.md", {'#', ' ', 'N', 'e', 'w', '\n', '<', '&', '>'});
  Sink html;
  assert(Txt::streamTxtToHtml("/new.md", html));
  assert(html.text.find("MD_CACHE_VERSION") != std::string::npos);
  assert(html.text.find("&lt;&amp;&gt;") != std::string::npos);
  std::cout << "PASS production TXT dual facade/raw byte offsets/cache identity/durable cache-clear sentinels and "
               "modern Markdown HTML stream; metadata/FS naming are explicit doubles\n";
}
