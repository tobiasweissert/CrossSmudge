#pragma once
#include <string>
namespace fake {
inline unsigned clock = 0;
inline int clockOffset = 0;
inline bool trusted = true, chain = true, host = true, connect = true, cancelOnWrite = false;
inline bool verify = false, minimum = false, sni = false, identity = false, loaded = false;
inline int tcpOpen = 0, contexts = 0, sessions = 0;
inline unsigned address = 0x08080808;
inline std::string sent, response, expectedHost = "chat.owner.example";
inline size_t readPos = 0;
inline void reset() {
  clock = 0;
  clockOffset = 0;
  trusted = chain = host = connect = true;
  verify = minimum = sni = identity = loaded = false;
  sent.clear();
  readPos = 0;
  address = 0x08080808;
  response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 2\r\n\r\n{}";
}
}  // namespace fake
