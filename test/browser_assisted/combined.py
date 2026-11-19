#!/usr/bin/env python3
"""Real service/native/Lua integration; controlled test DNS/dial/CA only.
Use --tcp on the parent host for kernel TCP; default uses real HTTP/TLS over
net.Pipe, explicitly not kernel socket or public TLS proof. No service edits.
"""
import argparse,os,pathlib,json,subprocess,sys,time,hashlib
sys.dont_write_bytecode=True
R=pathlib.Path(__file__).resolve().parents[2];os.chdir(R)
a=argparse.ArgumentParser();a.add_argument('--tcp',action='store_true');a.add_argument('--output',type=pathlib.Path);args=a.parse_args()
S=pathlib.Path(os.environ.get('COMBINED_SCRATCH','/root/.hermes/cache/scratch/sticky-browser-backend'))
Q=pathlib.Path(os.environ.get('ASSISTED_TEST_OUT',S/'normal'))
O=args.output or S/('combined-tcp'if args.tcp else 'combined-pipe')/('sanitized'if os.environ.get('SANITIZE')else 'normal');O.mkdir(parents=True,exist_ok=True);resources=O/('resources-'+str(time.time_ns()));resources.mkdir()
flags=['-O1','-g','-DBOARD_HAS_PSRAM=1','-Ilib/Lua/src','-Ilib/LuaHttp','-Ilib/LuaBrowser','-Ilib/Memory','-Ilib/miniz/src','-Isrc/activities/apps/lua']
if os.environ.get('SANITIZE'):flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
commands=[]
def run(cmd,**kw):
 start=time.monotonic();p=subprocess.run(list(map(str,cmd)),**kw);commands.append({'command':list(map(str,cmd)),'exit':p.returncode,'seconds':time.monotonic()-start});(O/'commands.json').write_text(json.dumps(commands,indent=2)+'\n');p.check_returncode()
codec=pathlib.Path(os.environ.get('BROWSER_CODEC_ROOT','/root/github/crosssmudge-airdrop/.pio/libdeps/sticky-owner-test'))
imageflags=[*flags,'-D__LINUX__','-DARDUINO_ARCH_ESP32=1','-DCROSSINK_MEMORY_TEST=1','-DFREEINK_FB_PSRAM=1','-DPNG_MAX_BUFFERED_PIXELS=16416','-Itest/browser/image_stubs','-Itest/memory_policy/stubs','-Ilib/Epub','-Ilib/EpdFont','-Ilib/MemoryBudget','-Ilib/Utf8','-I'+str(codec/'JPEGDEC/src'),'-I'+str(codec/'PNGdec/src')]
extra=[]
for src in ['test/browser_assisted/combined_images.cpp','test/browser/font_host.cpp','lib/EpdFont/EpdFont.cpp','lib/EpdFont/EpdFontFamily.cpp','lib/Utf8/Utf8.cpp','src/activities/apps/lua/BrowserImage.cpp',*sorted((R/'lib/Epub/Epub/converters').glob('*ToFramebufferConverter.cpp')),'lib/Epub/Epub/converters/ImageDecoderFactory.cpp','lib/Epub/Epub/converters/ImageToFramebufferDecoder.cpp',codec/'PNGdec/src/PNGdec.cpp']:
 obj=O/(pathlib.Path(src).stem+'.o');run(['c++','-std=c++20',*imageflags,'-c',src,'-o',obj]);extra.append(obj)
cmd=['c++','-std=c++20',*flags,'-DBROWSER_FONT_HOST=1','-DCOMBINED_CODEC_HOST=1','test/browser_assisted/replay_runner.cpp','lib/LuaHttp/LuaHttp.cpp','lib/LuaHttp/HttpCore.cpp','lib/LuaHttp/HttpGzip.cpp','lib/hal/HalLuaResources.cpp',*extra,Q/'jpeg.o',*sorted(Q.glob('png-*.o')),Q/'miniz.o',*[Q/(p.stem+'.o')for p in sorted((R/'lib/Lua/src').glob('l*.c'))],'-pthread','-lm','-ldl','-o',O/'native']
run(cmd)
prefix=(R/'test/browser_assisted/client.lua').read_text().split("local J=dofile('json.lua')",1)[0]
(O/'driver.lua').write_text(prefix+(R/'test/browser_assisted/combined.lua').read_text())
virtual=R/'services/sticky-reader/internal/service/combined_integration_test.go'
(O/'overlay.json').write_text(json.dumps({'Replace':{str(virtual):str(R/'test/browser_assisted/combined_service_test.go')}}))
env=dict(os.environ,COMBINED_ROOT=str(R),COMBINED_NATIVE=str(O/'native'),COMBINED_DRIVER=str(O/'driver.lua'),COMBINED_OUT=str(O),COMBINED_RESOURCES=str(resources),GOMAXPROCS='1',GOPROXY='off',GOMODCACHE=str(S/'go-mod-cache'),GOCACHE=str(S/'go-cache'),GOTMPDIR=str(S),TMPDIR=str(S),ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1',SMUDGE_X3_CONSTRAINTS='1')
if args.tcp:env['SRP_TCP_FIXTURES']='1'
else:env.pop('SRP_TCP_FIXTURES',None)
run(['go','test','-mod=readonly','-overlay',O/'overlay.json','-p=1','-count=1','-timeout=100s','-run','^(TestCombinedNativeLua|TestWorkerProcess)$','-v','./internal/service'],cwd=R/'services/sticky-reader',env=env)
(O/'inputs.json').write_text(json.dumps({str(p.relative_to(R)):hashlib.sha256(p.read_bytes()).hexdigest()for p in [R/'test/browser_assisted/combined.lua',R/'test/browser_assisted/combined_service_test.go',R/'test/browser_assisted/replay_runner.cpp',*sorted((R/'apps/browser').glob('*'))]if p.is_file()},indent=2)+'\n')
