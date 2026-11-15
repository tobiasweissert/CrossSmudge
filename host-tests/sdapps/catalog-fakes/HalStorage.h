#pragma once
#include <filesystem>
#include <fstream>
#include <cstring>
#include <vector>
class HalFile {
 std::filesystem::path path;
 std::vector<std::filesystem::path> children;
 size_t next=0;
 public:
 HalFile()=default;
 explicit HalFile(std::filesystem::path p):path(std::move(p)) {
  if(isDirectory())for(const auto& item:std::filesystem::directory_iterator(path))children.push_back(item.path());
 }
 explicit operator bool() const { return !path.empty() && std::filesystem::exists(path); }
 bool isDirectory() const {return !path.empty() && std::filesystem::is_directory(path);}
 uint64_t fileSize64(){return std::filesystem::file_size(path);}
 int read(void* out,size_t size){std::ifstream f(path,std::ios::binary);f.read(static_cast<char*>(out),size);return f.gcount();}
 size_t getName(char* out,size_t cap){auto name=path.filename().string();size_t n=std::min(cap-1,name.size());memcpy(out,name.data(),n);out[n]=0;return n;}
 HalFile openNextFile(){return next<children.size()?HalFile(children[next++]):HalFile{};}
};
struct FakeStorage {
 std::filesystem::path root;
 bool exists(const char* p){return std::filesystem::exists(root/(p+1));}
 HalFile open(const char* p){return HalFile(root/(p+1));}
 bool openFileForRead(const char*,const char* p,HalFile& out){out=open(p);return bool(out);}
};
inline FakeStorage Storage;
