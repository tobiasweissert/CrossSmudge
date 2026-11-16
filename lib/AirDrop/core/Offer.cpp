#include "Offer.h"
#include <cstring>
#include <limits>

namespace airdrop {
namespace {
constexpr uint64_t MaxObjects = 4096;
constexpr uint32_t MaxVisits = 16384;
constexpr uint32_t MaxDecodedStrings = 16384;
constexpr unsigned MaxDepth = 16;
bool alpha(unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool digit(unsigned char c) { return c >= '0' && c <= '9'; }
unsigned char lower(unsigned char c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
bool foldEqual(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) if (lower(a[i]) != lower(b[i])) return false;
  return true;
}
int hex(unsigned char c) {
  if (digit(c)) return c - '0';
  c = lower(c);
  return c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}
bool textCodepoint(uint32_t cp) {
  return cp >= 32 && cp != 127 && cp != 0x200e && cp != 0x200f && !(cp >= 0x80 && cp <= 0x9f) && cp != 0x2028 && cp != 0x2029 &&
         !(cp >= 0x202a && cp <= 0x202e) && !(cp >= 0x2066 && cp <= 0x2069) && cp != 0xfeff;
}
// Strict UTF-8, excluding controls and direction overrides in displayed metadata.
bool safeUtf8(std::string_view s) {
  size_t i = 0;
  while (i < s.size()) {
    uint32_t cp = static_cast<uint8_t>(s[i++]);
    unsigned extra = 0;
    uint32_t minimum = 0;
    if (cp < 0x80) { if (!textCodepoint(cp)) return false; continue; }
    if (cp >= 0xc2 && cp <= 0xdf) { cp &= 31; extra = 1; minimum = 0x80; }
    else if (cp >= 0xe0 && cp <= 0xef) { cp &= 15; extra = 2; minimum = 0x800; }
    else if (cp >= 0xf0 && cp <= 0xf4) { cp &= 7; extra = 3; minimum = 0x10000; }
    else return false;
    if (extra > s.size() - i) return false;
    while (extra--) { uint8_t c = s[i++]; if ((c & 0xc0) != 0x80) return false; cp = (cp << 6) | (c & 63); }
    if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff) || !textCodepoint(cp)) return false;
  }
  return true;
}
bool appendUtf8(uint32_t cp, char* dst, size_t cap, size_t& written) {
  if (!textCodepoint(cp) || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return false;
  unsigned count = cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
  if (written >= cap || count >= cap - written) return false;
  if (count == 1) dst[written++] = static_cast<char>(cp);
  else {
    dst[written++] = static_cast<char>((count == 2 ? 0xc0 : count == 3 ? 0xe0 : 0xf0) | (cp >> (6 * (count - 1))));
    for (unsigned shift = count - 1; shift; --shift) dst[written++] = static_cast<char>(0x80 | ((cp >> (6 * (shift - 1))) & 63));
  }
  dst[written] = 0;
  return true;
}
bool safeComponent(std::string_view name) {
  if (name.empty() || name.size() > 120 || name == "." || name == ".." || name.back() == ' ' || name.back() == '.' ||
      name.front() == ' ' || !safeUtf8(name)) return false;
  for (unsigned char c : name) if (c < 32 || c == 127 || std::strchr("/\\:*?\"<>|", c)) return false;
  auto stem = name.substr(0, name.find('.'));
  if (foldEqual(stem, "CON") || foldEqual(stem, "PRN") || foldEqual(stem, "AUX") || foldEqual(stem, "NUL")) return false;
  if (stem.size() == 4 && (foldEqual(stem.substr(0, 3), "COM") || foldEqual(stem.substr(0, 3), "LPT")) &&
      stem[3] >= '1' && stem[3] <= '9') return false;
  return true;
}
struct Object { uint8_t type; uint8_t low; uint64_t offset; uint64_t data; uint64_t count; uint64_t end; };
class Plist {
 public:
  Plist(const uint8_t* p, size_t n, OfferScratch& s) : bytes(p), size(n), scratch(s) {}
  bool init() {
    if (!bytes || size < 40 || size > MaxAskBytes || std::memcmp(bytes, "bplist00", 8)) return false;
    size_t t = size - 32;
    for (unsigned i = 0; i < 6; ++i) if (bytes[t + i]) return false;
    offsetWidth = bytes[t + 6]; refWidth = bytes[t + 7];
    if (!width(offsetWidth) || !width(refWidth)) return false;
    objects = be(t + 8, 8); root = be(t + 16, 8); table = be(t + 24, 8);
    if (!objects || objects > MaxObjects || root >= objects || table < 8 || table > t ||
        objects > (t - table) / offsetWidth || table + objects * offsetWidth != t) return false;
    uint64_t last = 7;
    for (uint64_t i = 0; i < objects; ++i) {
      uint64_t pos = offset(i);
      if (pos <= last || pos >= table) return false;
      last = pos;
    }
    std::memset(scratch.colors, 0, sizeof scratch.colors);
    scratch.visits = scratch.decodedStringBytes = 0;
    // Validate even unreachable objects; malformed data is never selectively ignored.
    for (uint64_t i = 0; i < objects; ++i) if (!walk(i)) return false;
    Object o{};
    return object(root, o) && o.type == 0xd;
  }
  uint64_t root = 0;
  bool object(uint64_t id, Object& o) const {
    if (id >= objects) return false;
    o.offset = offset(id); o.end = id + 1 < objects ? offset(id + 1) : table;
    o.type = bytes[o.offset] >> 4; o.low = bytes[o.offset] & 15; o.data = o.offset + 1; o.count = o.low;
    if (o.type == 0) return (o.low == 0 || o.low == 8 || o.low == 9) && o.data <= o.end;
    if (o.type == 1) { if (o.low > 3) return false; o.count = uint64_t(1) << o.low; }
    else if (o.type == 4 || o.type == 5 || o.type == 6 || o.type == 0xa || o.type == 0xd) {
      if (o.low == 15) {
        if (o.data >= o.end || (bytes[o.data] >> 4) != 1 || (bytes[o.data] & 15) > 3) return false;
        unsigned w = 1u << (bytes[o.data++] & 15);
        if (w > o.end - o.data) return false;
        o.count = be(o.data, w); o.data += w;
      }
    } else return false;
    uint64_t unit = o.type == 6 ? 2 : o.type == 0xa ? refWidth : o.type == 0xd ? 2 * refWidth : 1;
    return o.data <= o.end && o.count <= (o.end - o.data) / unit;
  }
  bool string(uint64_t id, char* out, size_t cap) const {
    Object o{}; if (!object(id, o) || (o.type != 5 && o.type != 6) || !cap) return false;
    size_t written = 0; out[0] = 0;
    if (o.type == 5) {
      if (o.count >= cap) return false;
      for (uint64_t i = 0; i < o.count; ++i) if (bytes[o.data + i] > 127 || !textCodepoint(bytes[o.data + i])) return false;
      std::memcpy(out, bytes + o.data, static_cast<size_t>(o.count)); out[o.count] = 0; return true;
    }
    for (uint64_t i = 0; i < o.count; ++i) {
      uint32_t cp = static_cast<uint32_t>(be(o.data + 2 * i, 2));
      if (cp >= 0xd800 && cp <= 0xdbff) {
        if (++i >= o.count) return false;
        uint32_t low = static_cast<uint32_t>(be(o.data + 2 * i, 2));
        if (low < 0xdc00 || low > 0xdfff) return false;
        cp = 0x10000 + ((cp - 0xd800) << 10) + low - 0xdc00;
      }
      if (!appendUtf8(cp, out, cap, written)) return false;
    }
    return true;
  }
  bool field(uint64_t dict, std::string_view key, uint64_t& value) {
    Object o{}; if (!object(dict, o) || o.type != 0xd) return false;
    for (uint64_t i = 0; i < o.count; ++i) {
      if (++scratch.visits > MaxVisits) return false;
      if (keyEquals(ref(o, i), key)) { value = ref(o, o.count + i); return true; }
    }
    return false;
  }
  uint64_t ref(const Object& o, uint64_t slot) const { return be(o.data + slot * refWidth, refWidth); }
  bool boolean(uint64_t id, bool expected) const { Object o{}; return object(id, o) && o.type == 0 && o.low == (expected ? 9 : 8); }
  bool integer(uint64_t id, uint64_t& n) const {
    Object o{}; if (!object(id, o) || o.type != 1) return false;
    n = be(o.data, static_cast<unsigned>(o.count)); return true;
  }
 private:
  const uint8_t* bytes; size_t size; OfferScratch& scratch;
  uint64_t objects = 0, table = 0; unsigned offsetWidth = 0, refWidth = 0;
  static bool width(unsigned n) { return n == 1 || n == 2 || n == 4 || n == 8; }
  uint64_t be(uint64_t pos, unsigned n) const { uint64_t v = 0; while (n--) v = (v << 8) | bytes[pos++]; return v; }
  uint64_t offset(uint64_t id) const { return be(table + id * offsetWidth, offsetWidth); }
  bool keyEquals(uint64_t id, std::string_view key) const {
    Object o{}; if (!object(id, o) || (o.type != 5 && o.type != 6) || o.count != key.size()) return false;
    for (size_t i = 0; i < key.size(); ++i) if (++scratch.visits > MaxVisits || be(o.data + i * (o.type == 6 ? 2 : 1), o.type == 6 ? 2 : 1) != static_cast<uint8_t>(key[i])) return false;
    return true;
  }
  bool sameKey(uint64_t a, uint64_t b) const {
    Object x{}, y{}; if (!object(a, x) || !object(b, y) || x.count != y.count) return false;
    for (uint64_t i = 0; i < x.count; ++i) {
      if (++scratch.visits > MaxVisits || be(x.data + i * (x.type == 6 ? 2 : 1), x.type == 6 ? 2 : 1) !=
          be(y.data + i * (y.type == 6 ? 2 : 1), y.type == 6 ? 2 : 1)) return false;
    }
    return true;
  }
  bool validateContents(const Object& o) {
    if (o.type == 5 || o.type == 6) {
      // Charge actual decoded UTF-8 bytes, without constructing a decoded copy.
      uint32_t decoded = 0;
      if (o.type == 5) {
        if (o.count > MaxDecodedStrings - scratch.decodedStringBytes) return false;
        for (uint64_t i = 0; i < o.count; ++i) {
          if (bytes[o.data + i] > 127 || !textCodepoint(bytes[o.data + i])) return false;
        }
        decoded = static_cast<uint32_t>(o.count);
      } else {
        for (uint64_t i = 0; i < o.count; ++i) {
          uint32_t cp = static_cast<uint32_t>(be(o.data + 2 * i, 2));
          if (cp >= 0xd800 && cp <= 0xdbff) {
            if (++i >= o.count) return false;
            uint32_t lo = static_cast<uint32_t>(be(o.data + 2 * i, 2));
            if (lo < 0xdc00 || lo > 0xdfff) return false;
            cp = 0x10000 + ((cp - 0xd800) << 10) + lo - 0xdc00;
          }
          if (!textCodepoint(cp) || (cp >= 0xd800 && cp <= 0xdfff)) return false;
          decoded += cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
          if (decoded > MaxDecodedStrings - scratch.decodedStringBytes) return false;
        }
      }
      scratch.decodedStringBytes += decoded;
    }
    if (o.type == 0xa || o.type == 0xd) {
      if (o.count > MaxObjects) return false;
      if (o.type == 0xd) {
        for (uint64_t i = 0; i < o.count; ++i) {
          Object key{};
          uint64_t kid = ref(o, i);
          if (!object(kid, key) || (key.type != 5 && key.type != 6)) return false;
          for (uint64_t j = 0; j < i; ++j) {
            if (++scratch.visits > MaxVisits || sameKey(kid, ref(o, j))) return false;
          }
        }
      }
    }
    return scratch.visits <= MaxVisits;
  }
  bool walk(uint64_t id) {
    if (++scratch.visits > MaxVisits || id >= objects) return false;
    if (scratch.colors[id] >= 2) return true;
    unsigned depth = 0;
    scratch.frames[0] = {};
    scratch.frames[0].id = static_cast<uint16_t>(id);
    for (;;) {
      auto& frame = scratch.frames[depth];
      if (!frame.entered) {
        if (scratch.colors[frame.id] == 1) return false;
        scratch.colors[frame.id] = 1;
        frame.entered = true;
        Object current{};
        if (!object(frame.id, current) || !validateContents(current)) return false;
      }
      Object current{};
      if (!object(frame.id, current)) return false;
      uint64_t children = current.type == 0xd ? 2 * current.count : current.type == 0xa ? current.count : 0;
      if (frame.child < children) {
        if (++scratch.visits > MaxVisits) return false;
        uint64_t child = ref(current, frame.child++);
        if (child >= objects || scratch.colors[child] == 1) return false;
        if (scratch.colors[child] >= 2) {
          unsigned height = scratch.colors[child] - 1;
          if (height > frame.height) frame.height = static_cast<uint8_t>(height);
          if (depth + frame.height > MaxDepth) return false;
        } else {
          if (depth == MaxDepth) return false;
          scratch.frames[++depth] = {};
          scratch.frames[depth].id = static_cast<uint16_t>(child);
        }
        continue;
      }
      if (depth + frame.height > MaxDepth) return false;
      scratch.colors[frame.id] = static_cast<uint8_t>(2 + frame.height);
      if (!depth) return true;
      uint8_t height = frame.height + 1;
      auto& parent = scratch.frames[--depth];
      if (height > parent.height) parent.height = height;
    }
  }
};
bool uuid(std::string_view id) {
  if (id.size() != 36) return false;
  for (size_t i = 0; i < id.size(); ++i) {
    if (i == 8 || i == 13 || i == 18 || i == 23) { if (id[i] != '-') return false; }
    else if (hex(id[i]) < 0) return false;
  }
  return true;
}
}  // namespace

bool validateUrl(std::string_view url, char* host, size_t capacity) {
  if (host && capacity) host[0] = 0;
  if (url.size() > 512 || url.substr(0,8) != "https://") return false;
  for (size_t i = 0; i < url.size(); ++i) {
    uint8_t c = url[i]; if (c < 33 || c > 126 || c == '\\') return false;
    if (c == '%') { if (i+2 >= url.size()) return false; int a=hex(url[i+1]), b=hex(url[i+2]); if(a<0 || b<0) return false; int v=16*a+b; if(v<32 || v==127 || v==92) return false; i+=2; }
  }
  auto tail=url.substr(8); auto end=tail.find_first_of("/?#"); auto h=tail.substr(0,end);
  if (h.empty() || h.size()>127 || h.find('.')==h.npos || h.front()=='.' || h.back()=='.') return false;
  size_t label=0;
  for(size_t i=0;i<h.size();++i) { uint8_t c=h[i]; if(c=='.') {if(!label || h[i-1]=='-')return false;label=0;} else {if(!alpha(c)&&!digit(c)&&c!='-')return false;if(!label&&c=='-')return false;if(++label>63)return false;} }
  if(!label || h.back()=='-')return false;
  auto tld=h.substr(h.rfind('.')+1); if(tld.size()<2)return false;for(uint8_t c:tld)if(!alpha(c))return false;
  auto path=end==tail.npos?std::string_view("/"):tail.substr(end); path=path.substr(0,path.find('#'));
  if(path.size()+(path.empty() || path.front()!='/' ? 1 : 0)>384)return false;
  if(!host || capacity<=h.size())return false;
  for(size_t i=0;i<h.size();++i) { host[i]=static_cast<char>(lower(h[i])); }
  host[h.size()]=0;return true;
}
bool safeFilename(std::string_view name) {
  if(!safeComponent(name))return false;
  if(name.front()=='.')return false;
  for(size_t i=1;i<name.size();++i)if(name[i]=='.'&&(name[i-1]==' '||name[i-1]=='.'))return false;
  return true;
}
// Fixed scalar/object descriptors use ~496 bytes in the host -Os stack report.
// Unlike a DOM or recursive graph walk, this frame does not grow with Ask size/depth;
// the session owns all large buffers and the explicit 17-frame graph stack.
bool parseOffer(const uint8_t* body, size_t size, Offer& out, OfferScratch& scratch, ParseError& error) {
  out = {}; error=ParseError::Graph; Plist p(body,size,scratch);if(!p.init())return false;
  uint64_t sender=0; if(!p.field(p.root,"SenderComputerName",sender) || !p.string(sender,out.sender,sizeof out.sender) || !out.sender[0]) {error=ParseError::Metadata;return false;}
  uint64_t tid=0;
  if(p.field(p.root,"TransferID",tid)) {uint64_t id=0;if(!p.field(tid,"id",id)||!p.string(id,out.transferId,sizeof out.transferId)||!uuid(out.transferId)){error=ParseError::Metadata;return false;}}
  uint64_t files=0,items=0; Object f{},u{};
  bool hasFiles=p.field(p.root,"Files",files),hasItems=p.field(p.root,"Items",items);
  if((hasFiles&&(!p.object(files,f)||f.type!=0xa)) || (hasItems&&(!p.object(items,u)||u.type!=0xa)))return false;
  if((f.count&&u.count)||(!f.count&&!u.count)||f.count>1||u.count>1){error=ParseError::Unsupported;return false;}
  uint64_t tt=0,typ=0;Object type{};
  if(p.field(p.root,"TransferType",tt)) {
    if(!p.object(tt,type)||type.type!=0xd||type.count!=1||!p.field(tt,u.count?"links":"files",typ)||!p.object(typ,type)||type.type!=0xd||type.count){error=ParseError::Unsupported;return false;}
  } else if(u.count) {error=ParseError::Unsupported;return false;}
  if(u.count) {
    error=ParseError::Url;
    if(!p.string(p.ref(u,0),out.url,sizeof out.url)||!validateUrl(out.url,out.host,sizeof out.host))return false;
    out.kind=OfferKind::Url;
  } else {
    uint64_t file=p.ref(f,0),v=0; error=ParseError::Metadata;
    if(!p.field(file,"FileName",v)||!p.string(v,out.filename,sizeof out.filename)||!safeFilename(out.filename)){error=ParseError::Filename;return false;}
    auto name=std::string_view(out.filename);
    // Sender FileType does not gate receipt; all regular payloads are opaque.
    uint64_t amount=0;if(!p.field(file,"FileSize",v)||!p.integer(v,amount)||amount>MaxFileBytes){error=ParseError::Size;return false;}out.fileSize=static_cast<uint32_t>(amount);
    if(!p.field(file,"FileIsDirectory",v)||!p.boolean(v,false)){error=ParseError::Unsupported;return false;}
    if(p.field(file,"ShouldConvertMediaFormats",v)&&!p.boolean(v,false)){error=ParseError::Unsupported;return false;}
    if(!p.field(file,"FileBomPath",v)||!p.string(v,out.archivePath,sizeof out.archivePath)){error=ParseError::Filename;return false;}
    auto path=std::string_view(out.archivePath);if(path.substr(0,2)!="./"){error=ParseError::Filename;return false;}path.remove_prefix(2);
    // Initial archive policy admits a root metadata directory, never sender wrappers.
    // Reject rather than strip a path; decoder binds the exact accepted ./basename.
    if(path!=name || path.find('/')!=path.npos){error=ParseError::Filename;return false;}
    if(p.field(p.root,"ConvertMediaFormats",v)&&!p.boolean(v,false)){error=ParseError::Unsupported;return false;}
    out.kind=OfferKind::File;
  }
  if(scratch.visits>MaxVisits){error=ParseError::Graph;out.kind=OfferKind::None;return false;}
  error=ParseError::None;return true;
}
}  // namespace airdrop
