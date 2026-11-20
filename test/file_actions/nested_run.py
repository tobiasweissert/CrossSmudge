#!/usr/bin/env python3
"""Real complete BookmarkStore/ClippingStore TUs; bounded memory-HAL composition.
The qualified clipping HAL is read-only and adapted only in scratch. No FAT or
power-loss qualification; cache/reference/library services are explicit doubles.
"""
from pathlib import Path
import fcntl, subprocess, tempfile, os, sys, json, hashlib, shlex
ROOT=Path(__file__).resolve().parents[2]
OUT=Path(os.environ.get('NESTED_LOG',str(Path(tempfile.gettempdir())/'crosscub-nested.log')))
def method(path, signature):
    text=(ROOT/path).read_text()
    assert text.count(signature)==1, signature
    start=text.index(signature)
    return text[start:text.index('\n}',start)+2]
def replace_once(text,old,new):
    assert text.count(old)==1,old
    return text.replace(old,new)

def hal_source():
    text=(ROOT/'test/library_builder/stubs/HalStorage.h').read_text()
    text=replace_once(text,'#pragma once','#pragma once\n#include "nested_io.h"')
    text=replace_once(text,'bool sync() { return bool(node); }','bool sync() { return bool(node) && !(failSync && path==readbackFaultPath); }')
    text=replace_once(text,'    fake::reads++;',r'''    fake::reads++;
    if (path==inputFaultPath) {
      if(inputFaultHits) ++inputReadsAfterFault;
      if(inputFault && pos==inputFaultPosition) {
        inputFault=false;++inputFaultHits;
        if(inputFaultReturn<0)return -1;
        const size_t n=std::min(size?size-1:0,node?node->bytes.size()-std::min(pos,node->bytes.size()):0);
        if(n)std::memcpy(out,node->bytes.data()+pos,n);
        pos+=n;return static_cast<int>(n);
      }
    }
    if(failRead && path==readbackFaultPath)return -1;
''')
    text=replace_once(text,'    pos += size;\n    return static_cast<int>(size);','    if(corruptRead && path==readbackFaultPath && size)static_cast<char*>(out)[0]^=1;\n    pos += size;\n    return static_cast<int>(size);')
    text=replace_once(text,'    fake::writesByPath[path]++;',r'''    fake::writesByPath[path]++;
    ++writeCalls;
    if(shortWrite){++shortWrites;return 0;}
    if(writeCalls==failWriteAt){++shortWrites;--size;if(!size)return 0;}
    ++mutations;''')
    text=replace_once(text,'  bool remove(const char* path) {','  bool remove(const char* path) {\n    ++mutations;')
    text=replace_once(text,'  bool rename(const char* from, const char* to) {',r'''  bool rename(const char* from, const char* to) {
    ++mutations;
    if(std::string(to)==failRenameTo) {
      if(mutateRenameFailure && exists(from))fake::files[to]=fake::files[from];
      return false;
    }
    if(failBackup && std::string(to).find(".rename.bak")!=std::string::npos)return false;''')
    # Real firmware BookmarkStore still names FsFile; all operations use the
    # complete checked HAL contract, not an FsFile-only alias shim.
    return text+'\nusing FsFile=HalFile;\n'

preamble=r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <HalStorage.h>
#include "nested_disk.h"
#include <uzlib.h>
#include <Serialization.h>
#define private public
#include "BookmarkStore.h"
#include "ClippingStore.h"
#undef private
#define LOG_ERR(...) ((void)0)
#define LOG_INF(...) ((void)0)
constexpr size_t MAX_BOOKMARKS=1024;
constexpr char BOOKMARKS_DIR[]="/.crosspoint/bookmarks";
constexpr char CLIPPINGS_DIR[]="/.crosspoint/clippings";
struct Epub { static std::string cachePathForFilePath(const std::string&p,const char*){return "/cache"+p;} };
struct Xtc {std::string path; Xtc(const std::string&p,const char*):path(p){} std::string getCachePath(){return "/cache"+path;} };
using Txt=Xtc;
namespace library {inline unsigned dirties=0;inline void markLibraryIndexDirty(){++dirties;}}
struct {std::string openEpubPath;bool saveToFile(){return true;}} APP_STATE;
struct {bool updatePath(const std::string&,const std::string&,const std::string&,const std::string&){return true;}} RECENT_BOOKS;
'''
source=preamble
for sig in ['std::string currentStoreFilePathForBook(', 'std::string legacyStoreFilePathForBook(']:
    source+='\n'+method('src/BookmarkStore.cpp',sig)
for sig in ['std::string storeFilePathForBook(', 'std::string modernStorePathForBook(']:
    source+='\n'+method('src/ClippingStore.cpp',sig)
source+='\n'+(ROOT/'src/activities/home/FileActionPaths.h').read_text().replace('#pragma once','')
source+='\n'+(ROOT/'src/util/BookMoveUtils.h').read_text().replace('#pragma once','')
bookmove=(ROOT/'src/util/BookMoveUtils.cpp').read_text()
assert bookmove.count('namespace {')==1 and bookmove.count('}  // namespace\n')==1
helpers=bookmove.split('namespace {',1)[1].split('}  // namespace',1)[0]
source+='\nnamespace {\n'+helpers+'\n}\nnamespace BookMoveUtils {\n'+method('src/util/BookMoveUtils.cpp','RenameMigrationResult migrateRenamedBookState(')+'\n}\n'
source+=(ROOT/'test/file_actions/nested_cases.cpp').read_text()
with open(Path(tempfile.gettempdir())/'crosscub-review-probes.lock','a') as lock:
    try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    except BlockingIOError:raise SystemExit('BLOCKED: shared compiler lock busy')
    with tempfile.TemporaryDirectory(prefix='files-independent-nested-') as tmp:
        tmp=Path(tmp);cpp=tmp/'probe.cpp';exe=tmp/'probe';cpp.write_text(source)
        (tmp/'HalStorage.h').write_text(hal_source())
        # Local quoted includes in the existing unrelated Arduino/FsHelpers
        # stubs must resolve the same scratch HAL, not introduce a second HAL.
        for name in ['Arduino.h','FsHelpers.h']:
            text=(ROOT/'test/library_builder/stubs'/name).read_text()
            text=replace_once(text,'#include "HalStorage.h"','#include <HalStorage.h>')
            if name=='FsHelpers.h':
                text+='\nnamespace FsHelpers {\ninline '+method('lib/FsHelpers/FsHelpers.cpp','bool isSafePathComponent(std::string_view name) {')+'\n}\n'
            (tmp/name).write_text(text)
        cmd=['g++','-std=c++20','-Wall','-Wextra','-Wno-unused-function','-Wno-unused-parameter','-O1',
             '-I'+str(tmp),'-I'+str(ROOT/'test/file_actions'),'-I'+str(ROOT/'test/clipping_store/stubs'),
             '-I'+str(ROOT/'test/library_builder/stubs'),'-I'+str(ROOT/'src'),
             '-I'+str(ROOT/'lib/Serialization'),'-I'+str(ROOT/'lib/Utf8'),'-I'+str(ROOT/'lib/uzlib/src'),
             str(cpp),str(ROOT/'src/BookmarkStore.cpp'),str(ROOT/'src/ClippingStore.cpp'),
             str(ROOT/'lib/Utf8/Utf8.cpp'),str(ROOT/'lib/uzlib/src/uzlib_checksums.c'),'-o',str(exe)]
        if '--sanitize' in sys.argv:cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer',*(['-fno-pie','-no-pie'] if sys.platform!='darwin' else [])]
        if os.environ.get('NESTED_INPUTS'):
            dependencies=subprocess.run(cmd[:-2]+['-MM'],capture_output=True,text=True,timeout=30)
            if dependencies.returncode:
                OUT.write_text(dependencies.stdout+dependencies.stderr)
                raise SystemExit(dependencies.returncode)
            paths={Path(token) for token in shlex.split(dependencies.stdout.replace('\\\n',' ')) if not token.endswith(':')}
            paths.update(ROOT/'test/file_actions'/name for name in ['nested_run.py','nested_cases.cpp','nested_io.h','nested_disk.h'])
            paths.update(ROOT/'test/library_builder/stubs'/name for name in ['HalStorage.h','Arduino.h','FsHelpers.h'])
            paths.update([ROOT/'src/util/BookMoveUtils.cpp',ROOT/'lib/FsHelpers/FsHelpers.cpp'])
            inputs={str(p.relative_to(ROOT)) if p.is_relative_to(ROOT) else 'generated/'+p.name:
                    hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(paths) if p.is_file()}
            Path(os.environ['NESTED_INPUTS']).write_text(json.dumps(inputs,indent=2))
        compiled=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=180)
        log='COMMAND: '+repr(cmd)+'\n'+compiled.stdout+'\nCOMPILE_EXIT='+str(compiled.returncode)+'\n'
        if compiled.returncode==0:
            result=subprocess.run([str(exe)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=60)
            log+=result.stdout+'\nRUN_EXIT='+str(result.returncode)+'\n'
        else:result=compiled
        OUT.write_text(log);print(log,flush=True)
        raise SystemExit(result.returncode)
