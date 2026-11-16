#!/usr/bin/env python3
"""Routine browser regression entrypoint. No downloads or target builds."""
import os,pathlib,subprocess,tempfile,json,time
R=pathlib.Path(__file__).resolve().parents[2];os.chdir(R)
OUT=pathlib.Path(os.environ.get('BROWSER_TEST_OUT',str(R/'.pio/browser-tests')));OUT.mkdir(parents=True,exist_ok=True)
subprocess.run(['python3','test/browser_main/allocator.py'],check=True)
candidates=[os.environ.get('ARDUINOJSON_INCLUDE',''),str(R/'.pio/libdeps/sticky-owner-test/ArduinoJson/src'),'/root/github/crosssmudge-appstore-fix/.pio/libdeps/sticky-owner-test/ArduinoJson/src']
JSON=next((p for p in candidates if p and pathlib.Path(p,'ArduinoJson.h').is_file()),None)
if not JSON:raise SystemExit('Set ARDUINOJSON_INCLUDE to an existing pinned ArduinoJson/src; no package installs performed')
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1',SMUDGE_X3_CONSTRAINTS='1')
flags=['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Ilib/Memory','-Ilib/miniz/src','-Ilib/LuaBrowser','-Ilib/Lua/src','-Ilib/LuaHttp','-Isrc/activities/apps/lua','-I'+JSON]
receipts=[]
def run(argv,**kw):
 argv=list(map(str,argv));start=time.time()
 result=subprocess.run(argv,env=env,**kw)
 receipts.append({'command':argv,'exit':result.returncode,'seconds':round(time.time()-start,3)})
 if os.environ.get('BROWSER_TEST_RECEIPT'):
  pathlib.Path(os.environ['BROWSER_TEST_RECEIPT']).write_text(json.dumps(receipts,indent=2)+'\n')
 result.check_returncode()
objects=[]
for source in sorted(pathlib.Path('lib/Lua/src').glob('l*.c')):
 obj=OUT/(source.stem+'.o');run(['cc',*flags,'-c',source,'-o',obj]);objects.append(obj)
run(['cc',*flags,'-c','lib/miniz/src/miniz_impl.c','-o',OUT/'miniz.o'])
run(['c++','-std=c++20',*flags,'test/browser/runner.cpp',*objects,'-lm','-ldl','-o',OUT/'lua'])
for suite in ('parser','layout','integration','advisory','source','cancel','work','css_budget','repair'):
 run([OUT/'lua',f'test/browser/{suite}.lua'])
for suite,source,extra in [('resource','test/browser/resource.cpp',[]),('redirects','test/browser/redirects.cpp',[]),('framing','test/browser/framing.cpp',['-lz']),('memory','test/browser/memory.cpp',[]),('transport','test/browser/transport.cpp',[]),('api3-policy','test/lua_http/policy_test.cpp',[]),('api3-protocol','test/lua_http/protocol_test.cpp',[]),('api3-protocol-psram','test/lua_http/protocol_test.cpp',['-DBOARD_HAS_PSRAM=1']),('api3-budget','test/lua_http/budget_test.cpp',[]),('api3-manifest','test/lua_http/manifest_test.cpp',[]),('bindings','test/browser/bindings.cpp',['lib/LuaHttp/LuaHttp.cpp',*objects,'-lm','-ldl']),('api3-async','test/lua_http/async_test.cpp',['lib/LuaHttp/LuaHttp.cpp',*objects,'-lm','-ldl','-pthread'])]:
 run(['c++','-std=c++20',*flags,source,'lib/LuaHttp/HttpCore.cpp','lib/LuaHttp/HttpGzip.cpp','lib/hal/HalLuaResources.cpp',OUT/'miniz.o',*extra,'-o',OUT/suite]);run([OUT/suite])
run(['c++','-std=c++20','-Itest/browser/activity_stubs',*flags,'-Isrc','test/browser/activity.cpp','lib/AirDrop/browser/BrowserLaunch.cpp','lib/AirDrop/core/Offer.cpp','lib/LuaHttp/LuaHttp.cpp','lib/LuaHttp/HttpCore.cpp','lib/LuaHttp/HttpGzip.cpp','lib/hal/HalLuaResources.cpp',OUT/'miniz.o',*objects,'-lm','-ldl','-pthread','-o',OUT/'activity'])
run([OUT/'activity'])
low=subprocess.run([OUT/'lua','test/browser/parser.lua'],env=dict(env,BROWSER_LUA_LIMIT='76800'),capture_output=True,text=True)
assert low.returncode==1 and 'memory' in low.stderr,low.stdout+low.stderr
receipts.append({'command':[str(OUT/'lua'),'test/browser/parser.lua'],'expected_oom':True,'exit':low.returncode})
print('PASS browser + API3 regressions; ASan/UBSan; exact int32/float32 Lua; bounded OOM; LSan disabled under ptrace',flush=True)

# Compile the actual pinned codecs and production converters with Sticky macros.
codec=pathlib.Path(os.environ.get('BROWSER_CODEC_ROOT',R/'.pio/libdeps/sticky-owner-test'))
assert (codec/'JPEGDEC/src/JPEGDEC.cpp').is_file(), 'Provision the pinned target libdeps first'
assert 'iBitNum >= 5 &&' in (codec/'JPEGDEC/src/jpeg.inl').read_text(), 'Apply pinned JPEG patches via PlatformIO first'
imageFlags=[*flags,'-D__LINUX__','-DARDUINO_ARCH_ESP32=1','-DCROSSINK_MEMORY_TEST=1','-DBOARD_HAS_PSRAM=1','-DFREEINK_FB_PSRAM=1','-DPNG_MAX_BUFFERED_PIXELS=16416','-Itest/browser/image_stubs','-Itest/memory_policy/stubs','-Ilib/Epub','-Ilib/EpdFont','-Ilib/MemoryBudget','-I'+str(codec/'JPEGDEC/src'),'-I'+str(codec/'PNGdec/src')]
# JPEGDEC intentionally uses unaligned x86 loads. Only that vendor object
# suppresses alignment; all owned code retains complete ASan/UBSan checks.
run(['c++','-std=c++20',*imageFlags,'-fno-sanitize=alignment','-c',codec/'JPEGDEC/src/JPEGDEC.cpp','-o',OUT/'jpeg.o'])
pngObjects=[]
for source in sorted((codec/'PNGdec/src').glob('*.c')):
 obj=OUT/('png-'+source.stem+'.o');run(['cc',*imageFlags,*(['-fno-sanitize=alignment']if source.name in ('inffast.c','inflate.c')else []),'-c',source,'-o',obj]);pngObjects.append(obj)
run(['c++','-std=c++20',*imageFlags,'test/browser/images.cpp','src/activities/apps/lua/BrowserImage.cpp',*pathlib.Path('lib/Epub/Epub/converters').glob('*ToFramebufferConverter.cpp'),'lib/Epub/Epub/converters/ImageDecoderFactory.cpp','lib/Epub/Epub/converters/ImageToFramebufferDecoder.cpp',OUT/'jpeg.o',codec/'PNGdec/src/PNGdec.cpp',*pngObjects,'-pthread','-o',OUT/'images'])
run([OUT/'images'])
fontFlags=[*imageFlags,'-DBROWSER_FONT_HOST=1','-DBROWSER_IMAGE_HOST=1','-Ilib/Utf8']
run(['c++','-std=c++20',*fontFlags,'test/browser/runner.cpp','test/browser/font_host.cpp','lib/EpdFont/EpdFont.cpp','lib/EpdFont/EpdFontFamily.cpp','lib/Utf8/Utf8.cpp','src/activities/apps/lua/BrowserImage.cpp',*pathlib.Path('lib/Epub/Epub/converters').glob('*ToFramebufferConverter.cpp'),'lib/Epub/Epub/converters/ImageDecoderFactory.cpp','lib/Epub/Epub/converters/ImageToFramebufferDecoder.cpp',OUT/'jpeg.o',codec/'PNGdec/src/PNGdec.cpp',*pngObjects,OUT/'miniz.o',*objects,'-lm','-ldl','-pthread','-o',OUT/'font-lua'])
run([OUT/'font-lua','test/browser/integration.lua'])

if os.environ.get('BROWSER_TEST_RECEIPT'):
 pathlib.Path(os.environ['BROWSER_TEST_RECEIPT']).write_text(json.dumps(receipts,indent=2)+'\n')
