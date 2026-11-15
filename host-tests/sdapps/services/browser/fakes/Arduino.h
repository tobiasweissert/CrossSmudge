#pragma once
#include <cstdint>
#include <string>
struct Fixture {
 uint32_t clock=0, pinned=0;
 int tcpCalls=0, handshakes=0, verify=0, minimum=0, trust=0, frees=0;
 bool mixedDns=false, badChain=false, badHost=false, connected=false, validTime=true, rollback=false;
 std::string domain,sni,request,response="HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nContent-Length: 9\r\n\r\n<p>ok</p>";
 size_t read=0;
};
inline Fixture fixture;
inline uint32_t millis(){return fixture.clock++;}
inline void delay(int n){fixture.clock+=n;}
