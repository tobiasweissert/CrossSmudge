#!/usr/bin/env python3
"""Actual renderer TU + pinned SDK adapter, and extracted exact reader declarations."""
import argparse, hashlib, json, os, pathlib, subprocess, sys
R=pathlib.Path(__file__).resolve().parents[3];T=pathlib.Path(__file__).resolve().parent
p=argparse.ArgumentParser();p.add_argument('--evidence',required=True,type=pathlib.Path);p.add_argument('--sanitize',action='store_true');a=p.parse_args();O=a.evidence.resolve();O.mkdir(parents=True,exist_ok=True)
assert subprocess.check_output(['git','-C',str(R/'freeink-sdk'),'rev-parse','HEAD'],text=True).strip()=='634a3f8c067a3714a6a7c622015138af096ab5d2'
flags=['-std=c++20','-O0','-g0','-fno-exceptions','-fno-rtti','-include','cstring','-ffunction-sections','-fdata-sections']
if a.sanitize: flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
incs=['test/crosspoint_migration/font-finish/renderer-stubs','lib/GfxRenderer','lib/EpdFont','test/epub_grayscale/raster_stubs','test/memory_policy/stubs','test/sd_card_font/stubs','lib/Memory','lib/MemoryBudget','lib/Utf8','lib/MiniBidi','lib/miniz/src','lib/InflateReader','lib/uzlib/src','freeink-sdk/libs/ui/FreeInkUI/include','freeink-sdk/libs/assets/Icons/include','lib/I18n','src/components']
base=['g++',*flags,'-I/root/.hermes/cache/scratch/crosspoint-i18n-preflight',*['-I'+str(R/i) for i in incs]]
receipts=[]
def run(name,cmd):
 cmd=list(map(str,cmd));x=subprocess.run(cmd,cwd=R,capture_output=True,text=True,timeout=160,env=dict(os.environ,TMPDIR='/root/.hermes/cache/scratch',ASAN_OPTIONS='detect_leaks=1',UBSAN_OPTIONS='halt_on_error=1'))
 (O/(name+'.log')).write_text(x.stdout+x.stderr);receipts.append({'name':name,'argv':cmd,'exit':x.returncode});(O/'receipts.json').write_text(json.dumps(receipts,indent=2));print(name,x.returncode,flush=True)
 if x.returncode:print((x.stdout+x.stderr)[-7000:]);sys.exit(x.returncode)
menu=(R/'src/activities/reader/XtcReaderMenuActivity.h').read_text()
# Preserve the complete declaration namespace and any actual global Rect forward declaration.
prefix=menu[menu.index('// Shared reader chrome'):menu.index('class XtcReaderMenuActivity')]
if 'struct Rect;' in menu[:menu.index('// Shared reader chrome')]: prefix='struct Rect;\n'+prefix
reader=(R/'src/activities/reader/EpubReaderActivity.cpp').read_text()
start=reader.index('(std::string(tr(STR_TRACK_READING_STATS)) + ": "')
expression=reader[start:reader.index('.c_str()',start)].strip()
# Compile the exact owner stats label expression with the real I18n macro.
probe='#include <UiAppHost.h>\n#include <I18n.h>\n#include <string>\nenum class ReaderStatusBarPosition : uint8_t;\nstruct ReaderStatusBarConfig;\n'+prefix+'\nauto statsLabel(bool bookStatsEnabled) { return '+expression+'; }\n'
(O/'sdk-declarations.cpp').write_text(probe)
run('sdk-declarations',base+['-fsyntax-only',O/'sdk-declarations.cpp'])
sources=['lib/GfxRenderer/GfxRenderer.cpp','lib/GfxRenderer/Bitmap.cpp','lib/GfxRenderer/BitmapHelpers.cpp','lib/GfxRenderer/FontCacheManager.cpp','lib/EpdFont/EpdFont.cpp','lib/EpdFont/EpdFontFamily.cpp','lib/EpdFont/SdCardFont.cpp','lib/EpdFont/FontDecompressor.cpp','lib/Memory/BuildScratch.cpp','lib/Utf8/Utf8.cpp','lib/MiniBidi/BidiUtils.cpp','lib/MiniBidi/minibidi.c','lib/InflateReader/InflateReader.cpp','lib/uzlib/src/tinflate.c','lib/uzlib/src/uzlib_checksums.c']
objects=[]
for i,s in enumerate(sources[:]):
 if s.endswith('.c'):
  obj=O/('c'+str(i)+'.o');run('c-'+str(i),['gcc','-O0',*(['-fsanitize=address,undefined','-fno-pie'] if a.sanitize else []),*['-I'+str(R/j) for j in incs],'-c',R/s,'-o',obj]);objects.append(obj);sources.remove(s)
run('renderer-compile',base+[T/'RendererProbe.cpp',*[R/s for s in sources],*objects,'-Wl,--gc-sections','-o',O/'renderer'])
run('renderer-run',[O/'renderer'])
run('inherited-renderer-compile',base+[R/'test/crosspoint_migration/font-finish/renderer_probe.cpp',*[R/s for s in sources],*objects,'-Wl,--gc-sections','-o',O/'inherited-renderer'])
run('inherited-renderer-run',[O/'inherited-renderer'])
paths=[R/s for s in sources]+[T/'RendererProbe.cpp',R/'lib/GfxRenderer/GfxRenderer.h',R/'src/activities/reader/XtcReaderMenuActivity.h',R/'src/activities/reader/EpubReaderActivity.cpp']+list((R/'freeink-sdk/libs/ui/FreeInkUI/include').rglob('*.h'))
(O/'inputs.json').write_text(json.dumps({str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in paths},indent=2))
(O/'binary-hashes.json').write_text(json.dumps({f:hashlib.sha256((O/f).read_bytes()).hexdigest() for f in ['renderer','inherited-renderer']},indent=2))
