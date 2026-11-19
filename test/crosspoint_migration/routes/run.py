#!/usr/bin/env python3
"""Bounded production-method probes. Not SDK, rendered, FAT or device proof."""
from pathlib import Path
from typing import Any
import argparse, hashlib, json, os, subprocess, sys, tempfile
R=Path(__file__).resolve().parents[3]
P=argparse.ArgumentParser();P.add_argument('--evidence',type=Path,required=True);P.add_argument('--sanitize',action='store_true');A=P.parse_args();A.evidence.mkdir(parents=True,exist_ok=True)
def method(path,signature):
 t=(R/path).read_text();s=t.index(signature);return t[s:t.index('\n}',s)+2]
def compile_run(name,source):
 with tempfile.TemporaryDirectory(prefix='routes-',dir=os.environ['TMPDIR']) as d:
  p=Path(d);(p/'probe.cpp').write_text(source)
  cmd=['g++','-std=c++20','-O1','-g','-Wall','-Wextra','-Werror']
  if A.sanitize:cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
  cmd += [str(p/'probe.cpp'),'-o',str(p/'probe')]
  a=subprocess.run(cmd,capture_output=True,text=True,timeout=70);log='COMMAND '+repr(cmd)+'\n'+a.stdout+a.stderr;rc=a.returncode
  if not rc:
   a=subprocess.run([str(p/'probe')],capture_output=True,text=True,timeout=20);log+=a.stdout+a.stderr;rc=a.returncode
  (A.evidence/(name+'.log')).write_text(log+'\nEXIT='+str(rc)+'\n')
  return {'name':name,'rc':rc,'source_sha256':hashlib.sha256(source.encode()).hexdigest(),
          'passes':[line for line in log.splitlines() if line.startswith('PASS ')]}
results=[]
opds='src/activities/browser/OpdsBookBrowserActivity.cpp'
s=r'''
#include <string>
#include <set>
#include <cassert>
#include <cstdio>
struct {std::set<std::string> roots;bool exists(const char* p){return roots.count(p);}} Storage;
struct HttpDownloader {enum DownloadError {OK,ABORTED,HTTP_ERROR,PUBLICATION_UNCERTAIN};};
struct OpdsBookBrowserActivity {enum class State {BROWSING,ERROR};State state=State::BROWSING;std::string errorMessage;int updates=0,finished=0;void requestUpdate(){++updates;}void finishDownload(HttpDownloader::DownloadError){++finished;}bool admitNewDownload(const std::string&);void finishSafeDownload(HttpDownloader::DownloadError);};
'''+method(opds,'bool OpdsBookBrowserActivity::admitNewDownload(')+'\n'+method(opds,'void OpdsBookBrowserActivity::finishSafeDownload(')+r'''
int main(){OpdsBookBrowserActivity a;assert(a.admitNewDownload("/book.epub"));for(auto suffix:{"",".part",".old"}){Storage.roots={std::string("/book.epub")+suffix};assert(!a.admitNewDownload("/book.epub"));assert(a.state==OpdsBookBrowserActivity::State::ERROR);assert(Storage.roots.size()==1);}a.finishSafeDownload(HttpDownloader::PUBLICATION_UNCERTAIN);assert(a.finished==0&&a.errorMessage.find("uncertain")!=std::string::npos);for(auto r:{HttpDownloader::OK,HttpDownloader::ABORTED,HttpDownloader::HTTP_ERROR})a.finishSafeDownload(r);assert(a.finished==3);puts("PASS OPDS new destination; collision/partial/old refusal; uncertain no finish/publication");}
'''
results.append(compile_run('opds-admission',s))
font='src/activities/settings/FontSelectionActivity.cpp'
s=r'''
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include <cassert>
#include <cstdio>
#include <algorithm>
struct CrossPointSettings {enum FONT_SIZE{TINY,SMALL,MEDIUM,LARGE,FONT_SIZE_COUNT};static constexpr uint8_t NOTOSANS=2,BUILTIN_FONT_COUNT=3;static uint8_t getStoredReaderFontSize(FONT_SIZE s){return s;}static uint8_t getReaderFontPointSize(FONT_SIZE s){return 10+2*s;}uint8_t fontFamily=0,readerFontPointSize=14;char sdFontFamilyName[64]={};} SETTINGS;
struct Family {std::string name;std::vector<uint8_t> availableSizes()const{return {11,15,19};}};
struct SdCardFontRegistry {std::vector<Family> families{{"Vector.otf"}};const auto& getFamilies()const{return families;}};
struct {int suppressed=0;void suppressNextConfirmRelease(){++suppressed;}} mappedInput;
struct FontSelectionActivity {struct Font {bool isBuiltin;uint8_t settingIndex;};std::vector<Font> fonts_{{true,0},{true,1},{true,2},{false,3}};int selectedIndex_=0;SdCardFontRegistry* registry_;bool finished=false;void finish(){finished=true;}void handleSelection();};
constexpr uint8_t INVALID_STORED_FONT_SIZE=255;
'''+method(font,'uint8_t closestSizeIndex(')+'\n'+method(font,'uint8_t closestBuiltinStoredSize(')+'\n'+method(font,'uint8_t currentFontPointSize(')+'\n'+method(font,'void FontSelectionActivity::handleSelection(')+r'''
int main(){assert(closestSizeIndex({12,14,16,18},17)==2);SdCardFontRegistry reg;FontSelectionActivity a;a.registry_=&reg;a.selectedIndex_=2;SETTINGS.readerFontPointSize=18;a.handleSelection();assert(SETTINGS.fontFamily==2&&SETTINGS.readerFontPointSize==18&&a.finished);a.selectedIndex_=0;a.handleSelection();assert(SETTINGS.fontFamily==0&&SETTINGS.readerFontPointSize==16);a.selectedIndex_=3;SETTINGS.readerFontPointSize=16;a.handleSelection();assert(SETTINGS.readerFontPointSize==15&&std::string(SETTINGS.sdFontFamilyName)=="Vector.otf");puts("PASS Noto/owner IDs; nearest-size tie; SD vector family selection");}
'''
results.append(compile_run('font-selection',s))
s=r'''
#include <string>
#include <set>
#include <cassert>
#include <cstdio>
namespace FsHelpers {bool hasEpubExtension(const std::string& p){return p.ends_with(".epub");}bool hasXtcExtension(const std::string& p){return p.ends_with(".xtc");}bool hasTxtExtension(const std::string& p){return p.ends_with(".txt");}bool hasMarkdownExtension(const std::string& p){return p.ends_with(".md");}}
struct {std::set<std::string> roots;bool exists(const char* p){return roots.count(p);}} Storage;
struct Epub {static std::string cachePathForFilePath(const std::string& p,const char*){return "/modern"+p;}};
struct Txt {std::string path;Txt(const std::string& p,const char*):path(p){}std::string getCachePath(){return "/legacy"+path;}};
struct Xtc {std::string path;Xtc(const std::string& p,const char*):path(p){}std::string getCachePath(){return "/xtc"+path;}};
'''+method('src/activities/home/FileBrowserActivity.cpp','bool getMutationBookCache(')+r'''
int main(){std::string cache;const char* type=nullptr;for(auto ext:{".txt",".md"}){const std::string path=std::string("/book")+ext;Storage.roots={"/legacy"+path};assert(getMutationBookCache(path,cache,type)&&cache=="/legacy"+path&&std::string(type)=="txt");Storage.roots={"/modern"+path};assert(getMutationBookCache(path,cache,type)&&cache=="/modern"+path&&std::string(type)=="epub");Storage.roots.insert("/legacy"+path);const auto before=Storage.roots;assert(!getMutationBookCache(path,cache,type)&&Storage.roots==before);Storage.roots.clear();assert(getMutationBookCache(path,cache,type));}assert(getMutationBookCache("/book.epub",cache,type)&&std::string(type)=="epub");assert(getMutationBookCache("/book.xtc",cache,type)&&std::string(type)=="xtc");assert(getMutationBookCache("/image.jpg",cache,type)&&!type&&cache.empty());puts("PASS actual unified/legacy TXT/Markdown Move cache admission; two-root refusal without mutation; EPUB/XTC/non-book controls");}
'''
results.append(compile_run('mutation-cache',s))
# Retain the inherited Files production-body/fault matrix. Its separate full
# Confirmation translation-unit build is blocked by the parent's OptionPopup
# conflict; do not silently pass that build or replace its popup assertions.
t=(R/'test/file_actions/run.py').read_text();t=t[:t.index("            header=ROOT/")]+"            return first\n"
ns:dict[str,Any]={'__file__':str(R/'test/file_actions/run.py'),'__name__':'routes_legacy_files'}
old_argv=sys.argv;sys.argv=['run.py']+(['--sanitize'] if A.sanitize else [])
saved_run=subprocess.run
def captured_run(*args,**kwargs):
 kwargs.update(stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
 result=saved_run(*args,**kwargs)
 print(result.stdout or '',end='')
 return result
try:
 exec(compile(t,'inherited-files-production-bodies','exec'),ns)
 legacy_method=ns['method']
 def composed_method(path,signature):
  body=legacy_method(path,signature)
  if signature=='void FileBrowserActivity::promptDeleteFile(':
   body=legacy_method(path,'bool getMutationBookCache(')+'\n'+body
  return body
 ns['method']=composed_method
 import contextlib,io
 out=io.StringIO()
 subprocess.run=captured_run
 with contextlib.redirect_stdout(out),contextlib.redirect_stderr(out):rc=ns['run']()
 (A.evidence/'files-production-bodies.log').write_text(out.getvalue()+'\nEXIT='+str(rc)+'\n')
 results.append({'name':'files-production-bodies','rc':rc,'passes':[l for l in out.getvalue().splitlines() if l.startswith('PASS ')]})
except Exception as e:
 results.append({'name':'files-production-bodies','rc':1,'error':repr(e)})
finally:
 sys.argv=old_argv
 subprocess.run=saved_run
(A.evidence/'results.json').write_text(json.dumps(results,indent=2)+'\n')
print(json.dumps(results,indent=2));sys.exit(int(any(x['rc'] for x in results)))
