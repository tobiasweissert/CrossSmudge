#!/usr/bin/env python3
import pathlib,subprocess,os,json,hashlib,sys,time
os.environ["TMPDIR"]="/root/.hermes/cache/scratch/epub-layout-final-r2"
R=pathlib.Path(__file__).resolve().parents[3];T=pathlib.Path(__file__).parent
E=pathlib.Path('/root/github/.host-data/crosssmudge/crosspoint-rebase-1655/integration/epub-layout-final')
O=pathlib.Path('/root/.hermes/cache/scratch/epub-layout-final-r2')/('checks-'+str(time.time_ns()));O.mkdir()
incs=[pathlib.Path('/root/.hermes/cache/scratch/sticky-browser-backend/pio-libdeps/sticky-owner-test/ArduinoJson/src'),T/'adapters',R/'test/chapter_html_slim_parser/stubs',R/'test/crosspoint_migration/epub-finish/stubs',R/'test/crosspoint_migration/font-finish/renderer-stubs',R/'freeink-sdk/libs/book/ContentProtection/include',R/'lib/uzlib/src',R/'lib/miniz/src',R/'test/fs_helpers/stubs',R/'test/epub_grayscale/raster_stubs',R/'src',R/'lib',R/'lib/Epub',R/'lib/Epub/Epub',R/'lib/Epub/Epub/parsers']+[R/'lib'/n for n in ['GifSupport','InflateReader','MemoryBudget','AsciiText','EpdFont','Utf8','Memory','Serialization','MiniBidi','FsHelpers','XmlParserUtils','GfxRenderer','FontCache','Bitmap','Hash','ZipFile','InflateStream','JsonParser','Txt','PngToBmpConverter','JpegToBmpConverter','GifToBmpConverter']]
base=['g++','-std=c++20','-O0','-g0','-fsyntax-only','-include',str(R/'lib/MemoryBudget/PoolBudget.h')]+['-I'+str(p) for p in incs]
sources=['Epub/ParsedText.cpp','Epub/blocks/TextBlock.cpp','Epub/Page.cpp','Epub/blocks/ImageBlock.cpp','Epub/parsers/ChapterHtmlSlimParser.cpp','Epub/Section.cpp','Epub.cpp']
if len(sys.argv)>1:sources=[x for x in sources if any(a in x for a in sys.argv[1:])]
receipts=[]
for f in sources:
 path=R/'lib/Epub'/f;cmd=base[:1]+(['-I'+str(R/'lib/Epub'),'-include',str(R/'test/chapter_html_slim_parser/stubs/Arduino.h'),'-I'+str(R/'lib/ZipFile'),'-I'+str(R/'lib/FsHelpers')] if f=='Epub.cpp' else [])+base[1:]+(['-iquote',str(R/'lib/Epub')] if f.endswith('Section.cpp') else [])+[str(path)];log=O/(f.replace('/','_')+'.log')
 with log.open('w') as out:
  try:ret=subprocess.run(cmd,stdout=out,stderr=subprocess.STDOUT,timeout=120,env={**os.environ,'TMPDIR':'/root/.hermes/cache/scratch'}).returncode
  except subprocess.TimeoutExpired:ret=124
 receipts.append({'source':str(path),'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'command':cmd,'exit':ret,'log':str(log)})
 print(f,ret,flush=True)
 if ret:print(log.read_text()[:14000])
(O/'receipts.json').write_text(json.dumps(receipts,indent=2));print(O)

sys.exit(next((r["exit"] for r in receipts if r["exit"]), 0))
