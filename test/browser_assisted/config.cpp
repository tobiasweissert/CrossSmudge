#include "BackendConfig.h"
#include <cassert>
#include <string>
#include <cstring>
using namespace ink::http;
int main(){char out[65]{};std::string good="{\"v\":1,\"origin\":\""+std::string(BackendOrigin)+"\",\"token\":\""+std::string(64,'a')+"\"}";assert(parseBackendConfig(good,out));assert(!strcmp(out,std::string(64,'a').c_str()));for(auto bad:{good+"x",good.substr(0,good.size()-1)+",\"v\":1}",std::string(513,' '),std::string("{\"v\":1}"),std::string("{\"v\":1,\"origin\":\"https://evil.org\",\"token\":\"")+std::string(64,'a')+"\"}"}){assert(!parseBackendConfig(bad,out));assert(!strcmp(out,std::string(64,'a').c_str()));}}
