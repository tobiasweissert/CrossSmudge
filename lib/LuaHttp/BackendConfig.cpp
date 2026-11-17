#include "BackendConfig.h"
#include <cstring>
namespace ink::http {
bool parseBackendConfig(std::string_view s,char (&token)[65]) {
 if(s.empty() || s.size()>512)return false;
 size_t p=0;unsigned seen=0;char candidate[65]{};
 struct Wipe {char* p;~Wipe(){eraseBackendSecret(p,65);}} wipe{candidate};
 auto ws=[&](){while(p<s.size() && (s[p]==' '||s[p]=='\r'||s[p]=='\n'||s[p]=='\t'))++p;};
 auto take=[&](char c){ws();if(p==s.size()||s[p]!=c)return false;++p;return true;};
 auto str=[&](std::string_view& v){if(!take('"'))return false;size_t begin=p;while(p<s.size()&&s[p]!='"'){unsigned char c=s[p++];if(c<32||c>=127||c=='\\')return false;}if(p==s.size())return false;v=s.substr(begin,p-begin);++p;return true;};
 if(!take('{'))return false;
 for(int i=0;i<3;++i){
  std::string_view key,value;if(i && !take(','))return false;if(!str(key)||!take(':'))return false;
  unsigned bit=key=="v"?1:key=="origin"?2:key=="token"?4:0;if(!bit||(seen&bit))return false;seen|=bit;
  if(bit==1){ws();if(p==s.size()||s[p++]!='1')return false;}
  else {if(!str(value))return false;if(bit==2 && value!=BackendOrigin)return false;if(bit==4){if(!backendToken(value))return false;memcpy(candidate,value.data(),64);}}
 }
 if(!take('}'))return false;ws();if(p!=s.size()||seen!=7)return false;
 memcpy(token,candidate,sizeof(token));return true;
}
}
