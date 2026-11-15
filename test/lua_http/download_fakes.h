#pragma once
#include "DownloadTransfer.h"
#include <map>
#include <string>
#include <algorithm>
#include <cstring>
using namespace ink::http;
struct Files final : DownloadFiles {
  std::map<std::string,std::string> data;
  std::string name;
  size_t pos=0,largest=0;
  bool shortWrite=false, syncFail=false, closeFail=false, renameFail=false, removed=false;
  std::atomic<bool>* cancelOnSync=nullptr;
  uint32_t* clockOnSync=nullptr;
  bool metadataFail=false;
  bool prepare() override {return !removed;}
  int exists(const char* p) override {return metadataFail || removed?-1:int(data.count(p));}
  bool open(const char* p,bool create) override {
    if(removed || (create && data.count(p)) || (!create && !data.count(p)))return false;
    name=p;pos=0;if(create)data[p]="";return true;
  }
  size_t size() override {return data.at(name).size();}
  bool seek(size_t n) override {pos=n;return !removed;}
  int read(char* out,size_t n) override {
    if(removed)return -1;
    n=std::min(n,data.at(name).size()-pos);memcpy(out,data.at(name).data()+pos,n);pos+=n;return n;
  }
  size_t write(const char* p,size_t n) override {
    largest=std::max(n,largest);if(removed)return 0;
    if(shortWrite && n)n--;
    data.at(name).append(p,n);return n;
  }
  bool sync() override {if(clockOnSync)*clockOnSync+=30001;if(cancelOnSync)cancelOnSync->store(true);return !syncFail && !removed;}
  bool close() override {name.clear();return !closeFail && !removed;}
  bool remove(const char* p) override {if(removed)return false;data.erase(p);return true;}
  bool promote(const char* p) override {
    if(renameFail || removed || data.count(p))return false;
    data[p]=std::move(data.at("part"));data.erase("part");return true;
  }
};
struct Wire {
  std::string response,sent;
  size_t pos=0,stop=SIZE_MAX,reads=0;
  uint32_t now=0;
  bool trusted=true,closed=false;
  std::atomic<bool>* cancellation=nullptr;
  Files* removal=nullptr;
  size_t trigger=SIZE_MAX;
  Io io() {return {this,
    [](void* p,const char*,uint32_t,const std::atomic<bool>&){return static_cast<Wire*>(p)->trusted;},
    [](void* p,char* b,size_t n)->int {
      auto& w=*static_cast<Wire*>(p);
      if(w.pos>=w.trigger){if(w.cancellation)w.cancellation->store(true);if(w.removal)w.removal->removed=true;}
      if(w.pos>=w.stop || w.pos==w.response.size())return 0;
      n=std::min({n,w.response.size()-w.pos,w.stop-w.pos,size_t(317)});
      memcpy(b,w.response.data()+w.pos,n);w.pos+=n;w.reads++;return n;
    },
    [](void* p,const char* b,size_t n)->int{static_cast<Wire*>(p)->sent.append(b,n);return n;},
    [](void* p){static_cast<Wire*>(p)->closed=true;},
    [](void* p){return static_cast<Wire*>(p)->now++;}};}
};
