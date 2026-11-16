#include "HttpRequest.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <string_view>

namespace airdrop {
namespace {
unsigned char lower(unsigned char c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
bool fold(std::string_view a, std::string_view b) {
  if(a.size()!=b.size())return false;
  for(size_t i=0;i<a.size();++i)if(lower(a[i])!=lower(b[i]))return false;
  return true;
}
bool token(unsigned char c) {
  return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||std::strchr("!#$%&'*+-.^_`|~",c);
}
int hex(unsigned char c) { if(c>='0'&&c<='9')return c-'0';c=lower(c);return c>='a'&&c<='f'?c-'a'+10:-1; }
bool decimal(std::string_view s,uint64_t& out) {
  if(s.empty())return false;
  out=0;
  for(unsigned char c:s){if(c<'0'||c>'9'||out>(std::numeric_limits<uint64_t>::max()-(c-'0'))/10)return false;out=out*10+c-'0';}return true;
}
bool copy(std::string_view s,char* out,size_t cap) {if(s.size()>=cap)return false;std::memcpy(out,s.data(),s.size());out[s.size()]=0;return true;}
}
HttpState RequestParser::feed(const uint8_t* data,size_t size,size_t& consumed) {
  consumed=0;if(state!=HttpState::NeedMore)return state;
  while(consumed<size){uint8_t c=data[consumed++];if(++bytes>16384){state=HttpState::Error;break;}
    if(cr){cr=false;if(c!='\n'||!finishLine()){state=HttpState::Error;break;}used=0;if(state!=HttpState::NeedMore)break;continue;}
    if(c=='\r'){cr=true;continue;}
    if(c=='\n'||!c||c>126||(c<32&&c!='\t')||used>=sizeof(line)-1){state=HttpState::Error;break;}
    line[used++]=static_cast<char>(c);
  }
  return state;
}
bool RequestParser::finishLine() {
  std::string_view s(line,used);
  if(first){first=false;if(s=="POST /Discover HTTP/1.1")parsed.route=Route::Discover;else if(s=="POST /Ask HTTP/1.1")parsed.route=Route::Ask;else if(s=="POST /Upload HTTP/1.1")parsed.route=Route::Upload;else return false;return true;}
  if(s.empty()){
    if(parsed.framing==Framing::None)return false;
    if(parsed.route==Route::Ask&&parsed.framing==Framing::ContentLength&&parsed.length>524288)return false;
    if(parsed.route==Route::Discover&&parsed.framing==Framing::ContentLength&&parsed.length>524288)return false;
    if(parsed.route==Route::Upload&&parsed.framing==Framing::ContentLength&&parsed.length>70000000)return false;
    state=HttpState::Ready;return true;
  }
  if(++headers>64||s.front()==' '||s.front()=='\t')return false;
  auto colon=s.find(':');if(colon==s.npos||!colon)return false;
  auto name=s.substr(0,colon);for(unsigned char c:name)if(!token(c))return false;
  auto value=s.substr(colon+1);while(!value.empty()&&(value.front()==' '||value.front()=='\t'))value.remove_prefix(1);
  while(!value.empty()&&(value.back()==' '||value.back()=='\t'))value.remove_suffix(1);
  for(unsigned char c:value)if(c<32||c==127)return false;
  if(fold(name,"Content-Length")){if(lengthSeen||parsed.framing==Framing::Chunked||!decimal(value,parsed.length))return false;lengthSeen=true;parsed.framing=Framing::ContentLength;}
  else if(fold(name,"Transfer-Encoding")){if(parsed.framing!=Framing::None||!fold(value,"chunked"))return false;parsed.framing=Framing::Chunked;}
  else if(fold(name,"Content-Type")){if(typeSeen||!copy(value,parsed.contentType,sizeof parsed.contentType))return false;typeSeen=true;}
  else if(fold(name,"TransferID")){if(transferSeen||value.size()!=36||!copy(value,parsed.transferId,sizeof parsed.transferId))return false;for(size_t i=0;i<value.size();++i)if(i==8||i==13||i==18||i==23){if(value[i]!='-')return false;}else if(hex(value[i])<0)return false;transferSeen=true;}
  else if(fold(name,"Expect")){if(expectSeen||!fold(value,"100-continue"))return false;expectSeen=true;parsed.expectContinue=true;}
  else if(fold(name,"Trailer"))return false; // No trailers admitted; prevents a second authorization/framing surface.
  return true;
}
BodyDecoder::BodyDecoder(const HttpRequest& r,uint64_t maxBody,uint64_t maxWire):framing(r.framing),bodyLimit(maxBody),wireLimit(maxWire) {
  if(framing==Framing::ContentLength&&r.length<=bodyLimit&&r.length<=wireLimit){remaining=r.length;phase=remaining?Phase::Data:Phase::Done;}
  else if(framing==Framing::Chunked)phase=Phase::Size;
}
BodyState BodyDecoder::feed(const uint8_t* data,size_t size,size_t& consumed,BodySink sink,void* ctx) {
  consumed=0;
  while(consumed<size&&phase!=Phase::Done&&phase!=Phase::Failed){
    if(phase==Phase::Data){
      size_t n=static_cast<size_t>(std::min<uint64_t>({remaining,size-consumed,2048}));
      if(!n||n>bodyLimit-payload||n>wireLimit-wire||!sink){phase=Phase::Failed;break;}
      SinkResult result=sink(ctx,data+consumed,n);
      if(result==SinkResult::WouldBlock)return BodyState::WouldBlock;
      if(result==SinkResult::Error){phase=Phase::Failed;break;}
      payload+=n;wire+=n;consumed+=n;remaining-=n;
      if(!remaining)phase=framing==Framing::ContentLength?Phase::Done:Phase::DataCr;
      continue;
    }
    if(wire>=wireLimit){phase=Phase::Failed;break;}
    uint8_t c=data[consumed++];++wire;
    switch(phase){
      case Phase::Size:{int v=hex(c);if(v>=0){if(digits>=16||chunk>(std::numeric_limits<uint64_t>::max()-v)/16){phase=Phase::Failed;break;}chunk=chunk*16+v;++digits;}else if(c=='\r'&&digits){phase=Phase::SizeLf;}else phase=Phase::Failed;break;}
      case Phase::SizeLf:if(c!='\n'||++chunks>16384||chunk>bodyLimit-payload){phase=Phase::Failed;break;}remaining=chunk;phase=chunk?Phase::Data:Phase::EndCr;chunk=0;digits=0;break;
      case Phase::DataCr:phase=c=='\r'?Phase::DataLf:Phase::Failed;break;
      case Phase::DataLf:phase=c=='\n'?Phase::Size:Phase::Failed;break;
      case Phase::EndCr:phase=c=='\r'?Phase::EndLf:Phase::Failed;break;
      case Phase::EndLf:phase=c=='\n'?Phase::Done:Phase::Failed;break;
      default:phase=Phase::Failed;break;
    }
  }
  return phase==Phase::Done?BodyState::Complete:phase==Phase::Failed?BodyState::Error:BodyState::NeedMore;
}
}  // namespace airdrop
