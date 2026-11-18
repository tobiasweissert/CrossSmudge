#pragma once
#include <cstddef>
#include <cstdint>
// Synthetic SD handle: operations hold the simulated SPI lock only while in I/O.
extern bool ioLocked;
extern unsigned writeCalls,readCalls;
extern size_t shortWrite;
extern int readResult;
extern int64_t clockUs,ioCostUs;
struct HalFile {
 bool valid=true;
 explicit operator bool() const{return valid;}
 size_t write(const uint8_t*,size_t n){ioLocked=true;++writeCalls;clockUs+=ioCostUs;ioLocked=false;return shortWrite<n?shortWrite:n;}
 int read(uint8_t*,size_t n){ioLocked=true;++readCalls;clockUs+=ioCostUs;ioLocked=false;return readResult<0?readResult:int(n);}
};
