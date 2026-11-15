#!/usr/bin/env python3
"""Compile the exact pinned FatFile::rename body against instrumented seams.
This is a source-level control-flow test, NOT an SD driver/power-loss test.
"""
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path('/root/github/crosssmudge-lua-http')
SOURCE = ROOT / '.pio/libdeps/sticky/SdFat/src/FatLib/FatFile.cpp'
OUT = Path(__file__).resolve().parent
text = SOURCE.read_text()
start = text.index('bool FatFile::rename(FatFile* dirFile, const char* newPath) {')
end = text.index('\n//------------------------------------------------------------------------------', start)
method = text[start:end]
prefix = r'''
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
'''
suffix = r'''
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
'''
generated = OUT / 'rename_fault_probe.cpp'
generated.write_text(prefix + '\n' + method + '\n' + suffix)
binary = Path(os.environ.get('TMPDIR', '/root/.hermes/cache/scratch')) / 'crosssmudge-rename-fault-probe'
command = ['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', str(generated), '-o', str(binary)]
compile_run = subprocess.run(command, capture_output=True, text=True)
(OUT / 'compile.log').write_text(compile_run.stdout + compile_run.stderr)
if compile_run.returncode:
    raise SystemExit(compile_run.returncode)
run = subprocess.run([str(binary)], capture_output=True, text=True)
(OUT / 'run.log').write_text(run.stdout + run.stderr)
receipt = {
    'kind': 'exact pinned method with instrumented dependency seams; not physical FAT/SD qualification',
    'source': str(SOURCE), 'source_sha256': hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
    'method_sha256': hashlib.sha256(method.encode()).hexdigest(),
    'generated_sha256': hashlib.sha256(generated.read_bytes()).hexdigest(),
    'compile_command': command, 'compile_exit': compile_run.returncode,
    'run_exit': run.returncode, 'asserted_cases': 6,
    'cases': ['success', 'source-cache failure', 'destination-cache failure after create',
              'source-remove failure with alias plus unsafe abort cleanup', 'final-cache-sync failure after unlink',
              'initial-sync failure ignored by pinned method'],
}
(OUT / 'receipt.json').write_text(json.dumps(receipt, indent=2))
print(run.stdout, end='')
print(json.dumps(receipt, indent=2))
raise SystemExit(run.returncode)
