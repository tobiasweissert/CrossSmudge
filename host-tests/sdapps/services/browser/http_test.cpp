#include "BrowserHttp.h"
#include <cassert>
#include <iostream>
int main() {
 using namespace stickyweb;
 auto a=decodeResponse("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: 3\r\n\r\nabc",false);
 assert(a.ok && a.body=="abc");
 assert(!decodeResponse("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 4\r\n\r\nabc",false).ok);
 auto b=decodeResponse("HTTP/1.1 302 Found\r\nLocation: /next\r\n\r\n",false);
 assert(b.ok && b.status==302 && b.location=="/next");
 assert(!decodeResponse("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\nabc",false).ok);
 assert(decodeResponse("HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\nabc",true).ok);
 assert(!decodeResponse("HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: 3\r\n\r\nabc",true).ok);
 assert(!decodeResponse("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 3\r\nContent-Length: 3\r\n\r\nabc",true).ok);
 assert(!decodeResponse("HTTP/1.1 2000 OK\r\nContent-Type: text/html\r\nContent-Length: 3\r\n\r\nabc",true).ok);
 assert(!decodeResponse("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nBad Header: value\r\nContent-Length: 3\r\n\r\nabc",true).ok);
 assert(!decodeResponse("HTTP/1.1 302 Found\r\nLocation:\r\nLocation: /next\r\n\r\n",false).ok);
 std::cout << "HTTP framing tests passed\n";
}
