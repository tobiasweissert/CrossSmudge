#include <AppCapabilities.h>
#if CROSSINK_APP_CAP_AIRDROP
#include <vendor/ESP32Drop/src/airdrop/core/ad_dnssd.h>
#include <vendor/ESP32Drop/src/awdl/port/awdl_port_esp32.h>

#include <cstdio>
#include <cstring>

#include "Discovery.h"
namespace airdrop::discovery {
namespace {
// Pinned ESP32Drop DNS name/checksum/message construction adapted to advertise
// only the implemented AirDrop service. Internal RAM: raw WiFi TX path must not
// borrow display buffers or allocate in callbacks. One frame-task writer only.
AwdlIdentity identity{};
uint8_t frame[AWDL_TX_HEADROOM + 1280]{};
uint8_t dns[1024]{};
uint8_t* name(uint8_t* p, const char* value) {
  while (*value) {
    const char* dot = strchr(value, '.');
    size_t n = dot ? size_t(dot - value) : strlen(value);
    *p++ = uint8_t(n);
    memcpy(p, value, n);
    p += n;
    if (!dot) break;
    value = dot + 1;
  }
  *p++ = 0;
  return p;
}
uint16_t checksum(const uint8_t* src, const uint8_t* dst, const uint8_t* udp, int n) {
  uint32_t sum = 17 + n;
  for (int i = 0; i < 16; i += 2) {
    sum += (src[i] << 8) | src[i + 1];
    sum += (dst[i] << 8) | dst[i + 1];
  }
  for (int i = 0; i + 1 < n; i += 2) sum += (udp[i] << 8) | udp[i + 1];
  if (n & 1) sum += udp[n - 1] << 8;
  while (sum >> 16) sum = (sum & 65535) + (sum >> 16);
  return uint16_t(~sum) ? uint16_t(~sum) : 65535;
}
size_t message() {
  memset(dns, 0, 12);
  dns[2] = 0x84;
  dns[7] = 6;
  uint8_t* p = dns + 12;
  char instance[48], host[24];
  snprintf(instance, sizeof instance, "%s._airdrop._tcp.local", identity.instance);
  snprintf(host, sizeof host, "%s.local", identity.instance);
  p = name(p, "_airdrop._tcp.local");
  *p++ = 0;
  *p++ = 12;
  *p++ = 0;
  *p++ = 1;
  *p++ = 0;
  *p++ = 0;
  *p++ = 0;
  *p++ = 120;
  auto* len = p;
  p += 2;
  auto* start = p;
  p = name(p, instance);
  len[0] = uint8_t((p - start) >> 8);
  len[1] = uint8_t(p - start);
  p = name(p, instance);
  *p++ = 0;
  *p++ = 33;
  *p++ = 0x80;
  *p++ = 1;
  *p++ = 0;
  *p++ = 0;
  *p++ = 0;
  *p++ = 120;
  len = p;
  p += 2;
  start = p;
  *p++ = 0;
  *p++ = 0;
  *p++ = 0;
  *p++ = 0;
  *p++ = 0x22;
  *p++ = 0x42;
  p = name(p, host);
  len[0] = uint8_t((p - start) >> 8);
  len[1] = uint8_t(p - start);
  p = name(p, instance);
  *p++ = 0;
  *p++ = 16;
  *p++ = 0x80;
  *p++ = 1;
  *p++ = 0;
  *p++ = 0;
  *p++ = 0;
  *p++ = 120;
  *p++ = 0;
  *p++ = 11;
  *p++ = 10;
  memcpy(p, "flags=0131", 10);
  p += 10;
  p = name(p, host);
  *p++ = 0;
  *p++ = 28;
  *p++ = 0x80;
  *p++ = 1;
  *p++ = 0;
  *p++ = 0;
  *p++ = 0;
  *p++ = 120;
  *p++ = 0;
  *p++ = 16;
  memcpy(p, identity.ll, 16);
  p += 16;
  // Pinned foundation NSEC records make IPv6-only host resolution explicit.
  p = name(p, host);
  *p++ = 0;
  *p++ = 47;
  *p++ = 0x80;
  *p++ = 1;
  *p++ = 0;
  *p++ = 0;
  *p++ = 0;
  *p++ = 120;
  len = p;
  p += 2;
  start = p;
  p = name(p, host);
  static const uint8_t hostTypes[] = {0, 4, 0, 0, 0, 8};
  memcpy(p, hostTypes, sizeof hostTypes);
  p += sizeof hostTypes;
  len[0] = uint8_t((p - start) >> 8);
  len[1] = uint8_t(p - start);
  p = name(p, instance);
  *p++ = 0;
  *p++ = 47;
  *p++ = 0x80;
  *p++ = 1;
  *p++ = 0;
  *p++ = 0;
  *p++ = 0;
  *p++ = 120;
  len = p;
  p += 2;
  start = p;
  p = name(p, instance);
  static const uint8_t instanceTypes[] = {0, 5, 0, 0, 0x80, 0, 0x40};
  memcpy(p, instanceTypes, sizeof instanceTypes);
  p += sizeof instanceTypes;
  len[0] = uint8_t((p - start) >> 8);
  len[1] = uint8_t(p - start);
  return size_t(p - dns);
}
// Validate the complete bounded query before deciding relevance. The preserved
// upstream asks_us helper deliberately answers malformed/foreign queries; that
// diagnostic fail-open policy is not our receiver admission policy.
bool queryName(const uint8_t* m, size_t size, size_t start, size_t& end, unsigned& work) {
  size_t p = start;
  bool jumped = false;
  unsigned pointers = 0, expanded = 0;
  while (p < size && work) {
    --work;
    uint8_t n = m[p];
    if (!n) {
      if (!jumped) end = p + 1;
      return expanded < 255;
    }
    if ((n & 0xc0) == 0xc0) {
      if (p + 1 >= size || ++pointers > 16) return false;
      size_t target = (size_t(n & 63) << 8) | m[p + 1];
      if (target < 12 || target >= p) return false;
      if (!jumped) {
        end = p + 2;
        jumped = true;
      }
      p = target;
      continue;
    }
    if (n > 63 || n > size - p - 1 || expanded + 1 + n > 255) return false;
    expanded += 1 + n;
    p += 1 + n;
  }
  return false;
}
bool queryIsOurs(const uint8_t* m, size_t size) {
  if (size < 12 || size > 1232 || m[0] || m[1] || m[2] || m[3]) return false;
  unsigned questions = (unsigned(m[4]) << 8) | m[5];
  unsigned records = ((unsigned(m[6]) << 8) | m[7]) + ((unsigned(m[8]) << 8) | m[9]) + ((unsigned(m[10]) << 8) | m[11]);
  if (!questions || questions > 16 || records > 64) return false;
  size_t p = 12;
  unsigned work = 8192;
  bool relevant = false;
  char instance[48], host[24];
  snprintf(instance, sizeof instance, "%s._airdrop._tcp.local", identity.instance);
  snprintf(host, sizeof host, "%s.local", identity.instance);
  for (unsigned i = 0; i < questions; ++i) {
    size_t start = p, next = 0;
    if (!queryName(m, size, p, next, work) || next > size || size - next < 4) return false;
    p = next;
    unsigned type = (unsigned(m[p]) << 8) | m[p + 1];
    unsigned klass = ((unsigned(m[p + 2]) << 8) | m[p + 3]) & 0x7fff;
    p += 4;
    if (klass != 1) return false;
    bool ownService = ad_dns_name_eq(m, int(size), int(start), "_airdrop._tcp.local") && (type == 12 || type == 255);
    bool ownHost =
        ad_dns_name_eq(m, int(size), int(start), host) && (type == 1 || type == 28 || type == 47 || type == 255);
    bool ownInstance = ad_dns_name_eq(m, int(size), int(start), instance) &&
                       (type == 1 || type == 16 || type == 28 || type == 33 || type == 47 || type == 255);
    relevant = relevant || ownService || ownHost || ownInstance;
  }
  for (unsigned i = 0; i < records; ++i) {
    size_t next = 0;
    if (!queryName(m, size, p, next, work) || next > size || size - next < 10) return false;
    unsigned type = (unsigned(m[next]) << 8) | m[next + 1];
    size_t data = (size_t(m[next + 8]) << 8) | m[next + 9];
    if (data > size - next - 10) return false;
    size_t start = next + 10, end = start + data, tail = 0;
    if (type == 12) {
      if (!queryName(m, size, start, tail, work) || tail != end) return false;
    } else if (type == 33) {
      if (data < 7 || !queryName(m, size, start + 6, tail, work) || tail != end) return false;
    } else if (type == 47) {
      if (!queryName(m, size, start, tail, work) || tail >= end) return false;
      int previous = -1;
      while (tail < end) {
        if (end - tail < 2) return false;
        unsigned window = m[tail++], bytes = m[tail++];
        if (int(window) <= previous || !bytes || bytes > 32 || bytes > end - tail || m[tail + bytes - 1] == 0)
          return false;
        previous = int(window);
        tail += bytes;
      }
    } else if (type == 1 && data != 4)
      return false;
    else if (type == 28 && data != 16)
      return false;
    else if (type == 16) {
      tail = start;
      while (tail < end) {
        unsigned bytes = m[tail++];
        if (bytes > end - tail) return false;
        tail += bytes;
      }
    }
    p = end;
  }
  return relevant && p == size;
}
void receive(void*, const uint8_t peer[6], const uint8_t* ip, size_t length) {
  static const uint8_t mdns[16] = {0xff, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xfb};
  if (length < 60 || (ip[0] >> 4) != 6 || ip[6] != 17 || ip[7] != 255 || ip[8] != 0xfe || (ip[9] & 0xc0) != 0x80)
    return;
  const size_t udpLength = (size_t(ip[44]) << 8) | ip[45];
  const size_t payload = (size_t(ip[4]) << 8) | ip[5];
  if (udpLength < 20 || udpLength != payload || udpLength != length - 40 || ip[42] != 0x14 || ip[43] != 0xe9 ||
      (!ip[46] && !ip[47]))
    return;
  if (memcmp(ip + 24, mdns, 16) && memcmp(ip + 24, identity.ll, 16)) return;
  if (checksum(ip + 8, ip + 24, ip + 40, int(udpLength)) != 65535) return;
  if (!queryIsOurs(ip + 48, udpLength - 8)) return;
  const size_t n = message();
  if (n > 1024) return;
  auto* out = frame + AWDL_TX_HEADROOM;
  memset(out, 0, 48);
  out[0] = 0x60;
  size_t u = n + 8;
  out[4] = uint8_t(u >> 8);
  out[5] = uint8_t(u);
  out[6] = 17;
  out[7] = 255;
  memcpy(out + 8, identity.ll, 16);
  static const uint8_t multicast[16] = {0xff, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xfb};
  memcpy(out + 24, multicast, 16);
  auto* udp = out + 40;
  udp[0] = udp[2] = 0x14;
  udp[1] = udp[3] = 0xe9;
  udp[4] = uint8_t(u >> 8);
  udp[5] = uint8_t(u);
  memcpy(udp + 8, dns, n);
  auto c = checksum(out + 8, out + 24, udp, int(u));
  udp[6] = uint8_t(c >> 8);
  udp[7] = uint8_t(c);
  (void)awdl_tx_ip6_now(peer, frame, n + 48);
}
}  // namespace
bool start() {
  awdl_identity(&identity);
  return awdl_rx_tap_set(receive, nullptr) == 0;
}
bool detach() { return awdl_rx_tap_set(nullptr, nullptr) == 0; }
}  // namespace airdrop::discovery
#endif
