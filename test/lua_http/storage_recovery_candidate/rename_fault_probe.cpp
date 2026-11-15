
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#define USE_LONG_FILE_NAMES 1
#define DBG_FAIL_MACRO do {} while(0)
constexpr int O_CREAT=1,O_EXCL=2,O_WRONLY=4;
constexpr int FILE_ATTR_CLOSED=0,FILE_ATTR_FILE=1,FILE_FLAG_WRITE=1;
using Cluster_t=uint32_t; using Sector_t=uint32_t;
struct DirFat_t { char name[11]; uint8_t attributes; uint8_t flags; uint8_t createTimeMs; uint32_t cluster; };
struct FsCache { enum {CACHE_FOR_READ, CACHE_FOR_WRITE}; };
struct Volume {
  DirFat_t entries[3]{}; bool present[3]={false,true,false};
  bool clusterFreed=false; int failure=0;
  bool cacheSync(){return failure!=4;}
  uint32_t clusterStartSector(uint32_t n){return n;}
  uint8_t* dataCachePrepare(uint32_t,int){return nullptr;}
  bool freeChain(uint32_t){clusterFreed=true;return true;}
};
class FatFile {
 public:
  Volume* m_vol=nullptr; uint32_t m_dirSector=1,m_firstCluster=7,m_dirCluster=0;
  int m_dirIndex=0,m_lfnOrd=0,m_attributes=1,m_flags=1;
  bool isFile(){return m_attributes==1;}
  bool isSubDir(){return false;}
  bool isLFN(){return false;}
  bool sync(){return m_vol->failure!=5;}
  void copy(FatFile* f){*this=*f;}
  DirFat_t* cacheDirEntry(int mode){
    if(m_vol->failure==1 && mode==FsCache::CACHE_FOR_READ)return nullptr;
    if(m_vol->failure==2 && mode==FsCache::CACHE_FOR_WRITE)return nullptr;
    return &m_vol->entries[m_dirSector];
  }
  bool open(FatFile* dir,const char*,int){m_vol=dir->m_vol;m_dirSector=2;m_vol->present[2]=true;return true;}
  bool mkdir(FatFile*,const char*,bool){return false;}
  bool remove(){
    if(m_vol->failure==3)return false;
    m_vol->present[m_dirSector]=false;
    if(m_firstCluster)m_vol->clusterFreed=true;
    return true;
  }
  bool rename(FatFile*,const char*);
};

bool FatFile::rename(FatFile* dirFile, const char* newPath) {
  DirFat_t entry;
  Cluster_t dirCluster = 0;
  FatFile file;
  FatFile oldFile;
  uint8_t* pc;
  DirFat_t* dir;

  // Must be an open file or subdirectory.
  if (!(isFile() || isSubDir())) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // Can't rename LFN in 8.3 mode.
  if (!USE_LONG_FILE_NAMES && isLFN()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // Can't move file to new volume.
  if (m_vol != dirFile->m_vol) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // sync() and cache directory entry
  sync();
  oldFile.copy(this);
  dir = cacheDirEntry(FsCache::CACHE_FOR_READ);
  if (!dir) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // save directory entry
  memcpy(&entry, dir, sizeof(entry));
  // make directory entry for new path
  if (isFile()) {
    if (!file.open(dirFile, newPath, O_CREAT | O_EXCL | O_WRONLY)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  } else {
    // don't create missing path prefix components
    if (!file.mkdir(dirFile, newPath, false)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    // save cluster containing new dot dot
    dirCluster = file.m_firstCluster;
  }
  // change to new directory entry

  m_dirSector = file.m_dirSector;
  m_dirIndex = file.m_dirIndex;
  m_lfnOrd = file.m_lfnOrd;
  m_dirCluster = file.m_dirCluster;
  // mark closed to avoid possible destructor close call
  file.m_attributes = FILE_ATTR_CLOSED;
  file.m_flags = 0;

  // cache new directory entry
  dir = cacheDirEntry(FsCache::CACHE_FOR_WRITE);
  if (!dir) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // copy all but name and name flags to new directory entry
  memcpy(&dir->createTimeMs, &entry.createTimeMs,
         sizeof(entry) - sizeof(dir->name) - 2);
  dir->attributes = entry.attributes;

  // update dot dot if directory
  if (dirCluster) {
    // get new dot dot
    Sector_t sector = m_vol->clusterStartSector(dirCluster);
    pc = m_vol->dataCachePrepare(sector, FsCache::CACHE_FOR_READ);
    dir = reinterpret_cast<DirFat_t*>(pc);
    if (!dir) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    memcpy(&entry, &dir[1], sizeof(entry));

    // free unused cluster
    if (!m_vol->freeChain(dirCluster)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    // store new dot dot
    sector = m_vol->clusterStartSector(m_firstCluster);
    pc = m_vol->dataCachePrepare(sector, FsCache::CACHE_FOR_WRITE);
    dir = reinterpret_cast<DirFat_t*>(pc);
    if (!dir) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    memcpy(&dir[1], &entry, sizeof(entry));
  }
  // Remove old directory entry;
  oldFile.m_firstCluster = 0;
  oldFile.m_flags = FILE_FLAG_WRITE;
  oldFile.m_attributes = FILE_ATTR_FILE;
  if (!oldFile.remove()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  return m_vol->cacheSync();

fail:
  return false;
}

int main(){
  for(int failure=0;failure<=5;++failure){
    Volume v;v.failure=failure;v.entries[1].cluster=7;
    FatFile part,root;part.m_vol=&v;root.m_vol=&v;
    bool result=part.rename(&root,"0");
    if(failure==0){assert(result && !v.present[1] && v.present[2] && v.entries[2].cluster==7);}
    if(failure==1){assert(!result && v.present[1] && !v.present[2]);}
    if(failure==2){assert(!result && v.present[1] && v.present[2] && v.entries[2].cluster==0);}
    if(failure==3){
      assert(!result && v.present[1] && v.present[2] && v.entries[2].cluster==7);
      // Simulate a fresh path-open followed by abort's remove("part").
      // Both directory entries now identify the same data cluster chain.
      FatFile cleanup;cleanup.m_vol=&v;cleanup.m_dirSector=1;cleanup.m_firstCluster=7;
      v.failure=0;assert(cleanup.remove());
      assert(v.clusterFreed && v.present[2] && v.entries[2].cluster==7);
    }
    if(failure==4){assert(!result && !v.present[1] && v.present[2] && v.entries[2].cluster==7);}
    if(failure==5){assert(result && !v.present[1] && v.present[2]);} // initial sync() return ignored
    std::cout << "case=" << failure << " result=" << result << " source=" << v.present[1]
      << " dest=" << v.present[2] << " destCluster=" << v.entries[2].cluster
      << " clusterFreed=" << v.clusterFreed << " PASS\n";
  }
}
