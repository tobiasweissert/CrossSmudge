#pragma once
#include <Arduino.h>

#include <map>
#include <set>
#include <string>
class HalStorage {
 public:
  std::map<std::string, String> files;
  std::set<std::string> unreadable;
  std::string failWrite, failRename;
  bool ambiguousRename = false;
  int writes = 0, mutations = 0;
  void reset() {
    files.clear();
    unreadable.clear();
    failWrite.clear();
    failRename.clear();
    ambiguousRename = false;
    writes = mutations = 0;
  }
  bool mkdir(const char*, bool = true) { return true; }
  bool exists(const char* p) const { return files.contains(p); }
  bool remove(const char* p) {
    ++mutations;
    return files.erase(p) > 0;
  }
  bool rename(const char* a, const char* b) {
    ++mutations;
    if (failRename == a) {
      if (ambiguousRename && files.contains(a)) files[b] = files[a];
      return false;
    }
    if (!files.contains(a) || files.contains(b)) return false;
    files[b] = files[a];
    files.erase(a);
    return true;
  }
  bool writeFile(const char* p, const String& s) {
    ++writes;
    ++mutations;
    if (failWrite == p) return false;
    files[p] = s;
    return true;
  }
  String readFile(const char* p) const {
    if (unreadable.contains(p) || !files.contains(p)) return {};
    return files.at(p);
  }
};
inline HalStorage Storage;
