#pragma once
#include <FindMyCore.h>
#include <cstring>
namespace crosscub::findmy {
constexpr size_t MaxImportBytes=512;
using StateBytes=std::array<uint8_t,128>;
inline uint32_t checksum(const uint8_t* p,size_t n) {
 uint32_t c=0xffffffff;
 while(n--){c^=*p++;for(int i=0;i<8;++i)c=(c>>1)^((0-(c&1))&0xedb88320);}
 return ~c;
}
inline void encodeState(const ::findmy::Checkpoint& cp,StateBytes& b) {
 size_t pos=0;
 auto number=[&](uint64_t n,size_t len){while(len--){b[pos++]=n&255;n>>=8;}};
 number(1,4);
 for(auto v:cp.identity)b[pos++]=v;
 for(auto v:cp.publicKey)b[pos++]=v;
 for(auto v:cp.seed)b[pos++]=v;
 for(auto v:{cp.initialIndex,cp.epochSeconds,cp.index,cp.lastWallSeconds})number(v,8);
 number(checksum(b.data(),pos),4);
}
inline bool decodeState(const StateBytes& b,::findmy::Checkpoint& cp) {
 size_t pos=0;
 auto number=[&](size_t len){uint64_t n=0;for(size_t i=0;i<len;++i)n|=uint64_t(b[pos++])<<(i*8);return n;};
 if(number(4)!=1)return false;
 cp.version=1;
 for(auto& v:cp.identity)v=b[pos++];
 for(auto& v:cp.publicKey)v=b[pos++];
 for(auto& v:cp.seed)v=b[pos++];
 cp.initialIndex=number(8);cp.epochSeconds=number(8);cp.index=number(8);cp.lastWallSeconds=number(8);
 const auto sum=checksum(b.data(),pos);return number(4)==sum;
}
inline bool parseImport(const char* data,size_t n,::findmy::Provision& out) {
 if(!data || n==0 || n>MaxImportBytes)return false;
 size_t pos=0;
 auto ws=[&](){while(pos<n&&(data[pos]==' '||data[pos]=='\n'||data[pos]=='\r'||data[pos]=='\t'))++pos;};
 auto take=[&](char c){ws();return pos<n&&data[pos++]==c;};
 auto quoted=[&](char* dst,size_t cap){ws();if(pos>=n||data[pos++]!='"')return false;size_t i=0;while(pos<n&&data[pos]!='"'){const auto c=data[pos++];if(c<' '||c=='\\'||i+1>=cap)return false;dst[i++]=c;}if(pos>=n)return false;++pos;dst[i]=0;return true;};
 if(!take('{'))return false;
 struct Secrets {
  ::findmy::MasterKey master{};::findmy::Seed seed{};char value[80]{};
  ~Secrets(){::findmy::secureErase(master.data(),master.size());::findmy::secureErase(seed.data(),seed.size());::findmy::secureErase(value,sizeof value);}
 } secrets;
 auto& master=secrets.master;auto& seed=secrets.seed;auto& value=secrets.value;
 uint64_t epoch=0;unsigned seen=0;
 for(unsigned field=0;field<4;++field){
  char key[32]{};
  if(!quoted(key,sizeof key)||!take(':')||!quoted(value,sizeof value))return false;
  unsigned bit=0;
  if(!std::strcmp(key,"schema")){bit=1;if(std::strcmp(value,"crosscub-findmy-v1"))return false;}
  else if(!std::strcmp(key,"master_private_key_hex")||!std::strcmp(key,"primary_ratchet_seed_hex")){
   bool m=key[0]=='m';bit=m?2:4;const size_t bytes=m?28:32;
   if(std::strlen(value)!=bytes*2)return false;
   for(size_t i=0;i<bytes;++i){unsigned v=0;for(size_t j=0;j<2;++j){char c=value[i*2+j];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;v=v*16+(c<='9'?c-'0':c-'a'+10);}if(m)master[i]=v;else seed[i]=v;}
  }else if(!std::strcmp(key,"paired_at")){
   bit=8;if(std::strlen(value)!=20)return false;
   for(size_t i=0;i<20;++i){char c=value[i];char sep=i==4||i==7?'-':i==10?'T':i==13||i==16?':':i==19?'Z':0;if(sep?c!=sep:c<'0'||c>'9')return false;}
   auto num=[&](size_t i,size_t len){unsigned v=0;while(len--)v=v*10+value[i++]-'0';return v;};
   unsigned year=num(0,4),month=num(5,2),day=num(8,2),hour=num(11,2),minute=num(14,2),second=num(17,2);
   auto leap=[](unsigned y){return y%4==0&&(y%100!=0||y%400==0);};
   const unsigned days[]={31,28,31,30,31,30,31,31,30,31,30,31};
   if(year<1970||year>2099||month<1||month>12||day<1||day>days[month-1]+(month==2&&leap(year))||hour>23||minute>59||second>59)return false;
   for(unsigned y=1970;y<year;++y)epoch+=365+leap(y);
   for(unsigned m=1;m<month;++m)epoch+=days[m-1]+(m==2&&leap(year));
   epoch=((epoch+day-1)*24+hour)*3600+minute*60+second;
  }else return false;
  ::findmy::secureErase(value,sizeof value);
  if(seen&bit)return false;
  seen|=bit;
  if(!take(field==3?'}':','))return false;
 }
 ws();if(pos!=n||seen!=15)return false;
 const auto status=::findmy::Provision::parse(master.data(),master.size(),seed.data(),seed.size(),0,epoch,out);
 ::findmy::secureErase(master.data(),master.size());::findmy::secureErase(seed.data(),seed.size());
 return status==::findmy::Status::Ok;
}
}
