#include "BrowserFetch.h"
#include "Arduino.h"
#include <cassert>
#include <iostream>
using namespace stickyweb;
int main(){
 auto never=[](){return false;};
 fixture={};auto r=fetchOnce("https://example.org/read?q=1",never);
 assert(r.ok&&r.body=="<p>ok</p>");
 assert(fixture.pinned==0x5db8d822&&fixture.domain=="example.org"&&fixture.sni=="example.org");
 assert(fixture.request.find("GET /read?q=1 HTTP/1.0\r\nHost: example.org\r\n")==0);
 assert(fixture.request.find("Cookie:")==std::string::npos&&fixture.request.find("Authorization:")==std::string::npos);
 assert(!fixture.connected&&fixture.frees==1);
 fixture={};fixture.mixedDns=true;r=fetchOnce("https://example.org/",never);assert(!r.ok&&fixture.tcpCalls==0);
 fixture={};fixture.badChain=true;r=fetchOnce("https://example.org/",never);assert(!r.ok&&fixture.request.empty()&&!fixture.connected);
 fixture={};fixture.badHost=true;r=fetchOnce("https://example.org/",never);assert(!r.ok&&fixture.request.empty());
 fixture={};fixture.rollback=true;r=fetchOnce("https://example.org/",never);assert(!r.ok&&fixture.tcpCalls==0&&fixture.handshakes==0);
 fixture={};fixture.validTime=false;r=fetchOnce("https://example.org/",never);assert(!r.ok&&fixture.tcpCalls==0);
 fixture={};r=fetchOnce("https://example.org/",[](){return true;});assert(!r.ok&&fixture.tcpCalls==0);
 fixture={};r=fetchOnce("http://example.org/",never);assert(!r.ok&&fixture.tcpCalls==0);
 fixture={};r=fetchOnce("https://example.org/",[](){return fixture.handshakes>0;});assert(!r.ok&&fixture.request.empty()&&!fixture.connected);
 std::cout<<"Production fetch adapter boundary fixtures passed (fake TLS/DNS; not cryptographic qualification)\n";
}
