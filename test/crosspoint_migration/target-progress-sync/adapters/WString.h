#pragma once
#include <string>
// Arduino String accepts C strings, not an implicit std::string conversion.
class String {
 public:
  String() = default;
  String(const char* s) : value_(s ? s : "") {}
  const char* c_str() const { return value_.c_str(); }
  size_t length() const { return value_.length(); }

 private:
  std::string value_;
};
