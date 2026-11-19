#!/usr/bin/env python3
import pathlib,subprocess,os,json,hashlib,sys,time
os.environ["TMPDIR"]="/root/.hermes/cache/scratch/epub-layout-final-r2"
R=pathlib.Path(__file__).resolve().parents[3];T=pathlib.Path(__file__).parent
E=pathlib.Path('/root/github/.host-data/crosssmudge/crosspoint-rebase-1655/integration/epub-layout-final')
O=pathlib.Path('/root/.hermes/cache/scratch/epub-layout-final-r2')/('checks-'+str(time.time_ns()));O.mkdir()
incs=[T/'adapters',R/'test/chapter_html_slim_parser/stubs',R/'test/crosspoint_migration/epub-finish/stubs',R/'test/crosspoint_migration/font-finish/renderer-stubs',R/'freeink-sdk/libs/book/ContentProtection/include',R/'lib/uzlib/src',R/'lib/miniz/src',R/'src',R/'lib',R/'lib/Epub',R/'lib/Epub/Epub',R/'lib/Epub/Epub/parsers']+[R/'lib'/n for n in ['InflateReader','MemoryBudget','AsciiText','EpdFont','Utf8','Memory','Serialization','MiniBidi','FsHelpers','XmlParserUtils','GfxRenderer','FontCache','Bitmap','Hash','ZipFile','InflateStream','JsonParser','Txt','PngToBmpConverter','JpegToBmpConverter','GifToBmpConverter']]
base=['g++','-std=c++20','-O0','-g0','-fsyntax-only','-include',str(R/'lib/MemoryBudget/PoolBudget.h')]+['-I'+str(p) for p in incs]

# Adapt only private storage inspection to WordStore; every original assertion/case is retained.
import re
original=R/'test/chapter_html_slim_parser/ChapterHtmlSlimParserTest.cpp'
adapted=original.read_text().replace('../../src/fontIds.h','src/fontIds.h')
adapted=re.sub(r'parser.currentTextBlock->words\[(\d+)\]',r'parser.currentTextBlock->wordAt(\1)',adapted)
(T/'ChapterHtmlSlimParserAdapted.cpp').write_text(adapted)
G=pathlib.Path('/root/.hermes/cache/scratch/library-finish-deps/googletest-1.17.0/googletest')
base.remove('-fsyntax-only')
base+=['-pthread','-I'+str(R),'-I'+str(G/'include'),'-I'+str(G),'-ffunction-sections','-fdata-sections']
if '--sanitize' in sys.argv: base+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
sources=['test/crosspoint_migration/epub-layout-final/ChapterHtmlSlimParserAdapted.cpp','test/chapter_html_slim_parser/DictionaryHtmlPagesTest.cpp','src/util/DictHtmlPages.cpp','test/crosspoint_migration/epub-layout-final/link_adapters.cpp']
sources += ['test/crosspoint_migration/epub-layout-final/MigrationTest.cpp','test/crosspoint_migration/epub-layout-final/CompactFallbackTest.cpp']
sources += ['lib/Epub/Epub/'+x for x in ['Page.cpp','blocks/TextBlock.cpp','blocks/ImageBlock.cpp','CjkBreaks.cpp','parsers/ChapterHtmlSlimParser.cpp','css/CssParser.cpp','ParsedText.cpp','WordStore.cpp','htmlEntities.cpp','parsers/PreviewBlockLocator.cpp','tables/CompactTableLayout.cpp','converters/ImageDimsProbe.cpp','converters/ImageToFramebufferDecoder.cpp']]
sources += ['lib/MiniBidi/BidiUtils.cpp','lib/MiniBidi/minibidi.c','lib/Utf8/Utf8.cpp']
c_cmd=['gcc','-O0','-c',str(R/'lib/MiniBidi/minibidi.c'),'-o',str(O/'minibidi.o')]
if '--sanitize' in sys.argv: c_cmd += ['-fsanitize=address,undefined']
with (O/'c-compile.log').open('w') as out:
 c_ret=subprocess.run(c_cmd,stdout=out,stderr=subprocess.STDOUT,timeout=120).returncode
cmd=base+[str(R/f) for f in sources if not f.endswith('.c')]+[str(O/'minibidi.o')]+[str(G/'src/gtest-all.cc'),str(G/'src/gtest_main.cc'),'-lexpat','-Wl,--gc-sections','-o',str(O/'chapter')]
receipts=[{'phase':'c-compile','command':c_cmd,'exit':c_ret}]
for phase,command in [('compile',cmd),('run',[str(O/'chapter'),'--gtest_output=xml:'+str(O/'results.xml')])]:
 with (O/(phase+'.log')).open('w') as f:
  try:ret=subprocess.run(command,stdout=f,stderr=subprocess.STDOUT,timeout=120,env={**os.environ,'TMPDIR':'/root/.hermes/cache/scratch','ASAN_OPTIONS':'detect_leaks=0'}).returncode
  except subprocess.TimeoutExpired:ret=124
 receipts.append({'phase':phase,'command':command,'exit':ret})
 print(phase,ret,flush=True)
 print((O/(phase+'.log')).read_text()[-24000:])
 if ret and phase=='compile': break
(O/'receipts.json').write_text(json.dumps({'runs':receipts,'sources':{f:hashlib.sha256((R/f).read_bytes()).hexdigest() for f in sources}},indent=2))
print(O)

sys.exit(next((r["exit"] for r in receipts if r["exit"]), 0))
