#!/usr/bin/env python3
"""Bounded production progress mapping/client probes. No device/network access."""
import argparse, fcntl, hashlib, json, os, pathlib, subprocess, sys
p=argparse.ArgumentParser(); p.add_argument('--phase',default='api'); p.add_argument('--label',default='probe'); p.add_argument('--sanitize',action='store_true'); a=p.parse_args()
R=pathlib.Path(__file__).resolve().parents[3]; T=pathlib.Path(__file__).resolve().parent
E=pathlib.Path('/root/github/.host-data/crosssmudge/crosspoint-rebase-1655/integration/target-progress-sync')/a.label; E.mkdir(parents=True,exist_ok=True)
B=pathlib.Path('/root/.hermes/cache/scratch/target-progress-sync'); B.mkdir(exist_ok=True)
lock=open('/root/.hermes/cache/scratch/crosspoint-qualification.lock','w'); fcntl.flock(lock,fcntl.LOCK_EX)
incs=[T/'adapters',R/'test/crosspoint_migration/epub-layout-final/adapters',R/'test/chapter_html_slim_parser/stubs',R/'test/crosspoint_migration/font-finish/renderer-stubs',R/'freeink-sdk/libs/book/ContentProtection/include',R/'lib/uzlib/src',R/'lib/miniz/src',R/'src',R/'lib',R/'lib/Epub',R/'lib/Epub/Epub',R/'lib/Epub/Epub/parsers']+[R/'lib'/n for n in ['KOReaderSync','GifSupport','InflateReader','MemoryBudget','AsciiText','EpdFont','Utf8','Memory','Serialization','MiniBidi','FsHelpers','XmlParserUtils','GfxRenderer','FontCache','Bitmap','Hash','ZipFile','InflateStream','JsonParser','Txt','PngToBmpConverter','JpegToBmpConverter','GifToBmpConverter']]
incs += [R/'.pio/libdeps/sticky/ArduinoJson/src',pathlib.Path('/root/.hermes/cache/scratch/sticky-browser-backend/pio-libdeps/sticky-owner-test/ArduinoJson/src')]
incs += [R/'lib/hal', R/'test/fs_helpers/stubs',pathlib.Path('/root/.hermes/cache/drinkwater-toolchain/user/libraries/ArduinoJson/src')]
incs += [R/'lib/HttpHeader']
base=['g++','-I'+str(R/'lib/Epub'),'-I'+str(R/'lib/ZipFile'),'-I'+str(R/'lib/FsHelpers'),'-std=c++20','-O0','-g0','-ffunction-sections','-fdata-sections','-include',str(R/'lib/MemoryBudget/PoolBudget.h')]+['-I'+str(i) for i in incs]
if a.sanitize: base+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
receipts=[]
def run(name,cmd):
 q=subprocess.run([str(v) for v in cmd],capture_output=True,text=True,timeout=150,env={**os.environ,'TMPDIR':str(B),'ASAN_OPTIONS':'detect_leaks=1'})
 (E/(name+'.log')).write_text(q.stdout+q.stderr)
 receipts.append({'name':name,'command':[str(v) for v in cmd],'exit':q.returncode})
 (E/'receipt.json').write_text(json.dumps(receipts,indent=2))
 print(name,q.returncode); print((q.stdout+q.stderr)[-10000:] if q.returncode else q.stdout[-3000:])
 return q.returncode
if a.phase=='transport-includes':
 source=R/'lib/KOReaderSync/KOReaderSyncClient.cpp'
 headers=[line.split('"')[1] for line in source.read_text().splitlines() if line.startswith('#include "') and any(n in line for n in ['HttpDownloader.h','HttpRedirectPolicy.h'])]
 assert len(headers)==2
 probe=E/'transport-includes.cpp'
 probe.write_text(''.join('#include "'+str((source.parent/h).resolve())+'"\n' for h in headers))
 # Library compilation must not rely on the application's src search path.
 rc=run('transport-includes',[v for v in base if v!='-I'+str(R/'src')]+['-fsyntax-only',probe])
elif a.phase=='api': rc=run('api',base+['-fsyntax-only',T/'api.cpp'])
elif a.phase=='syntax':
 rc=0
 for source in ['lib/KOReaderSync/ProgressMapper.cpp','lib/KOReaderSync/KOReaderSyncClient.cpp']:
  rc=run(pathlib.Path(source).stem,base+['-fsyntax-only',R/source]) or rc
elif a.phase=='mapping':
 import zipfile
 fixture=B/'fixture'; fixture.mkdir(exist_ok=True)
 with zipfile.ZipFile(fixture/'book.epub','w') as z:
  for i in range(2): z.writestr(f'chapter{i}.xhtml','<html><body><p>abcde</p><p>fghij</p></body></html>')
 G=pathlib.Path('/root/.hermes/cache/scratch/library-finish-deps/googletest-1.17.0/googletest')
 sources=[T/'MappingTest.cpp']+[R/x for x in ['lib/KOReaderSync/ProgressMapper.cpp','lib/KOReaderSync/ChapterXPathResolver.cpp','lib/Epub/Epub.cpp','lib/Epub/Epub/BookMetadataCache.cpp','lib/Epub/Epub/Section.cpp','lib/Epub/Epub/Page.cpp','lib/Epub/Epub/image/PxcV2.cpp','lib/Epub/Epub/parsers/ContainerParser.cpp','lib/Epub/Epub/parsers/ContentOpfParser.cpp','lib/Epub/Epub/parsers/TocNavParser.cpp','lib/Epub/Epub/parsers/TocNcxParser.cpp','lib/Epub/Epub/converters/ImageDimsProbe.cpp','lib/Epub/Epub/htmlEntities.cpp','lib/ZipFile/ZipFile.cpp','lib/Utf8/Utf8.cpp','lib/InflateReader/InflateReader.cpp','lib/FsHelpers/FsHelpers.cpp','lib/Txt/Txt.cpp','lib/Txt/TxtToHtml.cpp','lib/miniz/src/InflateStream.cpp','lib/miniz/src/miniz_impl.c','lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp','lib/Epub/Epub/ParsedText.cpp','lib/Epub/Epub/WordStore.cpp','lib/Memory/BuildScratch.cpp']]
 exe=B/('mapping-san' if a.sanitize else 'mapping')
 cmd=base+['-include',T/'adapters/Clock.h','-pthread','-I'+str(G/'include'),'-I'+str(G),'-DPROGRESS_FIXTURE_DIR="'+str(fixture)+'"']+sources+[G/'src/gtest-all.cc',G/'src/gtest_main.cc','-lexpat','-Wl,--gc-sections','-o',exe]
 rc=run('mapping-compile',cmd)
 if not rc:
  (E/'binary.json').write_text(json.dumps({'sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'size':exe.stat().st_size},indent=2))
  rc=run('mapping-run',[exe,'--gtest_output=xml:'+str(E/'mapping.xml')])
else: raise SystemExit('unknown phase')
inputs=list(T.rglob('*'))+[R/'lib/KOReaderSync'/x for x in ['ProgressMapper.h','ProgressMapper.cpp','CrossPointPosition.h','KOReaderSyncClient.h','KOReaderSyncClient.cpp']]
(E/'inputs.json').write_text(json.dumps({str(x.relative_to(R)):hashlib.sha256(x.read_bytes()).hexdigest() for x in inputs if x.is_file()},indent=2))
sys.exit(rc)
