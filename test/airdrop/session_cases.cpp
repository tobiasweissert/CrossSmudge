#include "session/Session.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>
using namespace airdrop;
using namespace airdrop::session;
namespace {
struct Harness {
  std::unique_ptr<Workspace> workspace{new Workspace};
  std::unique_ptr<uint8_t[]> ask{new uint8_t[MaxAskBytes]};
  std::vector<uint8_t> input;
  std::vector<int> codes;
  std::vector<std::string> events;
  std::vector<uint8_t> output;
  std::string failure;
  std::vector<std::string> failuresReported;
  std::string offeredFilename, offeredArchivePath;
  bool responseCloses=false;
  Offer offered{};OfferIdentity identity{},queued{};
  uint64_t clock=1000,sequence=0,requestOffset=0,deadline=0;
  size_t position=0,split=2048,readBoundary=0;
  unsigned cancelOnWait=0,disconnectOnWait=0;
  unsigned reads=0,waits=0,pendingCount=0,decisionCalls=0,fileBegins=0,finishes=0,cancels=0,appendCalls=0,handovers=0;
  int answer=1;
  unsigned answerAfter=3,blockCount=0,wouldBlockEvery=0;
  uint64_t waitMs=1,stageCost=0,appendCost=0;
  bool slowAcceptedUrl=false,peerAlive=true,stop=false,eofDisconnect=false,appendFail=false,finishFail=false,stageFail=false,staleQueue=false,disconnectOnFinish=false,deadlineOnFinish=false,disconnectOnDecision=false,deadlineOnDecision=false,cancelBeforeDecision=false,writeFail=false;
  Harness(){workspace->ask=ask.get();}
  static Harness& self(void* p){return *static_cast<Harness*>(p);}
  static int read(void* p,uint8_t* out,size_t cap){auto& h=self(p);++h.reads;if(h.wouldBlockEvery&&h.reads%h.wouldBlockEvery==0)return 0;
    if(h.position==h.input.size())return h.eofDisconnect?-1:0;
    size_t n=std::min({cap,h.split,h.input.size()-h.position});if(h.position<h.readBoundary)n=std::min(n,h.readBoundary-h.position);std::memcpy(out,h.input.data()+h.position,n);h.position+=n;return int(n);
  }
  static bool write(void* p,const uint8_t* data,size_t n,uint64_t deadline){auto& h=self(p);if(h.clock>=deadline)return false;assert(h.clock<deadline);if(h.writeFail)return false;
    std::string value(reinterpret_cast<const char*>(data),n);
    if(value.find("Connection: close\r\n")!=std::string::npos)h.responseCloses=true;
    if(value.rfind("HTTP/1.1 ",0)==0){int code=std::stoi(value.substr(9,3));h.codes.push_back(code);h.events.push_back("http:"+std::to_string(code));if(code==200&&h.slowAcceptedUrl)h.clock=h.deadline+1;}
    else h.events.push_back("response-body");
    return true; // A local write may succeed after peer FIN; alive() remains the explicit peer signal.
  }
  static bool alive(void* p){return self(p).peerAlive;}
  static uint64_t now(void* p){return self(p).clock;}
  static void wait(void* p){auto& h=self(p);++h.waits;h.clock+=h.waitMs;
    if(h.cancelOnWait&&h.waits>=h.cancelOnWait)h.stop=true;
    if(h.disconnectOnWait&&h.waits>=h.disconnectOnWait)h.peerAlive=false;
    if(h.cancelBeforeDecision&&h.pendingCount&&h.decisionCalls>=3)h.stop=true;
  }
  static bool cancelled(void* p){return self(p).stop;}
  static void pending(void* p,const Offer& offer,const OfferIdentity& id,uint64_t deadline){auto& h=self(p);h.offered=offer;h.identity=id;h.deadline=deadline;++h.pendingCount;h.decisionCalls=0;h.events.push_back("pending");h.queued=id;if(h.staleQueue)++h.queued.generation;}
  static bool equal(const OfferIdentity& a,const OfferIdentity& b){return a.generation==b.generation&&a.session==b.session&&a.offer==b.offer&&a.connection==b.connection&&a.interfaceIndex==b.interfaceIndex&&std::memcmp(a.peer,b.peer,16)==0;}
  static int decision(void* p,const OfferIdentity& id){auto& h=self(p);++h.decisionCalls;
    // Real queue ownership is Receiver's boundary; this adapter only scripts filtered replies.
    if(!equal(h.queued,id)||h.decisionCalls<h.answerAfter)return 0;
    if(h.disconnectOnDecision)h.peerAlive=false;
    if(h.deadlineOnDecision)h.clock=h.deadline;if(h.slowAcceptedUrl)h.clock=h.deadline-1;
    if(h.answer)h.events.push_back(h.answer>0?"accept":"decline");return h.answer;
  }
  static bool beginFile(void* p,const Offer& offer){auto& h=self(p);++h.fileBegins;h.clock+=h.stageCost;h.events.push_back("begin-file");assert(offer.kind==OfferKind::File);h.offeredFilename=offer.filename;h.offeredArchivePath=offer.archivePath;return !h.stageFail;}
  static stream::WriteResult append(void* p,const uint8_t* data,size_t n){auto& h=self(p);assert(n<=1024);h.clock+=h.appendCost;++h.appendCalls;
    if(h.appendFail)return stream::WriteResult::Error;if(h.blockCount){--h.blockCount;return stream::WriteResult::WouldBlock;}
    h.output.insert(h.output.end(),data,data+n);h.events.push_back("append");return stream::WriteResult::Consumed;
  }
  static bool finishFile(void* p){auto& h=self(p);++h.finishes;h.events.push_back("finish-file");if(h.disconnectOnFinish)h.peerAlive=false;if(h.deadlineOnFinish)h.clock+=1800000;return !h.finishFail;}
  static void cancelFile(void* p){auto& h=self(p);++h.cancels;h.events.push_back("cancel-file");}
  static void progress(void*,uint64_t,uint64_t){}
  static void urlAccepted(void* p,const Offer& offer,const OfferIdentity& id){auto& h=self(p);++h.handovers;h.events.push_back("url-handoff");assert(offer.kind==OfferKind::Url&&equal(h.identity,id));}
  static void failed(void* p,const char* reason){self(p).failure=reason;self(p).failuresReported.emplace_back(reason);}
  void serve(uint64_t connection=1){
    Io io{this,read,write,alive,now,wait};Backend backend{this,cancelled,pending,decision,beginFile,append,finishFile,cancelFile,progress,urlAccepted,failed};
    OfferIdentity token{};token.generation=1;token.session=connection;token.connection=connection;token.interfaceIndex=3;token.peer[0]=0xfe;token.peer[1]=0x80;token.peer[15]=1;
    session::serve(*workspace,io,backend,token,sequence);
  }
  size_t event(const std::string& name)const{auto at=std::find(events.begin(),events.end(),name);return at==events.end()?events.size():size_t(at-events.begin());}
  unsigned count(int code)const{return unsigned(std::count(codes.begin(),codes.end(),code));}
  void reconnect(std::vector<uint8_t> next){input=std::move(next);position=0;peerAlive=true;stop=false;eofDisconnect=false;reads=0;}
};
std::vector<uint8_t> load(const std::string& root,const char* name){std::ifstream f(root+"/"+name,std::ios::binary);assert(f);return {std::istreambuf_iterator<char>(f),{}};}
void add(std::vector<uint8_t>& to,const std::vector<uint8_t>& from){to.insert(to.end(),from.begin(),from.end());}
std::vector<uint8_t> request(const char* route,const std::vector<uint8_t>& body,const std::string& extra="",bool chunked=false){
  std::string head=std::string("POST ")+route+" HTTP/1.1\r\n"+(chunked?"Transfer-Encoding: chunked\r\n":"Content-Length: "+std::to_string(body.size())+"\r\n")+extra+"\r\n";
  std::vector<uint8_t> result(head.begin(),head.end());
  if(chunked){char n[32];std::snprintf(n,sizeof n,"%zx\r\n",body.size());result.insert(result.end(),n,n+std::strlen(n));add(result,body);static const char end[]="\r\n0\r\n\r\n";result.insert(result.end(),end,end+sizeof end-1);}else add(result,body);return result;
}
const std::string uploadHeaders="Content-Type: application/x-cpio\r\nTransferID: A3F34E01-1234-5678-ABCD-EF0123456789\r\nExpect: 100-continue\r\n";
int failures=0;
#define CHECK(test, condition) do{if(!(condition)){std::fprintf(stderr,"FAIL %s line%d: %s\n",test,__LINE__,#condition);++failures;}}while(false)
}
#include "session_framing_cases.inc"
int main(int argc,char** argv){
  assert(argc==2);std::string root=argv[1];auto url=load(root,"url.plist"),file=load(root,"file.plist"),legacy=load(root,"file-noid.plist"),stored=load(root,"stored.dvzip"),compressed=load(root,"zlib.dvzip");
  framingCases(root,file);
  {Harness h;h.answerAfter=1;h.stageCost=16000;h.appendCost=1;
    h.input=request("/Ask",load(root,"large.plist"));add(h.input,request("/Upload",load(root,"large.dvzip"),uploadHeaders));h.serve();
    CHECK("above 2MB with productive local pause",h.finishes==1&&h.count(200)==2&&h.output.size()==2000001&&std::all_of(h.output.begin(),h.output.end(),[](uint8_t c){return c=='x';}));
  }
  for(bool staging:{false,true}){
    Harness h;h.answerAfter=1;h.split=13;h.stageCost=staging?16000:0;h.appendCost=staging?0:16000;
    h.input=request("/Ask",file);add(h.input,request("/Upload",stored,uploadHeaders));h.serve();
    CHECK("local staging/SD time is not peer idle time",h.finishes==1&&h.count(200)==2&&h.output.size()==3);
  }
  {Harness h;h.input=request("/Ask",file);auto upload=request("/Upload",stored,uploadHeaders);upload.resize(upload.size()-10);add(h.input,upload);h.waitMs=1000;h.serve();
    bool idle=false;for(auto& s:h.failuresReported)idle|=s.find("15-second input stall")!=std::string::npos;
    CHECK("genuinely idle upload still expires",idle&&h.finishes==0&&h.cancels==1);
  }
  {Harness h;h.answerAfter=1;h.stageCost=1800000;h.input=request("/Ask",file);add(h.input,request("/Upload",stored,uploadHeaders));h.serve();
    CHECK("local work still counts toward absolute upload deadline",h.finishes==0&&h.count(200)==1&&h.cancels==1);
  }
  {Harness h;h.input=request("/Discover",{});h.serve();
    CHECK("discovery closes promptly for separate Ask",h.responseCloses&&h.count(200)==1&&h.waits==0&&h.pendingCount==0);
    h.reconnect(request("/Ask",file));add(h.input,request("/Upload",stored,uploadHeaders));h.serve(2);
    CHECK("separate Ask after discovery",h.pendingCount==1&&h.count(200)==3&&h.finishes==1);
  }
  for(size_t split:{size_t(1),size_t(2),size_t(7),size_t(17),size_t(2048)}){
    Harness h;h.input=request("/Ask",url);h.split=split;h.wouldBlockEvery=3;h.serve();
    CHECK("native URL",h.pendingCount==1&&h.count(200)==1&&h.handovers==1&&h.fileBegins==0&&h.output.empty());
    CHECK("no autoaccept",h.event("pending")<h.event("accept")&&h.event("accept")<h.event("http:200"));
    CHECK("ack before handoff",h.event("response-body")<h.event("url-handoff"));
  }
  for(int mode=0;mode<4;++mode){Harness h;h.input=request("/Ask",url);h.answer=mode==0?-1:0;h.waitMs=1000;
    if(mode==2){h.staleQueue=true;h.cancelBeforeDecision=true;}if(mode==3){h.cancelBeforeDecision=true;}
    h.serve();CHECK("decline timeout stale cancel",h.pendingCount==1&&h.count(200)==0&&h.handovers==0&&h.fileBegins==0&&h.output.empty());
  }
  {Harness h;h.input=request("/Ask",url);h.slowAcceptedUrl=true;h.serve();CHECK("accepted URL survives bounded positive ACK crossing consent expiry",h.count(200)==1&&h.handovers==1);}
  {Harness h;h.input=request("/Ask",url);h.disconnectOnDecision=true;h.serve();CHECK("disconnect before response",h.count(200)==0&&h.handovers==0&&h.fileBegins==0);}
  {Harness h;h.input=request("/Ask",url);h.deadlineOnDecision=true;h.serve();CHECK("deadline at Accept",h.count(200)==0&&h.handovers==0&&h.fileBegins==0);}
  for(bool compressedRecord:{false,true})for(bool chunked:{false,true})for(size_t split:{size_t(1),size_t(13),size_t(2048)}){
    Harness h;h.input=request("/Ask",file);add(h.input,request("/Upload",compressedRecord?compressed:stored,uploadHeaders,chunked));h.split=split;h.blockCount=3;h.wouldBlockEvery=4;h.serve();
    CHECK("file pipeline",h.count(200)==2&&h.count(100)==1&&h.fileBegins==1&&h.finishes==1&&h.cancels==0&&std::string(h.output.begin(),h.output.end())=="abc");
    CHECK("publication before success",h.event("accept")<h.event("http:200")&&h.event("http:200")<h.event("begin-file")&&h.event("accept")<h.event("begin-file")&&h.event("begin-file")<h.event("http:100")&&h.event("finish-file")<h.events.size()&&h.events.back()=="http:200");
  }
  for(const char* mime : {"application/x-cpio; charset=utf-8", "application/x-apple-cpio", "application/octet-stream", ""}) {
    std::string headers="TransferID: A3F34E01-1234-5678-ABCD-EF0123456789\r\n";
    if(*mime)headers+=std::string("Content-Type: ")+mime+"\r\n";
    Harness h;h.input=request("/Ask",file);add(h.input,request("/Upload",stored,headers));h.serve();
    CHECK("MIME metadata does not reject valid approved archive",h.count(200)==2&&h.finishes==1&&std::string(h.output.begin(),h.output.end())=="abc");
    Harness invalid;invalid.input=request("/Ask",file);add(invalid.input,request("/Upload",{'b','a','d'},headers));invalid.serve();
    CHECK("MIME metadata cannot bypass archive validation",invalid.finishes==0&&invalid.count(200)==1&&invalid.cancels==1);
  }
  for(bool zipped:{false,true})for(size_t split:{size_t(1),size_t(13),size_t(2048)}){
    Harness h;h.input=request("/Ask",file);auto archive=zipped?compressed:stored;archive.resize(archive.size()-4);
    auto upload=request("/Upload",archive,uploadHeaders,true);upload.resize(upload.size()-5); // no HTTP zero chunk
    add(h.input,upload);h.split=split;h.serve();
    CHECK("native archive completion without HTTP last chunk",h.finishes==1&&h.cancels==0&&h.count(200)==2&&h.responseCloses&&h.clock<16000&&std::string(h.output.begin(),h.output.end())=="abc");
  }
  {Harness h;h.input=request("/Ask",file);auto corrupt=compressed;corrupt[corrupt.size()-5]^=1;auto upload=request("/Upload",corrupt,uploadHeaders,true);upload.resize(upload.size()-5);add(h.input,upload);h.serve();
    CHECK("archive completion cannot bypass zlib checksum",h.finishes==0&&h.cancels==1&&h.count(200)==1);
  }
  {Harness h;h.input=request("/Upload",stored,uploadHeaders);h.serve();CHECK("unauthorized Upload",h.pendingCount==0&&h.count(100)==0&&h.count(200)==0&&h.fileBegins==0);}
  {Harness h;h.input=request("/Ask",file);std::string wrong=uploadHeaders;wrong.replace(wrong.find("A3F34E01"),8,"B3F34E01");add(h.input,request("/Upload",stored,wrong));h.serve();CHECK("wrong TransferID",h.count(200)==1&&h.count(100)==0&&h.fileBegins==0);}
  for(bool expired : {false,true}) {Harness h;h.input=request("/Ask",file);add(h.input,request("/Upload",stored,uploadHeaders));h.disconnectOnFinish=!expired;h.deadlineOnFinish=expired;h.serve();CHECK("no final ACK after publication loses live deadline/channel",h.count(200)==1&&h.finishes==1&&h.cancels==0);}
  {Harness h;h.input=request("/Ask",legacy);add(h.input,request("/Upload",stored,"Content-Type: application/x-cpio\r\nExpect: 100-continue\r\n"));h.serve();CHECK("live legacy consent",h.count(200)==2&&h.fileBegins==1&&h.finishes==1);}
  {Harness h;h.input=request("/Ask",file);h.eofDisconnect=true;h.serve();h.reconnect(request("/Upload",stored,uploadHeaders));h.serve(2);CHECK("disconnected approval",h.count(200)==1&&h.count(100)==0&&h.fileBegins==0);}
  {Harness h;h.input=request("/Ask",file);add(h.input,request("/Upload",stored,uploadHeaders));h.serve();auto begins=h.fileBegins;h.reconnect(request("/Upload",stored,uploadHeaders));h.serve(2);CHECK("replayed Upload",h.count(200)==2&&h.count(100)==1&&h.fileBegins==begins&&h.finishes==1);}
  {Harness h;h.input=request("/Ask",file);add(h.input,request("/Upload",stored,uploadHeaders));h.serve();h.reconnect(request("/Ask",file));add(h.input,request("/Upload",stored,uploadHeaders));h.serve(2);CHECK("fresh approval every transfer",h.pendingCount==2&&h.count(200)==4&&h.count(100)==2&&h.fileBegins==2&&h.finishes==2&&h.sequence==2);}
  for(int fault=0;fault<4;++fault){Harness h;h.input=request("/Ask",file);auto archive=stored;if(fault==3)archive.resize(archive.size()-6);add(h.input,request("/Upload",archive,uploadHeaders));h.stageFail=fault==0;h.appendFail=fault==1;h.finishFail=fault==2;h.eofDisconnect=true;h.serve();CHECK("storage/archive error",h.count(200)==1&&h.handovers==0);if(fault)CHECK("partial cancelled",h.cancels==1);if(fault==1||fault==3)CHECK("no finish invalid bytes",h.finishes==0);}
  {Harness h;h.input=request("/Ask",file);auto incomplete=request("/Upload",stored,uploadHeaders);incomplete.resize(incomplete.size()-1);add(h.input,incomplete);h.waitMs=1000;h.serve();
    CHECK("stalled incomplete HTTP body cannot publish",h.finishes==0&&h.cancels==1&&h.count(200)==1);
    CHECK("input stall identifies byte counts",std::any_of(h.failuresReported.begin(),h.failuresReported.end(),[](const auto& reason){return reason.find("15-second input stall; wire=")!=std::string::npos&&reason.find("file=3")!=std::string::npos;}));
  }
  {Harness h;h.input=request("/Ask",url,"Content-Length: 1\r\n");h.serve();CHECK("duplicate framing",h.count(200)==0&&h.pendingCount==0&&h.fileBegins==0);}
  {Harness h;h.input=request("/Ask",url);h.writeFail=true;h.serve();CHECK("failed ack write",h.handovers==0&&h.fileBegins==0);}
  {Harness h;h.stageFail=true;h.input=request("/Ask",file);add(h.input,request("/Upload",stored,uploadHeaders));h.serve();
    CHECK("AD-NAME-004 collision-safe staging failure explains no overwrite",h.failure.find("destination exists or SD unavailable")!=std::string::npos&&h.failure.find("no overwrite")!=std::string::npos&&h.count(507)==1&&h.count(200)==1&&h.count(100)==0&&h.finishes==0);}
  {Harness h;const std::string name="Résumé e\xcc\x81 你好.EPUB";h.input=request("/Ask",load(root,"utf8.plist"));add(h.input,request("/Upload",load(root,"utf8.dvzip"),uploadHeaders));h.serve();
    CHECK("AD-NAME-008 real plist/consent/session/decoder forwards original UTF8 name",h.count(200)==2&&h.finishes==1&&h.offeredFilename==name&&h.offeredArchivePath=="./"+name&&std::string(h.output.begin(),h.output.end())=="abc");}
  std::printf("production Session tests: failures=%d workspace=%zu; no device proof\n",failures,sizeof(Workspace));return failures?1:0;
}
