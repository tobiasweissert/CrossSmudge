#!/usr/bin/env python3
"""Bounded read-only-source review probe; no SD/native-FAT qualification.
Execute exact production BookmarkStore begin/rollback/commit/write bodies.
Derived from the retained reviewer probe; real codecs now execute.
Path identities and storage are adapters; failing I/O is a plain false/no-mutation
backup rename or a short write, NOT a speculative FAT alias model.
"""
from pathlib import Path
import fcntl, subprocess, tempfile, json, hashlib
ROOT=Path(__file__).resolve().parents[2]
import os, sys
OUT=Path(os.environ.get('NESTED_LOG', str(Path(os.environ.get('TMPDIR','/root/.hermes/cache/scratch'))/'crosscub-nested.log')))
def method(path, signature):
    text=(ROOT/path).read_text(); start=text.index(signature)
    return text[start:text.index('\n}',start)+2]
preamble=r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#define private public
#include "src/BookmarkStore.h"
#undef private
#define LOG_ERR(...) ((void)0)
#define LOG_INF(...) ((void)0)
constexpr uint8_t LEGACY_VERSION=2, COUNT_U16_VERSION=3, PARAGRAPH_ANCHOR_VERSION=4, VERSION=5;
constexpr size_t MAX_BOOKMARKS=1024;
constexpr char BOOKMARKS_DIR[]="/.crosspoint/bookmarks";
std::map<std::string,std::string> disk;
bool failBackup=false, shortWrite=false;
std::string failRenameTo; bool mutateRenameFailure=false, corruptRead=false;
int failWriteAt=0, writeCalls=0; bool failSync=false, failRead=false;
int shortWrites=0;
std::string inputFaultPath; size_t inputFaultPosition=0; int inputFaultReturn=-1, inputFaultHits=0;
bool inputFault=false; int inputReadsAfterFault=0, mutations=0;
struct FsFile {
 std::string path; size_t pos=0;
 bool sync(){return !failSync;}
 int available(){return disk[path].size()-pos;}
 int read(void* data,size_t n){if(path==inputFaultPath){if(inputFaultHits)++inputReadsAfterFault;if(inputFault && pos==inputFaultPosition){inputFault=false;++inputFaultHits;if(inputFaultReturn<0)return -1;size_t partial=n?n-1:0;memcpy(data,disk[path].data()+pos,partial);pos+=partial;return partial;}}if(failRead && path=="/current/Target/A.epub")return 0; n=std::min(n,disk[path].size()-pos); memcpy(data,disk[path].data()+pos,n);if(corruptRead && path=="/current/Target/A.epub" && n) static_cast<char*>(data)[0]^=1;pos+=n;return n;}
 size_t write(const uint8_t* data,size_t n) {
   if(shortWrite){++shortWrites;return 0;}
   if(++writeCalls==failWriteAt){++shortWrites;size_t partial=n?n-1:0;disk[path].append(reinterpret_cast<const char*>(data),partial);return partial;}
   ++mutations;disk[path].append(reinterpret_cast<const char*>(data),n);return n;
 }
 explicit operator bool()const{return !path.empty();}
 size_t position(){return pos;}
 bool seek(size_t n){if(n>disk[path].size())return false;pos=n;return true;}
 bool seekCur(size_t n){return seek(pos+n);}
 void close(){}
};
constexpr int O_WRONLY=1,O_CREAT=2,O_TRUNC=4;
struct StorageAdapter {
 FsFile open(const char*p,int){disk[p]="";FsFile f;f.path=p;return f;}
 bool exists(const char*p){return disk.count(p);}
 bool mkdir(const char*){return true;}
 bool remove(const char*p){++mutations;return disk.erase(p)>0;}
 bool rename(const char*a,const char*b){++mutations;
   if(std::string(b)==failRenameTo){if(mutateRenameFailure && disk.count(a))disk[b]=disk[a];return false;}
   if(failBackup && std::string(b).find(".rename.bak")!=std::string::npos)return false;
   if(!disk.count(a)||disk.count(b))return false;
   disk[b]=disk[a];disk.erase(a);return true;
 }
 bool openFileForRead(const char*,const std::string&p,FsFile&f){if(!disk.count(p))return false;f.path=p;f.pos=0;return true;}
 bool openFileForWrite(const char*,const std::string&p,FsFile&f){++mutations;disk[p]="";f.path=p;return true;}
} Storage;
// Deliberately simple, distinct path identity adapter; no hash collisions.
std::string currentStoreFilePathForBook(const std::string&p,const std::string&){return "/current"+p;}
std::string legacyStoreFilePathForBook(const std::string&p,const std::string&){return "/legacy"+p;}
'''
source=preamble+'\n'+(ROOT/'lib/Serialization/Serialization.h').read_text().replace('#pragma once','').replace('#include <HalStorage.h>','')
for sig in ['bool readBookmarkString(', 'bool readBookmarkCount(', 'bool BookmarkStore::readFromFile(const std::string&', 'bool bookmarksMatchIdentity(', 'bool mergeBookmarks(', 'bool deleteBookmarkStorePath(', 'bool BookmarkStore::writeToFile(', 'bool BookmarkStore::beginRenameMigration(', 'bool BookmarkStore::commitRenameMigration(', 'bool BookmarkStore::rollbackRenameMigration(']:
    source+='\n'+method('src/BookmarkStore.cpp',sig)+'\n'
source+=r'''
struct Epub { static std::string cachePathForFilePath(const std::string&p,const char*){return "/cache"+p;} };
struct Xtc {std::string path; Xtc(const std::string&p,const char*):path(p){} std::string getCachePath(){return "/cache"+path;} };
using Txt=Xtc;
struct {std::string openEpubPath;bool saveToFile(){return true;}} APP_STATE;
struct {bool updatePath(const std::string&,const std::string&,const std::string&,const std::string&){return true;}} RECENT_BOOKS;
'''
source+='\n#define private public\n#include "src/ClippingStore.h"\n#undef private\n'
source+='\nconstexpr uint8_t CLIP_LEGACY_VERSION=1, CLIP_TEXT_OFFSET_VERSION=2, CLIP_LAYOUT_SIGNATURE_VERSION=3, CLIP_VERSION=4;\nconstexpr char CLIPPINGS_DIR[]="/.crosspoint/clippings";\nconstexpr size_t TEXT_COPY_BUFFER_SIZE=128;\nstd::string storeFilePathForBook(const std::string&p,const std::string&){return "/clips"+p;}\n'
import re
for sig in ['bool copyBytes(', 'bool ClippingStore::readFromFile(const std::string&', 'bool ClippingStore::writeToFile(', 'bool ClippingStore::beginRenameMigration(', 'bool ClippingStore::commitRenameMigration(', 'bool ClippingStore::rollbackRenameMigration(']:
    body=method('src/ClippingStore.cpp',sig)
    body=re.sub(r'\b(LEGACY_VERSION|TEXT_OFFSET_VERSION|LAYOUT_SIGNATURE_VERSION|VERSION)\b',r'CLIP_\1',body)
    source+='\n'+body+'\n'
source+=(ROOT/'src/util/BookMoveUtils.h').read_text().replace('#pragma once','')
source+='\nnamespace {\n'+method('src/util/BookMoveUtils.cpp','bool getCachePath(')+'\n}\nnamespace BookMoveUtils {\n'+method('src/util/BookMoveUtils.cpp','RenameMigrationResult migrateRenamedBookState(')+'\n}\n'
source+=(ROOT/'test/file_actions/nested_cases.cpp').read_text()
with open('/root/.hermes/cache/scratch/crosscub-review-probes.lock','a') as lock:
    try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    except BlockingIOError:raise SystemExit('BLOCKED: shared compiler lock busy')
    with tempfile.TemporaryDirectory(prefix='files-independent-nested-',dir='/root/.hermes/cache/scratch') as tmp:
        cpp=Path(tmp)/'probe.cpp';exe=Path(tmp)/'probe';cpp.write_text(source)
        cmd=['g++','-std=c++17','-Wall','-Wextra','-Wno-unused-function','-Wno-unused-parameter','-O1','-I'+str(ROOT),str(cpp),'-o',str(exe)]
        if '--sanitize' in sys.argv:cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
        compile_result=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=60)
        log='COMMAND: '+repr(cmd)+'\n'+compile_result.stdout+'\nCOMPILE_EXIT='+str(compile_result.returncode)+'\n'
        if compile_result.returncode==0:
            result=subprocess.run([str(exe)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=10)
            log+=result.stdout+'\nRUN_EXIT='+str(result.returncode)+'\n'
        else:result=compile_result
        OUT.write_text(log);print(log)
        raise SystemExit(result.returncode)
