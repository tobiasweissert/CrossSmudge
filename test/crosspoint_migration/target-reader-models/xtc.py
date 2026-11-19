#!/usr/bin/env python3
import argparse, hashlib, json, os, pathlib, subprocess, sys
R=pathlib.Path(__file__).resolve().parents[3]; T=pathlib.Path(__file__).resolve().parent
p=argparse.ArgumentParser(); p.add_argument('--evidence',required=True,type=pathlib.Path); p.add_argument('--sanitize',action='store_true'); a=p.parse_args(); O=a.evidence; O.mkdir(parents=True,exist_ok=True)
flags=['-std=c++20','-O1','-g0','-ffunction-sections','-fdata-sections','-Wl,--gc-sections']
if a.sanitize: flags += ['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
incs=['test/crosspoint_migration/epub-finish/stubs','test/browser/image_stubs','test/memory_policy/stubs','lib/Epub','lib/Epub/Epub/parsers','lib/Memory','lib/MemoryBudget','lib/Serialization','lib/Utf8','lib/XmlParserUtils','test/crosspoint_migration/reader-core/stubs','lib/Xtc','lib/GfxRenderer']
activity=R/'src/activities/reader/XtcReaderActivity.cpp'
text=activity.read_text(); helper=text[text.index('enum class XtchRenderPass'):text.index('}  // namespace', text.index('enum class XtchRenderPass'))]
prelude='''#include <Xtc.h>
#include <Logging.h>
#include <array>
#include <cassert>
class GfxRenderer {
 public:
  std::array<bool,72> black{};
  void drawPixel(int x,int y,bool value) { assert(x>=0 && x<8 && y>=0 && y<9); black[y*8+x]=value; }
  bool isPixelBlack(int x,int y) const { return black[y*8+x]; }
};
'''
sleep=text[text.index('// Only use the 1-bit BW path;'):]
size_expr=sleep[sleep.index('const size_t pageBufferSize ='):sleep.index(';',sleep.index('const size_t pageBufferSize ='))+1]
sleep_size='size_t sleepPageSize(uint16_t pageWidth,uint16_t pageHeight,uint8_t bitDepth) { '+size_expr+' return pageBufferSize; }\n'
(O/'xtc-render.cpp').write_text(prelude+sleep_size+helper+(T/'XtcProbe.cpp').read_text())
src=[O/'xtc-render.cpp']+[R/x for x in ['lib/Xtc/Xtc.cpp','lib/Xtc/Xtc/XtcParser.cpp','lib/GfxRenderer/Bitmap.cpp','lib/GfxRenderer/BitmapHelpers.cpp']]
(O/'inputs.json').write_text(json.dumps({str(s):hashlib.sha256(s.read_bytes()).hexdigest() for s in src+[T/'xtc.py']},indent=2))
receipts=[]
for name,cmd in [('compile',['g++',*flags,*['-I'+str(R/i) for i in incs],*map(str,src),'-o',str(O/'xtc')]),('run',[str(O/'xtc')])]:
 x=subprocess.run(cmd,cwd=R,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=120,env=dict(os.environ,TMPDIR='/root/.hermes/cache/scratch',ASAN_OPTIONS='detect_leaks=1',UBSAN_OPTIONS='halt_on_error=1'))
 (O/(name+'.log')).write_text(x.stdout);receipts.append({'name':name,'argv':cmd,'exit':x.returncode});(O/'receipts.json').write_text(json.dumps(receipts,indent=2));print(name,x.returncode)
 if x.returncode:print(x.stdout[-6000:]);sys.exit(x.returncode)
(O/'binary-hashes.json').write_text(json.dumps({'xtc':hashlib.sha256((O/'xtc').read_bytes()).hexdigest()},indent=2))
