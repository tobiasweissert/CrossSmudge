#!/usr/bin/env python3
"""Real reader models; renderer/storage peripherals are explicit existing doubles."""
import argparse, hashlib, json, os, pathlib, subprocess, sys
R = pathlib.Path(__file__).resolve().parents[3]
T = pathlib.Path(__file__).resolve().parent
p = argparse.ArgumentParser()
p.add_argument('--evidence', type=pathlib.Path, required=True)
p.add_argument('--sanitize', action='store_true')
p.add_argument('--regressions', action='store_true')
a = p.parse_args(); O = a.evidence.resolve(); O.mkdir(parents=True, exist_ok=True)
receipts = []
G = pathlib.Path('/root/.hermes/cache/scratch/library-finish-deps/googletest-1.17.0/googletest')
incs = [R/'test/crosspoint_migration/epub-layout-final/adapters', R/'test/chapter_html_slim_parser/stubs', R/'test/crosspoint_migration/epub-finish/stubs', R/'test/crosspoint_migration/font-finish/renderer-stubs', R/'freeink-sdk/libs/book/ContentProtection/include', R/'lib/uzlib/src', R/'lib/miniz/src', R/'src', R/'lib', R/'lib/Epub', R/'lib/Epub/Epub', R/'lib/Epub/Epub/parsers', R/'src/activities/reader'] + [R/'lib'/n for n in ['InflateReader','MemoryBudget','AsciiText','EpdFont','Utf8','Memory','Serialization','MiniBidi','FsHelpers','XmlParserUtils','GfxRenderer','FontCache','Bitmap','Hash','ZipFile','InflateStream','JsonParser','Txt','PngToBmpConverter','JpegToBmpConverter','GifToBmpConverter']]
flags = ['-std=c++20','-O0','-g0','-pthread','-ffunction-sections','-fdata-sections','-include',str(R/'lib/MemoryBudget/PoolBudget.h')] + ['-I'+str(i) for i in incs] + ['-I'+str(R),'-I'+str(G/'include'),'-I'+str(G)]
if a.sanitize: flags += ['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
def run(name, cmd):
    env = dict(os.environ, TMPDIR='/root/.hermes/cache/scratch', ASAN_OPTIONS='detect_leaks=1', UBSAN_OPTIONS='halt_on_error=1', PYTHONDONTWRITEBYTECODE='1')
    try:
        result = subprocess.run(list(map(str,cmd)), cwd=R, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=180)
        rc, log = result.returncode, result.stdout
    except subprocess.TimeoutExpired as ex: rc, log = 124, str(ex)
    (O/(name+'.log')).write_text(log)
    receipts.append({'name':name,'argv':list(map(str,cmd)),'exit':rc})
    (O/'receipts.json').write_text(json.dumps(receipts,indent=2))
    print(name,rc,flush=True)
    if rc: print(log[-10000:]); sys.exit(rc)
common = [R/'lib/Epub/Epub'/s for s in ['Page.cpp','blocks/TextBlock.cpp','blocks/ImageBlock.cpp','converters/ImageToFramebufferDecoder.cpp','CjkBreaks.cpp']]+[R/'lib/Utf8/Utf8.cpp',R/'test/crosspoint_migration/epub-layout-final/link_adapters.cpp']
sources = [T/'ModelsTest.cpp',*common,G/'src/gtest-all.cc',G/'src/gtest_main.cc']
if a.regressions:
    sources += [R/'test/crosspoint_migration/epub-layout-final'/s for s in ['ChapterHtmlSlimParserAdapted.cpp','MigrationTest.cpp','CompactFallbackTest.cpp']]
    sources += [R/'test/chapter_html_slim_parser/DictionaryHtmlPagesTest.cpp',R/'src/util/DictHtmlPages.cpp']
    sources += [R/'lib/Epub/Epub'/s for s in ['parsers/ChapterHtmlSlimParser.cpp','css/CssParser.cpp','ParsedText.cpp','WordStore.cpp','htmlEntities.cpp','parsers/PreviewBlockLocator.cpp','tables/CompactTableLayout.cpp','converters/ImageDimsProbe.cpp']]
    sources += [T/'ConstructorTest.cpp']
sources += [R/'lib/MiniBidi/BidiUtils.cpp']
run('bidi-compile',['gcc','-O0','-c',R/'lib/MiniBidi/minibidi.c','-o',O/'bidi.o']+(['-fsanitize=address,undefined','-fno-pie'] if a.sanitize else []))
sources += [O/'bidi.o']
inputs = [s for s in sources if s.suffix != '.o'] + list(T.glob('*'))
(O/'inputs.json').write_text(json.dumps({str(s):hashlib.sha256(s.read_bytes()).hexdigest() for s in inputs if s.is_file()},indent=2))
run('models-compile',['g++',*flags,*sources,'-lexpat','-Wl,--gc-sections','-o',O/'models'])
run('models-run',[O/'models','--gtest_output=xml:'+str(O/'results.xml')])
(O/'binary-hashes.json').write_text(json.dumps({'models':hashlib.sha256((O/'models').read_bytes()).hexdigest()},indent=2))
