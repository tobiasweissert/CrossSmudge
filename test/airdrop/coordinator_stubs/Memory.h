#pragma once
#include <memory>
#include <new>
#include <utility>
namespace coordinator {template<class T>inline bool failAllocation=false;}
template<class T,class...Args>std::unique_ptr<T> makeUniqueNoThrow(Args&&...args){
 if(coordinator::failAllocation<T>)return {};
 return std::unique_ptr<T>(new(std::nothrow)T(std::forward<Args>(args)...));
}
