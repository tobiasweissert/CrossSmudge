#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace dictation {
constexpr size_t maxPcm = 5 * 16000 * 2;  // five seconds, mono PCM16
constexpr size_t maxText = 3072, maxKey = 256;
constexpr char setupUrl[] = "https://platform.openai.com/api-keys";
inline void wipe(void* p, size_t n) {
  auto q = static_cast<volatile unsigned char*>(p);
  while (n--) *q++ = 0;
}
inline bool validKey(const char* p, size_t n) {
  if (!n || n > maxKey) return false;
  for (size_t i = 0; i < n; ++i)
    if (p[i] < 33 || static_cast<unsigned char>(p[i]) > 126) return false;
  return true;
}
inline const char* statusError(int n) {
  if (n == 200) return nullptr;
  if (n == 401 || n == 403) return "Key rejected. Check project permissions.";
  if (n == 429) return "Quota or rate limit. Check API billing.";
  return "Upload failed. Not retried.";
}
inline bool wav(uint8_t* out, size_t bytes) {
  if (!bytes || bytes % 2 || bytes > maxPcm) return false;
  const uint8_t header[44] = {'R', 'I', 'F', 'F', 0,  0, 0,   0,   'W', 'A', 'V', 'E', 'f', 'm', 't',
                              ' ', 16,  0,   0,   0,  1, 0,   1,   0,   128, 62,  0,   0,   0,   125,
                              0,   0,   2,   0,   16, 0, 'd', 'a', 't', 'a', 0,   0,   0,   0};
  memcpy(out, header, 44);
  for (unsigned i = 0; i < 4; i++) {
    out[4 + i] = (36 + bytes) >> (8 * i);
    out[40 + i] = bytes >> (8 * i);
  }
  return true;
}
}  // namespace dictation
