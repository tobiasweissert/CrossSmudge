#pragma once
// Test-only host block device and adapter; never linked into firmware.
#include "SealStage.h"
#include "FatLib/FatVolume.h"
#include "FatLib/FatFormatter.h"
#include <array>
#include <map>
#include <cstring>
struct BlockDisk : FsBlockDevice {
  std::map<Sector_t,std::array<uint8_t,512>> sectors;
  unsigned writes=0,failAt=0; bool mutateOnFailure=false;
  bool isBusy() override { return false; }
  Sector_t sectorCount() override { return 32768; }
  bool syncDevice() override { return true; }
  bool readSector(Sector_t s,uint8_t* p) override {
    if(s>=sectorCount()) return false;
    auto it=sectors.find(s);
    if(it==sectors.end()) std::memset(p,0,512); else std::memcpy(p,it->second.data(),512);
    return true;
  }
  bool readSectors(Sector_t s,uint8_t* p,size_t n) override {
    for(size_t i=0;i<n;++i) if(!readSector(s+i,p+512*i)) return false;
    return true;
  }
  bool writeSector(Sector_t s,const uint8_t* p) override {
    if(s>=sectorCount()) return false;
    bool bad=++writes==failAt;
    if(!bad||mutateOnFailure) std::memcpy(sectors[s].data(),p,512);
    return !bad;
  }
  bool writeSectors(Sector_t s,const uint8_t* p,size_t n) override {
    for(size_t i=0;i<n;++i) if(!writeSector(s+i,p+512*i)) return false;
    return true;
  }
};
struct SdFatFiles : recoverable::SealFiles {
  FatVolume& volume; FatFile writer;
  explicit SdFatFiles(FatVolume& v):volume(v) {}
  static const char* path(unsigned s) { return s ? "slot1.dat" : "slot0.dat"; }
  recoverable::SlotState size(unsigned s,uint64_t& n) override {
    if(s>1) return recoverable::SlotState::uncertain;
    FatFile file;
    if(!file.open(&volume,path(s),O_RDONLY)) return recoverable::SlotState::uncertain;
    n=file.fileSize(); bool ok=file.close();
    return ok?recoverable::SlotState::valid:recoverable::SlotState::uncertain;
  }
  bool read(unsigned s,uint64_t o,uint8_t* p,size_t n) override {
    if(s>1||o>UINT32_MAX) return false;
    FatFile file; if(!file.open(&volume,path(s),O_RDONLY)) return false;
    bool ok=file.seekSet(uint32_t(o)) && file.read(p,n)==int(n);
    bool closed=file.close(); return ok&&closed;
  }
  bool reset(unsigned s) override { return s<=1 && writer.open(&volume,path(s),O_RDWR|O_TRUNC); }
  bool openSeal(unsigned s) override { return s<=1 && writer.open(&volume,path(s),O_RDWR); }
  bool write(unsigned,uint64_t o,const uint8_t* p,size_t n) override {
    return o<=UINT32_MAX && writer.seekSet(uint32_t(o)) && writer.write(p,n)==n;
  }
  bool sync(unsigned) override { return writer.sync(); }
  bool close(unsigned) override { return writer.close(); }
};
