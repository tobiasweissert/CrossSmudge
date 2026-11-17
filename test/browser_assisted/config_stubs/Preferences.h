#pragma once
#include <cstring>
#include <map>
#include <string>
#include <cstdint>
extern std::map<std::string,std::string> nvs;
extern int failWrite,failRead;
class Preferences {
 public:
 bool begin(const char* space,bool=false){return !strcmp(space,"browser-backend");}
 void end(){}
 bool isKey(const char* k){return nvs.count(k);}
 uint8_t getUChar(const char* k,uint8_t d){auto it=nvs.find(k);return it==nvs.end()?d:uint8_t(it->second[0]);}
 size_t putUChar(const char* k,uint8_t v){if(failWrite>0 && --failWrite==0)return 0;nvs[k]=std::string(1,char(v));return 1;}
 size_t getBytes(const char* k,void* out,size_t n){if(failRead>0 && --failRead==0)return 0;auto it=nvs.find(k);if(it==nvs.end() || it->second.size()!=n)return 0;memcpy(out,it->second.data(),n);return n;}
 size_t putBytes(const char* k,const void* in,size_t n){if(failWrite>0 && --failWrite==0)return 0;nvs[k]=std::string((const char*)in,n);return n;}
};
