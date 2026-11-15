#pragma once
#include "Arduino.h"
#include <netinet/in.h>
#include <netdb.h>
inline int fixtureGetaddrinfo(const char*,const char*,const addrinfo*,addrinfo** out){
 static sockaddr_in addresses[2];static addrinfo answers[2];
 for(int i=0;i<2;++i){addresses[i]={};answers[i]={};addresses[i].sin_addr.s_addr=htonl(i?0x0a000001:0x5db8d822);answers[i].ai_addr=reinterpret_cast<sockaddr*>(&addresses[i]);answers[i].ai_addrlen=sizeof(sockaddr_in);answers[i].ai_family=AF_INET;}
 answers[0].ai_next=fixture.mixedDns?&answers[1]:nullptr;*out=answers;return 0;
}
inline void fixtureFreeaddrinfo(addrinfo*){}
#define getaddrinfo fixtureGetaddrinfo
#define freeaddrinfo fixtureFreeaddrinfo
