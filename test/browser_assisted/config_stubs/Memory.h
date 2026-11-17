#pragma once
#include <memory>
#include <new>
template<class T> std::unique_ptr<T> makeUniqueNoThrow(size_t n){return std::unique_ptr<T>(new(std::nothrow) char[n]);}
