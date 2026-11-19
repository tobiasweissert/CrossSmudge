#!/usr/bin/env python3
"""Bounded production codec qualification. No SDK init, installs or firmware build."""
import argparse, hashlib, json, os, pathlib, struct, subprocess, time, zlib
R=pathlib.Path(__file__).resolve().parents[3]; T=pathlib.Path(__file__).parent
p=argparse.ArgumentParser();p.add_argument('--output',type=pathlib.Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();O=a.output.resolve();O.mkdir(parents=True,exist_ok=True)
C=pathlib.Path(os.environ.get('READER_CODEC_ROOT','/root/github/crosssmudge-appstore-fix/.pio/libdeps/sticky-owner-test'))
receipts=[]
def run(cmd):
 cmd=list(map(str,cmd));start=time.monotonic();print('RUN',json.dumps(cmd),flush=True)
 try:
  result=subprocess.run(cmd,cwd=R,timeout=120,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1','UBSAN_OPTIONS':'halt_on_error=1'},capture_output=True,text=True)
  rc=result.returncode;out=result.stdout+result.stderr
 except subprocess.TimeoutExpired as e:rc=124;out=str(e)
 f=O/(str(len(receipts))+'.log');f.write_text(out);print(out[-12000:],flush=True)
 receipts.append({'command':cmd,'exit':rc,'seconds':round(time.monotonic()-start,3),'log':str(f)})
 (O/'receipts.json').write_text(json.dumps(receipts,indent=2)+'\n')
 if rc:raise SystemExit(rc)
def png(name,depth,kind,row,extra=b'',w=64,h=16):
 def chunk(k,v):return struct.pack('>I',len(v))+k+v+struct.pack('>I',zlib.crc32(k+v))
 raw=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,depth,kind,0,0,0))+extra+chunk(b'IDAT',zlib.compress((b'\0'+row)*h))+chunk(b'IEND',b'')
 (O/name).write_bytes(raw)
def chunk(k,v):return struct.pack('>I',len(v))+k+v+struct.pack('>I',zlib.crc32(k+v))
png('mid.png',8,0,bytes([128])*64)
png('rgba.png',8,6,bytes([0,0,0,0])*64)
png('grayalpha16.png',16,4,bytes([0,0,0,0])*64)
png('graykey16.png',16,0,bytes([0x12,0x34])*64,chunk(b'tRNS',bytes([0x12,0x34])))
png('graynotkey16.png',16,0,bytes([0x12,0x35])*64,chunk(b'tRNS',bytes([0x12,0x34])))
png('rgbkey16.png',16,2,bytes([0x12,0x34,0x23,0x45,0x34,0x56])*64,chunk(b'tRNS',bytes([0x12,0x34,0x23,0x45,0x34,0x56])))
png('pal.png',1,3,bytes([0])*8,chunk(b'PLTE',bytes([0,0,0,255,255,255]))+chunk(b'tRNS',bytes([0,255])))
png('baddepth.png',3,0,bytes([0])*24)
flags=['-O1','-g0','-fno-omit-frame-pointer','-D__LINUX__','-DARDUINO_ARCH_ESP32=1','-DCROSSINK_MEMORY_TEST=1','-DBOARD_HAS_PSRAM=1','-DFREEINK_FB_PSRAM=1','-DPNG_MAX_BUFFERED_PIXELS=16416']
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-pie','-no-pie']
for inc in [T/'stubs',R/'test/browser/image_stubs',R/'test/memory_policy/stubs',R/'lib/Epub',R/'lib/Memory',R/'lib/MemoryBudget',R/'lib/GfxRenderer',R/'lib/EpdFont',R/'lib/miniz/src',R/'lib/ZipFile',R/'lib/PngToBmpConverter',R/'lib/JpegToBmpConverter',C/'JPEGDEC/src',C/'PNGdec/src']:
 flags+=['-I'+str(inc)]
objects=[]
for src in [R/'lib/miniz/src/miniz_impl.c',C/'JPEGDEC/src/JPEGDEC.cpp',C/'PNGdec/src/PNGdec.cpp',*sorted((C/'PNGdec/src').glob('*.c'))]:
 obj=O/(src.name+'.o');extra=['-fno-sanitize=alignment'] if src.name in ['JPEGDEC.cpp','inffast.c','inflate.c'] else []
 run(['c++','-std=c++20',*flags,*extra,'-c',src,'-o',obj] if src.suffix=='.cpp' else ['cc',*flags,*extra,'-c',src,'-o',obj]);objects.append(obj)
sources=[T/'codecs.cpp',R/'lib/Epub/Epub/converters/JpegToFramebufferConverter.cpp',R/'lib/Epub/Epub/converters/PngToFramebufferConverter.cpp',R/'lib/Epub/Epub/converters/ImageToFramebufferDecoder.cpp',R/'lib/Epub/Epub/converters/ImageDimsProbe.cpp',R/'lib/PngToBmpConverter/PngToBmpConverter.cpp',R/'lib/JpegToBmpConverter/JpegToBmpConverter.cpp',R/'lib/GfxRenderer/BitmapHelpers.cpp',R/'lib/miniz/src/InflateStream.cpp',R/'lib/Memory/BuildScratch.cpp',R/'lib/ZipFile/ZipFile.cpp']
files=[*sources,R/'lib/Epub/Epub/converters/DirectPixelWriter.h',R/'lib/Epub/Epub/converters/PixelCache.h',R/'lib/miniz/src/InflateStream.h',R/'lib/miniz/third_party/miniz.h',*C.joinpath('JPEGDEC/src').glob('*'),*C.joinpath('PNGdec/src').glob('*')]
(O/'inputs.json').write_text(json.dumps({str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files if f.is_file()},indent=2)+'\n')
run(['c++','-std=c++20',*flags,*sources,*objects,'-lz','-pthread','-o',O/'codecs'])
run([O/'codecs',O])
print('PASS real production codec/ZIP core; dependent hardware doubles; '+('ASan/UBSan' if a.sanitize else 'normal'),flush=True)
# Keep logs/receipts, not regenerable object/executable bulk on the constrained host.
for f in [*objects,O/'codecs']:f.unlink()
