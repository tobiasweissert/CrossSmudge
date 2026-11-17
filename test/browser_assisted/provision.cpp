#include "HalBrowserBackend.h"
#include <map>
#include <string>
#include <cassert>
#include <cstring>
std::map<std::string,std::string> nvs;
std::string sdConfig;
bool readable=true,closed=false,wifi=true;
int failWrite=0,failRead=0;
static std::string config(char token){return "{\"v\":1,\"origin\":\"https://sticky-browser.tobiasweissert.ch\",\"token\":\""+std::string(64,token)+"\"}";}
int main(){auto a=HalBrowserBackend::access();char out[65]{};assert(!a.read(nullptr,out));sdConfig=config('a');assert(!a.import(nullptr));assert(closed);assert(a.read(nullptr,out));assert(out[0]=='a');auto old=nvs;
for(int step:{1,2}){nvs=old;sdConfig=config('b');failWrite=step;assert(a.import(nullptr));failWrite=0;assert(a.read(nullptr,out)&&out[0]=='a');}
nvs=old;failRead=1;sdConfig=config('b');assert(a.import(nullptr));failRead=0;assert(a.read(nullptr,out)&&out[0]=='a');
readable=false;assert(a.import(nullptr));readable=true;assert(a.read(nullptr,out)&&out[0]=='a');
for(auto bad:{std::string(513,' '),config('A'),config('a')+"x",std::string("{}")}){sdConfig=bad;assert(a.import(nullptr));assert(a.read(nullptr,out)&&out[0]=='a');}
unsigned char bytes[16];wifi=false;assert(!a.random(nullptr,bytes,16));wifi=true;assert(a.random(nullptr,bytes,16));assert(!memcmp(bytes,"\0\1\2\3\4\5\6\7\10\11\12\13\14\15\16\17",16));
}
