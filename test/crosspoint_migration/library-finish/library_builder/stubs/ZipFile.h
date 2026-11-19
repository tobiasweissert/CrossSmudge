#pragma once
#include <map>

#include "Epub.h"
#include "Print.h"
inline std::map<std::string, std::map<std::string, std::string>> zipEntries;
class ZipFile {
  std::string path;
  static std::string escape(const std::string& s) {
    std::string r;
    for (char c : s) {
      if (c == '&')
        r += "&amp;";
      else if (c == '<')
        r += "&lt;";
      else if (c == '\"')
        r += "&quot;";
      else
        r += c;
    }
    return r;
  }

 public:
  explicit ZipFile(const std::string& p) : path(p) {}
  bool open() {
    ++fake::parses;
    metadataCacheUse.push_back(false);
    return zipEntries.count(path) || bookMetadata[path].success;
  }
  bool close() { return true; }
  bool readFileToStream(const char* file, Print& out, size_t chunk, bool early) {
    std::string xml;
    if (zipEntries.count(path)) {
      auto it = zipEntries[path].find(file);
      if (it == zipEntries[path].end()) return false;
      xml = it->second;
    } else if (std::string(file) == "META-INF/container.xml")
      xml = "<container><rootfiles><rootfile full-path=\"content.opf\"/></rootfiles></container>";
    else {
      const auto& m = bookMetadata[path];
      xml = "<package xmlns:dc=\"urn:dc\"><metadata><dc:title>" + escape(m.title) + "</dc:title><dc:creator>" +
            escape(m.author) + "</dc:creator><dc:subject>" + escape(m.genre) +
            "</dc:subject><meta name=\"calibre:series\" content=\"" + escape(m.series) +
            "\"/><meta name=\"calibre:series_index\" content=\"" + escape(m.seriesIndex) + "\"/></metadata><manifest>";
    }
    for (size_t i = 0; i < xml.size(); i += chunk) {
      auto n = std::min(chunk, xml.size() - i);
      if (out.write(reinterpret_cast<const uint8_t*>(xml.data() + i), n) != n) return early;
    }
    return true;
  }
};
