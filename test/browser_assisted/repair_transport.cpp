#define main originalFramingMain
#include "../browser/framing.cpp"
#undef main
// Production exchange, with byte-fragmented deterministic transport. TCP is a separate gate.
struct RepairPeer : Peer {
 int connections=0;
 static bool connect(void* p,const char*,uint32_t,const std::atomic<bool>&){++static_cast<RepairPeer*>(p)->connections;return true;}
};
int main(int argc,char** argv){
 assert(argc==2);const bool headers=std::string(argv[1])=="headers";
 Request q;Scratch s;Result r;std::atomic<bool> cancel{false};std::string output;
 r.sinkContext=&output;r.sink=[](void* p,const char* b,size_t n){static_cast<std::string*>(p)->append(b,n);return true;};
 auto run=[&](int status,const std::string& body,bool gzip,bool chunked,const std::string& initial,const std::string& trailer,bool png=false){
  output.clear();r.capacity=q.limit;RepairPeer p;
  const std::string wire=gzip?zipped(body):body;
  p.response="HTTP/1.1 "+std::to_string(status)+" Test\r\nContent-Type: "+(png?std::string("image/png"):std::string("application/json; charset=utf-8"))+"\r\n"+initial+(gzip?"Content-Encoding: gzip\r\n":"");
  if(chunked){char size[32];snprintf(size,sizeof(size),"%zx",wire.size());p.response+="Transfer-Encoding: chunked\r\n\r\n"+std::string(size)+"\r\n"+wire+"\r\n0\r\n"+trailer+"\r\n";}
  else p.response+="Content-Length: "+std::to_string(wire.size())+"\r\n\r\n"+wire;
  Io io{&p,RepairPeer::connect,Peer::read,Peer::write,Peer::close,Peer::millis};exchange(q,r,s,io,cancel);assert(p.connections==1);
 };
 assert(prepareBackend(q,"/v1/open","{}"));
 if(headers){
  for(bool gzip:{false,true}){
   for(const auto& field:{"Location: \r\n","LoCaTiOn: \r\n","Location:\t\r\n","Location: https://example.org/\r\n","Set-Cookie: \r\n","Cookie:\t\r\n","sEt-CoOkIe: x=y\r\n","cOoKiE: x=y\r\n"}){
    run(200,"{}",gzip,false,field,"");assert(r.error&&std::string(r.error)=="backend_headers");
    run(200,"{}",gzip,true,"",field);assert(r.error&&std::string(r.error)=="backend_headers");
   }
   run(200,"{}",gzip,true,"","X-Allowed: yes\r\n");assert(r.error); // Closed backend trailer policy.
   run(200,"{}",gzip,true,"","");assert(!r.error&&output=="{}");
  }
  for(auto field:{"Location : \r\n","Cookie\t: \r\n"," Set-Cookie: \r\n"}){run(200,"{}",false,false,field,"");assert(r.error);}
  for(int status=300;status<400;++status){run(status,"{}",false,false,"Location: https://example.org/\r\n","");assert(r.error);}
  q=Request{};q.browser=true;assert(parseUrl("https://example.org/",q.url));
  for(bool gzip:{false,true})for(const auto& field:{"Location: \r\n","Set-Cookie: x=y\r\n","Cookie: x=y\r\n","X-Test: ok\r\n"}){
   run(200,"{}",gzip,true,field,field);assert(!r.error&&output=="{}");
  }
  q.browser=false;run(200,"{}",false,false,"Cookie: x=y\r\n","");assert(!r.error&&r.size==2);
  run(200,"{}",false,true,"","Location: \r\n");assert(r.error&&std::string(r.error)=="protocol");
 }else{
  auto json=[](size_t n){return "{\"x\":\""+std::string(n-8,'x')+"\"}";};
  for(auto path:{"/v1/image","/v1/open"}){
   assert(prepareBackend(q,path,"{}"));
   for(int status:{200,201,202,206,400,429,500}){
    if(std::string(path)=="/v1/image"&&status==200)continue;
    size_t cap=status>=400?2048:49152;
    for(bool gzip:{false,true})for(bool chunks:{false,true}){
     run(status,json(cap),gzip,chunks,"","");assert(!r.error&&output.size()==cap);
     run(status,json(cap+1),gzip,chunks,"","");assert(r.error&&std::string(r.error)=="body_limit");
    }
   }
  }
  assert(prepareBackend(q,"/v1/image","{}"));
  run(200,std::string(65536,'x'),false,false,"","",true);assert(!r.error&&output.size()==65536);
  run(200,std::string(65537,'x'),false,false,"","",true);assert(r.error);
 }
 puts(headers?"PASS CB-NATIVE-001 closed backend fields/trailers; API3/API6 controls":"PASS CB-NATIVE-002 valid JSON/error/PNG response-kind caps");
}
