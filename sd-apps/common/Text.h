#ifndef CP_TEXT_H
#define CP_TEXT_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
static size_t cp_length(const char* s, size_t cap) { size_t n=0; if(s) while(n<cap && s[n]) ++n; return n; }
static int cp_copy(char* out, size_t cap, const char* in) {
  size_t n=cp_length(in,cap); if(!in || !cap || n==cap) return 0;
  memcpy(out,in,n+1); return 1;
}
#endif
