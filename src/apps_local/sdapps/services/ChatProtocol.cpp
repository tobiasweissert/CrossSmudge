#include "ChatProtocol.h"

#include <array>
namespace stickychat {
namespace {
bool asciiAlnum(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); }
// Reject numeric component syntax without libc/lwIP or a DNS lookup. Range
// validation is unnecessary: even out-of-range numeric-looking values are not
// owner DNS names. lwIP's one-to-four components may use decimal/octal or 0x hex.
bool numericIpv4Host(std::string_view host) {
  size_t start = 0;
  unsigned parts = 0;
  while (start < host.size()) {
    if (++parts > 4) return false;
    auto end = host.find('.', start);
    if (end == host.npos) end = host.size();
    auto part = host.substr(start, end - start);
    bool hex = part.size() >= 2 && part[0] == '0' && (part[1] == 'x' || part[1] == 'X');
    size_t digit = hex ? 2 : 0;
    if (part.empty()) return false;
    for (; digit < part.size(); ++digit) {
      char c = part[digit];
      if (!(c >= '0' && c <= '9') && !(hex && ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))))
        return false;
    }
    start = end + 1;
  }
  return parts != 0;
}
struct Json {
  std::string_view s;
  size_t p = 0;
  void ws() {
    while (p < s.size() && (s[p] == ' ' || s[p] == '\t' || s[p] == '\r' || s[p] == '\n')) ++p;
  }
  bool take(char c) {
    ws();
    if (p == s.size() || s[p] != c) return false;
    ++p;
    return true;
  }
  int hex4() {
    if (p + 4 > s.size()) return -1;
    int v = 0;
    for (int i = 0; i < 4; ++i) {
      char c = s[p++];
      int n = c >= '0' && c <= '9'   ? c - '0'
              : c >= 'a' && c <= 'f' ? c - 'a' + 10
              : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                     : -1;
      if (n < 0) return -1;
      v = v * 16 + n;
    }
    return v;
  }
  bool string(std::string& out, size_t cap) {
    if (!take('"')) return false;
    out.clear();
    while (p < s.size()) {
      unsigned char c = s[p++];
      if (c == '"') return true;
      if (c < 32) return false;
      if (c == '\\') {
        if (p == s.size()) return false;
        c = s[p++];
        if (c == 'u') {
          int n = hex4();
          if (n <= 0) return false;
          if (n >= 0xd800 && n <= 0xdbff) {
            if (p + 2 > s.size() || s.substr(p, 2) != "\\u") return false;
            p += 2;
            int lo = hex4();
            if (lo < 0xdc00 || lo > 0xdfff) return false;
            n = 0x10000 + ((n - 0xd800) << 10) + lo - 0xdc00;
          } else if (n >= 0xdc00 && n <= 0xdfff)
            return false;
          if (n < 128)
            out += static_cast<char>(n);
          else if (n < 2048) {
            out += static_cast<char>(0xc0 | (n >> 6));
            out += static_cast<char>(0x80 | (n & 63));
          } else if (n < 65536) {
            out += static_cast<char>(0xe0 | (n >> 12));
            out += static_cast<char>(0x80 | ((n >> 6) & 63));
            out += static_cast<char>(0x80 | (n & 63));
          } else {
            out += static_cast<char>(0xf0 | (n >> 18));
            out += static_cast<char>(0x80 | ((n >> 12) & 63));
            out += static_cast<char>(0x80 | ((n >> 6) & 63));
            out += static_cast<char>(0x80 | (n & 63));
          }
        } else {
          switch (c) {
            case '"':
            case '\\':
            case '/':
              out += c;
              break;
            case 'b':
              out += '\b';
              break;
            case 'f':
              out += '\f';
              break;
            case 'n':
              out += '\n';
              break;
            case 'r':
              out += '\r';
              break;
            case 't':
              out += '\t';
              break;
            default:
              return false;
          }
        }
      } else if (c < 128)
        out += c;
      else {
        unsigned count = c >= 0xc2 && c <= 0xdf ? 1 : c >= 0xe0 && c <= 0xef ? 2 : c >= 0xf0 && c <= 0xf4 ? 3 : 0;
        if (!count || p + count > s.size()) return false;
        unsigned char first = s[p];
        if ((c == 0xe0 && first < 0xa0) || (c == 0xed && first >= 0xa0) || (c == 0xf0 && first < 0x90) ||
            (c == 0xf4 && first >= 0x90))
          return false;
        out += c;
        for (unsigned i = 0; i < count; ++i) {
          unsigned char b = s[p++];
          if ((b & 0xc0) != 0x80) return false;
          out += b;
        }
      }
      if (out.size() > cap) return false;
    }
    return false;
  }
};
}  // namespace
bool validateConfig(OwnerConfig& c) {
  c.host.clear();
  if (c.url.size() > 261 || c.url.substr(0, 8) != "https://" || c.ca.size() > 12288 ||
      c.ca.find("-----BEGIN CERTIFICATE-----") == c.ca.npos || c.token.empty() || c.token.size() > 512)
    return false;
  for (char x : c.token)
    if (!asciiAlnum(x) && x != '-' && x != '_' && x != '.' && x != '~') return false;
  auto h = c.url.substr(8);
  if (h.empty() || h.size() > 253 || h.front() == '.' || h.back() == '.') return false;
  size_t start = 0;
  bool alpha = false;
  for (size_t i = 0; i <= h.size(); ++i) {
    if (i == h.size() || h[i] == '.') {
      if (i == start || i - start > 63 || h[start] == '-' || h[i - 1] == '-') return false;
      start = i + 1;
    } else {
      if (!asciiAlnum(h[i]) && h[i] != '-') return false;
      if ((h[i] >= 'a' && h[i] <= 'z') || (h[i] >= 'A' && h[i] <= 'Z')) alpha = true;
    }
  }
  if (!alpha || numericIpv4Host(h)) return false;
  c.host = std::move(h);
  return true;
}
bool allowedIpv4(uint32_t ip, bool allowPrivate) {
  unsigned a = ip >> 24, b = (ip >> 16) & 255, c = (ip >> 8) & 255;
  bool priv = a == 10 || (a == 172 && b >= 16 && b <= 31) || (a == 192 && b == 168);
  if (priv) return allowPrivate;
  return a != 0 && a != 127 && a < 224 && !(a == 100 && b >= 64 && b <= 127) && !(a == 169 && b == 254) &&
         !(a == 192 && b == 0 && (c == 0 || c == 2)) && !(a == 192 && b == 88 && c == 99) &&
         !(a == 198 && (b == 18 || b == 19)) && !(a == 198 && b == 51 && c == 100) && !(a == 203 && b == 0 && c == 113);
}
bool validProviderKey(std::string_view key) {
  if (key.empty() || key.size() > 512) return false;
  for (char c : key) if (!asciiAlnum(c) && c != '-' && c != '_') return false;
  return true;
}
std::string providerChatBody(std::string_view request) {
  // Only accepts the Session serializer's fixed prefix, never caller-selected routes/models.
  auto start = request.find(",\"messages\":[");
  if (start == request.npos || request.size() > kContextMax) return {};
  std::string body = "{\"model\":\"gpt-4.1-mini\",\"store\":false,\"stream\":false,\"max_completion_tokens\":2048";
  body += request.substr(start);
  if (body.size() > kContextMax) return {};
  return body;
}
namespace {
struct ProviderJson {
  Json j;
  Envelope e;
  bool transcript, textSeen = false, roleSeen = false, finishSeen = false;
  unsigned nodes = 0;
  bool value(const std::string& path, unsigned depth = 0) {
    if (depth > 12 || ++nodes > 2048) return false;
    j.ws();
    if (j.p == j.s.size()) return false;
    if ((path == (transcript ? "/text" : "/choices/0/message/content") ||
         (!transcript && (path == "/choices/0/message/role" || path == "/choices/0/finish_reason"))) &&
        j.s[j.p] != '"') return false;
    if (j.s[j.p] == '"') {
      std::string text;
      if (!j.string(text, kReplyMax)) return false;
      if (path == (transcript ? "/text" : "/choices/0/message/content")) {
        if (textSeen || text.empty() || text.size() > (transcript ? kDraftMax : kReplyMax)) return false;
        textSeen = true; e.text = std::move(text);
      } else if (!transcript && path == "/choices/0/message/role") {
        if (roleSeen || text != "assistant") return false;
        roleSeen = true;
      } else if (!transcript && path == "/choices/0/finish_reason") {
        if (finishSeen || text != "stop") return false;
        finishSeen = true;
      }
      return true;
    }
    if (j.take('{')) {
      if (j.take('}')) return true;
      do {
        std::string key;
        if (!j.string(key, 128) || key.find('/') != key.npos || !j.take(':') ||
            !value(path + '/' + key, depth + 1)) return false;
        if (j.take('}')) return true;
      } while (j.take(','));
      return false;
    }
    if (j.take('[')) {
      if (j.take(']')) return true;
      unsigned index = 0;
      do {
        if (!value(path + '/' + std::to_string(index++), depth + 1)) return false;
        if (j.take(']')) return true;
      } while (j.take(','));
      return false;
    }
    auto start = j.p;
    while (j.p < j.s.size() && j.s[j.p] != ',' && j.s[j.p] != '}' && j.s[j.p] != ']' &&
           j.s[j.p] != ' ' && j.s[j.p] != '\n' && j.s[j.p] != '\r' && j.s[j.p] != '\t') ++j.p;
    auto atom = j.s.substr(start, j.p-start);
    if (atom == "null" || atom == "true" || atom == "false") return true;
    // Strict JSON number grammar for ignored usage/metadata.
    size_t n = 0;
    if (n < atom.size() && atom[n] == '-') ++n;
    if (n == atom.size()) return false;
    if (atom[n] == '0') ++n;
    else {
      if (atom[n] < '1' || atom[n] > '9') return false;
      while (n < atom.size() && atom[n] >= '0' && atom[n] <= '9') ++n;
    }
    if (n < atom.size() && atom[n] == '.') {
      size_t first = ++n;
      while (n < atom.size() && atom[n] >= '0' && atom[n] <= '9') ++n;
      if (first == n) return false;
    }
    if (n < atom.size() && (atom[n] == 'e' || atom[n] == 'E')) {
      ++n;
      if (n < atom.size() && (atom[n] == '+' || atom[n] == '-')) ++n;
      size_t first = n;
      while (n < atom.size() && atom[n] >= '0' && atom[n] <= '9') ++n;
      if (first == n) return false;
    }
    return n == atom.size();
  }
};
}
Envelope parseProviderEnvelope(std::string_view input, bool transcription) {
  if (input.size() > kResponseMax) return {};
  ProviderJson parser{Json{input}, {}, transcription};
  if (!parser.value("")) return {};
  parser.j.ws();
  parser.e.ok = parser.j.p == input.size() && parser.textSeen &&
                (transcription || (parser.roleSeen && parser.finishSeen));
  return parser.e;
}
Envelope parseEnvelope(std::string_view input, bool transcription) {
  Envelope e;
  if (input.size() > kResponseMax) return e;
  Json j{input};
  if (!j.take('{')) return e;
  bool seenText = false, seenId = false;
  for (int field = 0; field < 2; ++field) {
    std::string key, value;
    if (!j.string(key, 16) || !j.take(':')) return e;
    if (key == (transcription ? "text" : "reply")) {
      if (seenText || !j.string(value, transcription ? kDraftMax : kReplyMax) || value.empty()) return e;
      seenText = true;
      e.text = std::move(value);
    } else if (!transcription && key == "request_id") {
      if (seenId || !j.string(value, 64) || value.empty()) return e;
      seenId = true;
      e.id = std::move(value);
    } else
      return e;
    j.ws();
    if (j.take('}')) {
      j.ws();
      e.ok = seenText && (transcription || seenId) && j.p == input.size();
      return e;
    }
    if (!j.take(',')) return e;
  }
  return e;
}
static Response decodeHttp(std::string_view raw, bool provider, bool cleanClose) {
  Response r;
  auto split = raw.find("\r\n\r\n");
  if (split == raw.npos || split > kHeaderMax) return r;
  auto first = raw.find("\r\n");
  if (first == raw.npos) return r;
  auto line = raw.substr(0, first);
  if (line != "HTTP/1.1 200 OK" && line != "HTTP/1.0 200 OK") return r;
  size_t length = 0;
  bool hasLength = false, hasType = false, chunked = false;
  for (size_t pos = first + 2; pos < split;) {
    auto end = raw.find("\r\n", pos);
    if (end == raw.npos || end > split) return r;
    line = raw.substr(pos, end - pos);
    pos = end + 2;
    auto colon = line.find(':');
    if (colon == line.npos || colon == 0) return r;
    std::string key(line.substr(0, colon));
    for (char& c : key) {
      if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
      if (!asciiAlnum(c) && c != '-') return r;
    }
    auto value = line.substr(colon + 1);
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.remove_suffix(1);
    for (unsigned char c : value)
      if (c < 32 || c == 127) return r;
    if (key == "content-length") {
      if (hasLength || value.empty()) return r;
      hasLength = true;
      for (char c : value) {
        if (c < '0' || c > '9' || length > kResponseMax) return r;
        length = length * 10 + c - '0';
      }
      if (length > kResponseMax) return r;
    } else if (key == "content-type") {
      if (hasType || value.substr(0, value.find(';')) != "application/json") return r;
      hasType = true;
    } else if (provider && key == "transfer-encoding") {
      if (chunked || value != "chunked") return r;
      chunked=true;
    } else if (key == "transfer-encoding" || key == "location" || (key == "content-encoding" && value != "identity"))
      return r;
  }
  if (!hasType || (chunked && hasLength)) return r;
  auto body=raw.substr(split+4);
  if (!chunked) {
    if (body.size()>kResponseMax || (hasLength ? body.size()!=length : (!provider || !cleanClose))) return r;
    r.body=body;
  } else {
    // First pass validates complete framing without allocating body copies.
    size_t decoded=0, pos=0;
    unsigned chunks=0;
    auto next=[&](size_t& cursor, size_t& size) {
      auto end=body.find("\r\n",cursor);
      if(end==body.npos || end==cursor || end-cursor>8) return false;
      size=0;
      for(char c:body.substr(cursor,end-cursor)) {
        int n=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;
        if(n<0 || size>kResponseMax/16) return false;
        size=size*16+n;
      }
      cursor=end+2;
      return size<=body.size()-cursor && body.size()-cursor-size>=2 && body.substr(cursor+size,2)=="\r\n";
    };
    for (;;) {
      size_t size=0;
      if (++chunks>1024 || !next(pos,size) || size>kResponseMax-decoded) return r;
      decoded+=size; pos+=size+2;
      if(!size) { if(pos!=body.size()) return r; break; }
    }
    r.body.reserve(decoded);
    pos=0;
    for (;;) {
      size_t size=0;
      if (!next(pos,size)) return {}; // Same immutable framing validated above.
      if(!size) break;
      r.body.append(body.substr(pos,size)); pos+=size+2;
    }
  }
  r.ok=true;
  return r;
}
Response decodeResponse(std::string_view raw) { return decodeHttp(raw,false,false); }
Response decodeProviderResponse(std::string_view raw, bool cleanClose) {
  if(raw.size()>kResponseMax+kHeaderMax+4096) return {};
  return decodeHttp(raw,true,cleanClose);
}
}  // namespace stickychat
