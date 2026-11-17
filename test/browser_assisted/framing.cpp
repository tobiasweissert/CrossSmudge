#define main originalFramingMain
#include "../browser/framing.cpp"
#undef main
int main(){Request q;assert(prepareBackend(q,"/v1/open","{}",std::string(64,'a')));Scratch s;Result r;std::atomic<bool> cancel{false};std::string output;
 r.capacity=49152;r.sinkContext=&output;r.sink=[](void* p,const char* b,size_t n){((std::string*)p)->append(b,n);return true;};
 auto run=[&](int status,std::string body,std::string extra=""){output.clear();r.capacity=q.limit;Peer p{"HTTP/1.1 "+std::to_string(status)+" Test\r\nContent-Type: "+std::string(!strcmp(q.url.path,"/v1/image")&&status==200?"image/png":"application/json")+"\r\nContent-Length: "+std::to_string(body.size())+"\r\n"+extra+"\r\n"+body};Io io{&p,Peer::connect,Peer::read,Peer::write,Peer::close,Peer::millis};exchange(q,r,s,io,cancel);};
 auto gz=zipped(std::string(49152,'a'));run(200,gz,"Content-Encoding: gzip\r\n");assert(!r.error && output.size()==49152);
 run(200,zipped(std::string(49153,'a')),"Content-Encoding: gzip\r\n");assert(r.error && std::string(r.error)=="body_limit");
 run(400,zipped(std::string(2048,'a')),"Content-Encoding: gzip\r\n");assert(!r.error && output.size()==2048);
 run(400,zipped(std::string(2049,'a')),"Content-Encoding: gzip\r\n");assert(r.error && std::string(r.error)=="body_limit");
 gz.back()^=1;run(200,gz,"Content-Encoding: gzip\r\n");assert(r.error);gz.back()^=1;run(200,gz+gz,"Content-Encoding: gzip\r\n");assert(r.error);
 assert(prepareBackend(q,"/v1/image","{}",std::string(64,'a')));run(200,std::string(65536,'a'));assert(!r.error && output.size()==65536);run(200,std::string(65537,'a'));assert(r.error);run(200,zipped("x"),"Content-Encoding: gzip\r\n");assert(r.error);
 run(200,"x","Set-Cookie: synthetic=denied\r\n");assert(r.error);run(200,"x","Location: /v1/open\r\n");assert(r.error);
 assert(!prepareBackend(q,"/v1/open",std::string("\xc0\xaf",2),std::string(64,'a')));
 assert(!prepareBackend(q,"/v1/open",std::string("\xed\xa0\x80",3),std::string(64,'a')));
 assert(prepareBackend(q,"/v1/open","{\"x\":\"\xf0\x9f\x98\x80\"}",std::string(64,'a')));
 puts("PASS backend decoded JSON/error/PNG caps; actual gzip/trailer/integrity; UTF8 request boundary");
}
