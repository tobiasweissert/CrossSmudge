#pragma once
#include <HalStorage.h>

#include <cassert>
// Byte snapshots preserve old assertion syntax while the qualified HAL owns
// real shared node identity, independent cursors and directory traversal.
class NestedDisk {
  using Bytes = std::map<std::string, std::string>;
  bool live = true;
  Bytes saved;
  Bytes snapshot() const {
    if (!live) return saved;
    Bytes result;
    for (const auto& [path, node] : fake::files)
      if (!node->directory) result[path] = std::string(node->bytes.begin(), node->bytes.end());
    return result;
  }
  void put(const std::string& p, const std::string& value) {
    assert(live);
    fake::add(p, value);
  }

 public:
  NestedDisk() = default;
  NestedDisk(const NestedDisk& other) : live(false), saved(other.snapshot()) {}
  NestedDisk& operator=(const NestedDisk& other) {
    assert(live);
    auto values = other.snapshot();
    fake::reset();
    for (const auto& [p, v] : values) put(p, v);
    return *this;
  }
  NestedDisk& operator=(std::initializer_list<Bytes::value_type> values) {
    assert(live);
    fake::reset();
    for (const auto& [p, v] : values) put(p, v);
    return *this;
  }
  std::string at(const std::string& p) const { return snapshot().at(p); }
  size_t count(const std::string& p) const { return snapshot().count(p); }
  size_t erase(const std::string& p) {
    assert(live);
    return fake::files.erase(p);
  }
  bool operator==(const NestedDisk& other) const { return snapshot() == other.snapshot(); }
  bool equalsExcept(const NestedDisk& other, const std::string& path) const {
    auto left = snapshot(), right = other.snapshot();
    left.erase(path);
    right.erase(path);
    return left == right;
  }
  struct Reference {
    NestedDisk& owner;
    std::string path;
    operator std::string() const { return owner.at(path); }
    Reference& operator=(const std::string& v) {
      owner.put(path, v);
      return *this;
    }
    Reference& operator=(const Reference& v) { return *this = std::string(v); }
    Reference& operator+=(const std::string& v) { return *this = owner.at(path) + v; }
    void replace(size_t p, size_t n, size_t count, char c) {
      auto v = owner.at(path);
      v.replace(p, n, count, c);
      owner.put(path, v);
    }
    uint8_t& operator[](size_t p) { return fake::files.at(path)->bytes.at(p); }
  };
  Reference operator[](const std::string& p) {
    if (!count(p)) put(p, "");
    return {*this, p};
  }
};
inline NestedDisk disk;
