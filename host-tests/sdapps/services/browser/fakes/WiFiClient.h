#pragma once
#include "Arduino.h"
struct IPAddress {
 uint32_t ip;
 IPAddress(unsigned a,unsigned b,unsigned c,unsigned d):ip((a<<24)|(b<<16)|(c<<8)|d){}
};
class WiFiClient {
 public:
 void setConnectionTimeout(int){}
 void setTimeout(int){}
 bool connect(IPAddress ip,int port){fixture.pinned=ip.ip;++fixture.tcpCalls;return fixture.connected=(port==443);}
 bool connected(){return fixture.connected;}
 int available(){return 0;}
 size_t write(const uint8_t*,int n){return n;}
 int read(uint8_t*,int){return 0;}
 void stop(){fixture.connected=false;}
};
