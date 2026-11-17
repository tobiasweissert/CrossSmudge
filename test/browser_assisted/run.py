#!/usr/bin/env python3
"""Isolated production Lua/native tests. Never uses PlatformIO or shared caches."""
import os,pathlib,subprocess,json,time,struct,zlib,hashlib
import sys,tempfile
sys.dont_write_bytecode=True
from guard import check
parent_guard=check()
R=pathlib.Path(__file__).resolve().parents[2];os.chdir(R)
O=pathlib.Path(os.environ.get('ASSISTED_TEST_OUT',str(pathlib.Path(tempfile.gettempdir())/'crosssmudge-browser-assisted-tests')));O.mkdir(parents=True,exist_ok=True)
(O/'tmp').mkdir(exist_ok=True)
(O/'parent-build-guard.json').write_text(json.dumps(parent_guard,indent=2)+'\n')
flags=['-O1','-g','-Ilib/Lua/src','-Ilib/LuaHttp','-Ilib/LuaBrowser','-Ilib/Memory','-Ilib/miniz/src','-Isrc/activities/apps/lua','-DBOARD_HAS_PSRAM=1']
if os.environ.get('SANITIZE'):flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1',SMUDGE_X3_CONSTRAINTS='1',TMPDIR=str(O/'tmp'))
def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data))
png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',128,64,1,3,0,0,0))+chunk(b'PLTE',b'\0\0\0\xff\xff\xff')+chunk(b'IDAT',zlib.compress(b''.join(b'\0'+b'\x55'*16 for _ in range(64))))+chunk(b'IEND',b'')
(O/'palette.png').write_bytes(png);env['ASSISTED_PNG_FIXTURE']=str(O/'palette.png')
sources=[pathlib.Path('src/activities/apps/lua/LuaRunner.cpp'),pathlib.Path('src/activities/apps/lua/LuaRunner.h'),*pathlib.Path('apps/browser').glob('*'),*pathlib.Path('lib/LuaHttp').glob('*.cpp'),*pathlib.Path('lib/LuaHttp').glob('*.h'),*pathlib.Path('lib/hal').glob('HalBrowserBackend.*')]
def inputs():return {str(p):hashlib.sha256(p.read_bytes()).hexdigest()for p in sources if p.is_file()}
source_before=inputs()
receipts=[]
def run(cmd):
 t=time.monotonic();p=subprocess.run(list(map(str,cmd)),env=env);receipts.append({'cmd':list(map(str,cmd)),'exit':p.returncode,'seconds':round(time.monotonic()-t,3)});(O/'receipt.json').write_text(json.dumps(receipts,indent=2));p.check_returncode()
objs=[]
for src in sorted(pathlib.Path('lib/Lua/src').glob('l*.c')):
 obj=O/(src.stem+'.o');run(['cc',*flags,'-c',src,'-o',obj]);objs.append(obj)
run(['cc',*flags,'-c','lib/miniz/src/miniz_impl.c','-o',O/'miniz.o'])
run(['c++','-std=c++20',*flags,'test/browser/runner.cpp',*objs,'-lm','-ldl','-o',O/'lua'])
for test in ['test/browser_assisted/canonical.lua','test/browser_assisted/png.lua','test/browser_assisted/protocol.lua','test/browser_assisted/maximal.lua','test/browser_assisted/storage.lua','test/browser_assisted/oldhost.lua','test/browser_assisted/client.lua','test/browser_assisted/repair_enums.lua','test/browser_assisted/repair_offline.lua','test/browser/parser.lua','test/browser/layout.lua','test/browser/integration.lua','test/browser/cancel.lua','test/browser/repair.lua']:
 if pathlib.Path(test).exists():run([O/'lua',test])
common=['lib/LuaHttp/HttpCore.cpp','lib/LuaHttp/HttpGzip.cpp',O/'miniz.o']
for name in ['lane','config','framing','repair_transport']:
 if pathlib.Path('test/browser_assisted/'+name+'.cpp').exists():
  run(['c++','-std=c++20',*flags,'test/browser_assisted/'+name+'.cpp','lib/LuaHttp/BackendConfig.cpp',*common,*(['-lz']if name in ('framing','repair_transport')else []),'-o',O/name]);run([O/name,'headers']) if name=='repair_transport' else run([O/name])
  if name=='repair_transport':run([O/name,'caps'])
if pathlib.Path('test/browser_assisted/bindings.cpp').exists():
 run(['c++','-std=c++20',*flags,'test/browser_assisted/bindings.cpp','lib/LuaHttp/LuaHttp.cpp','lib/LuaHttp/BackendConfig.cpp','lib/hal/HalLuaResources.cpp',*common,*objs,'-lm','-ldl','-pthread','-o',O/'bindings']);run([O/'bindings'])

run(['c++','-std=c++20',*flags,'-Itest/browser_assisted/config_stubs','-Ilib/hal','test/browser_assisted/provision.cpp','lib/hal/HalBrowserBackend.cpp','lib/LuaHttp/BackendConfig.cpp',*common,'-o',O/'provision']);run([O/'provision'])

if os.environ.get('BROWSER_CODEC_ROOT'):
 codec=pathlib.Path(os.environ['BROWSER_CODEC_ROOT'])
 imageFlags=[*flags,'-D__LINUX__','-DARDUINO_ARCH_ESP32=1','-DCROSSINK_MEMORY_TEST=1','-DFREEINK_FB_PSRAM=1','-DPNG_MAX_BUFFERED_PIXELS=16416','-Itest/browser/image_stubs','-Itest/memory_policy/stubs','-Ilib/Epub','-Ilib/EpdFont','-Ilib/MemoryBudget','-I'+str(codec/'JPEGDEC/src'),'-I'+str(codec/'PNGdec/src')]
 run(['c++','-std=c++20',*imageFlags,'-fno-sanitize=alignment','-c',codec/'JPEGDEC/src/JPEGDEC.cpp','-o',O/'jpeg.o'])
 pngObjects=[]
 for source in sorted((codec/'PNGdec/src').glob('*.c')):
  obj=O/('png-'+source.stem+'.o');run(['cc',*imageFlags,*(['-fno-sanitize=alignment']if source.name in ('inffast.c','inflate.c')else []),'-c',source,'-o',obj]);pngObjects.append(obj)
 converters=[*pathlib.Path('lib/Epub/Epub/converters').glob('*ToFramebufferConverter.cpp'),'lib/Epub/Epub/converters/ImageDecoderFactory.cpp','lib/Epub/Epub/converters/ImageToFramebufferDecoder.cpp']
 run(['c++','-std=c++20',*imageFlags,'test/browser_assisted/png.cpp','src/activities/apps/lua/BrowserImage.cpp',*converters,O/'jpeg.o',codec/'PNGdec/src/PNGdec.cpp',*pngObjects,'-pthread','-o',O/'png']);run([O/'png'])
 fontFlags=[*imageFlags,'-DBROWSER_FONT_HOST=1','-Ilib/Utf8']
 run(['c++','-std=c++20',*fontFlags,'test/browser/runner.cpp','test/browser/font_host.cpp','lib/EpdFont/EpdFont.cpp','lib/EpdFont/EpdFontFamily.cpp','lib/Utf8/Utf8.cpp',O/'miniz.o',*objs,'-lm','-ldl','-pthread','-o',O/'font-lua'])
 (O/'captures').mkdir(exist_ok=True);env['BROWSER_CAPTURE']=str(O/'captures');run([O/'font-lua','test/browser_assisted/client.lua'])

env['ASSISTED_TEST_OUT']=str(O)
run([sys.executable,'test/browser_assisted/feature.py'])
source_after=inputs();assert source_before==source_after,'Production sources changed during qualification'
(O/'source-inputs.json').write_text(json.dumps(source_after,indent=2)+'\n')
print('PASS source-stable isolated qualification:',O)
