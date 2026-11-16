#pragma once
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using String = std::string;
inline void yield() {}
namespace appfs {
struct Node {
  std::string data;
  bool directory = false;
  bool openFailure = false;
  bool shortRead = false;
  bool closeFailure = false;
  bool iterationFailure = false;
  bool allocationFailure = false;
  bool nameFailure = false;
  unsigned closeCalls = 0, closeFailureAt = 0;
};
inline std::unordered_map<std::string, Node> files{{"/", {"", true}}};
inline int opened = 0;
inline unsigned existsCalls = 0;
inline std::vector<std::string> openPaths;
inline void reset() {
  assert(opened == 0);
  files.clear(); files["/"] = {"", true}; existsCalls = 0; openPaths.clear();
}
inline void directory(const std::string& path) {
  for (size_t i = 1; i < path.size(); ++i) if (path[i] == '/') files.try_emplace(path.substr(0, i), Node{"", true});
  files.try_emplace(path, Node{"", true});
}
inline bool accessible(const std::string& path) {
  const auto root = files.find("/");
  if (root == files.end() || !root->second.directory || root->second.openFailure) return false;
  for (size_t i = 1; i < path.size(); ++i) if (path[i] == '/') {
    const auto parent = files.find(path.substr(0, i));
    if (parent == files.end() || !parent->second.directory || parent->second.openFailure) return false;
  }
  return true;
}
}  // namespace appfs

class HalFile {
  appfs::Node* node_ = nullptr;
  std::string path_;
  size_t offset_ = 0, next_ = 0;
  bool allocationFailed_ = false, iterationFailed_ = false;
  std::vector<std::string> children_;

 public:
  HalFile() = default;
  HalFile(appfs::Node* node, const std::string& path) : node_(node), path_(path) {
    if (!node_) return;
    ++appfs::opened;
    if (node_->directory) {
      const std::string prefix = path_ == "/" ? "/" : path_ + "/";
      for (const auto& [name, ignored] : appfs::files) {
        (void)ignored;
        if (name.size() > prefix.size() && name.compare(0, prefix.size(), prefix) == 0 &&
            name.find('/', prefix.size()) == std::string::npos) children_.push_back(name);
      }
      std::sort(children_.begin(), children_.end());
    }
  }
  HalFile(const HalFile&) = delete;
  HalFile& operator=(const HalFile&) = delete;
  HalFile(HalFile&& other) : node_(other.node_), path_(std::move(other.path_)), offset_(other.offset_),
                            next_(other.next_), allocationFailed_(other.allocationFailed_),
                            iterationFailed_(other.iterationFailed_), children_(std::move(other.children_)) {
    other.node_ = nullptr;
  }
  HalFile& operator=(HalFile&& other) {
    if (this != &other) {
      close(); node_ = other.node_; path_ = std::move(other.path_); offset_ = other.offset_; next_ = other.next_;
      allocationFailed_ = other.allocationFailed_; iterationFailed_ = other.iterationFailed_;
      children_ = std::move(other.children_); other.node_ = nullptr;
    }
    return *this;
  }
  ~HalFile() { close(); }
  explicit operator bool() const { return node_ != nullptr; }
  bool isDirectory() const { return node_ && node_->directory; }
  size_t size() const { return node_ ? node_->data.size() : 0; }
  uint64_t fileSize64() const { return size(); }
  size_t read(void* output, size_t count) {
    if (!node_ || node_->directory) return 0;
    count = std::min(count, node_->data.size() - offset_);
    if (node_->shortRead && count) --count;
    std::memcpy(output, node_->data.data() + offset_, count); offset_ += count; return count;
  }
  bool close() {
    if (!node_) return true;
    ++node_->closeCalls;
    const bool ok = !node_->closeFailure && (!node_->closeFailureAt || node_->closeCalls != node_->closeFailureAt);
    --appfs::opened; node_ = nullptr; return ok;
  }
  bool allocationFailed() const { return allocationFailed_; }
  bool iterationFailed() const { return iterationFailed_; }
  HalFile openNextFile() {
    allocationFailed_ = iterationFailed_ = false;
    if (!node_ || !node_->directory) return {};
    if (node_->allocationFailure) { allocationFailed_ = true; return {}; }
    if (node_->iterationFailure) { iterationFailed_ = true; return {}; }
    if (next_ == children_.size()) return {};
    const std::string& path = children_[next_++];
    const auto found = appfs::files.find(path);
    if (found == appfs::files.end() || found->second.openFailure) { iterationFailed_ = true; return {}; }
    return HalFile(&found->second, path);
  }
  size_t getName(char* output, size_t capacity) {
    if (!capacity) return 0;
    output[0] = 0;
    if (!node_ || node_->nameFailure) return 0;
    const std::string name = path_.substr(path_.rfind('/') + 1);
    if (name.size() + 1 > capacity) return 0;
    std::memcpy(output, name.c_str(), name.size() + 1); return name.size();
  }
};
struct HostStorage {
  bool exists(const char* path) {
    ++appfs::existsCalls;
    const auto found = appfs::files.find(path);
    return appfs::accessible(path) && found != appfs::files.end() && !found->second.openFailure;
  }
  HalFile open(const char* path) {
    appfs::openPaths.emplace_back(path);
    const auto found = appfs::files.find(path);
    return !appfs::accessible(path) || found == appfs::files.end() || found->second.openFailure ?
      HalFile{} : HalFile(&found->second, path);
  }
  bool openFileForRead(const char*, const char* path, HalFile& output) {
    auto file = open(path); if (!file) return false; output = std::move(file); return true;
  }
  String readFile(const char* path) {
    const auto found = appfs::files.find(path); return found == appfs::files.end() ? String{} : found->second.data;
  }
};
inline HostStorage hostStorage;
// Match the real HAL's macro, so enum tokens named Storage fail compilation.
#define Storage hostStorage
