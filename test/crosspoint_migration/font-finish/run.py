#!/usr/bin/env python3
"""Bounded real production probes; peripheral adapters are explicitly doubles."""
import argparse, json, os, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
p=argparse.ArgumentParser();p.add_argument('--evidence',required=True);p.add_argument('--sanitize',action='store_true');args=p.parse_args()
e=Path(args.evidence);e.mkdir(parents=True,exist_ok=True)
flags=['g++','-std=c++20','-g','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers','-fno-exceptions','-fno-rtti','-include','cstring']
if args.sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
receipts=[]
import hashlib
owned=['lib/EpdFont/SdCardFont.cpp','lib/EpdFont/SdCardFont.h','lib/EpdFont/SdCardFontManager.cpp','lib/EpdFont/SdCardFontManager.h','lib/EpdFont/SdCardFontRegistry.cpp','lib/EpdFont/SdCardFontRegistry.h','lib/EpdFont/scripts/sd-fonts.yaml','lib/GfxRenderer/FontCacheManager.cpp','lib/GfxRenderer/FontCacheManager.h','lib/GfxRenderer/GfxRenderer.cpp','lib/GfxRenderer/GfxRenderer.h','src/FontInstaller.cpp','src/SdCardFontSystem.cpp','src/SdCardFontSystem.h','src/TtfRenderProfileStore.cpp','src/TtfRenderProfileStore.h']
inputs=[ROOT/i for i in owned]+[i for i in Path(__file__).parent.rglob('*') if i.is_file() and '__pycache__' not in str(i)]
(e/'source-hashes.json').write_text(json.dumps({str(i.relative_to(ROOT)):hashlib.sha256(i.read_bytes()).hexdigest() for i in inputs},indent=2))
def run(name,cmd):
 r=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=110)
 (e/(name+'.log')).write_text(r.stdout);receipts.append({'name':name,'argv':cmd,'exit':r.returncode});print(name,r.returncode,flush=True)
 (e/'receipts.json').write_text(json.dumps(receipts,indent=2))
 if r.returncode: print(r.stdout[-5000:]);raise SystemExit(r.returncode)
incs=['test/sd_card_font/stubs','lib/EpdFont','lib/Utf8','lib/Memory','lib/MemoryBudget']
src=['test/crosspoint_migration/font-finish/sd_probe.cpp','lib/EpdFont/SdCardFont.cpp','lib/EpdFont/EpdFont.cpp','lib/Utf8/Utf8.cpp']
run('sd-compile',flags+['-I'+str(ROOT/i) for i in incs]+[str(ROOT/i) for i in src]+['-o',str(e/'sd-probe')])
run('sd-execute',[str(e/'sd-probe')])
incs=['test/crosspoint_migration/font-finish/catalog-stubs','test/font_catalog_index/stubs','lib/EpdFont']
src=['test/crosspoint_migration/font-finish/catalog_probe.cpp','lib/EpdFont/SdCardFontRegistry.cpp']
run('catalog-compile',flags+['-DBOARD_HAS_PSRAM']+['-I'+str(ROOT/i) for i in incs]+[str(ROOT/i) for i in src]+['-o',str(e/'catalog-probe')])
import tempfile
with tempfile.TemporaryDirectory(prefix='font-catalog-',dir=os.environ.get('TMPDIR')) as tmp:
 run('catalog-execute',[str(e/'catalog-probe'),tmp])
incs=['test/crosspoint_migration/font-finish/renderer-stubs','lib/GfxRenderer','lib/EpdFont','test/epub_grayscale/raster_stubs','test/memory_policy/stubs','test/sd_card_font/stubs','lib/Memory','lib/MemoryBudget','lib/Utf8','lib/MiniBidi','lib/miniz/src','lib/InflateReader','lib/uzlib/src']
src=['test/crosspoint_migration/font-finish/renderer_probe.cpp','lib/GfxRenderer/GfxRenderer.cpp','lib/GfxRenderer/Bitmap.cpp','lib/GfxRenderer/BitmapHelpers.cpp','lib/GfxRenderer/FontCacheManager.cpp','lib/EpdFont/EpdFont.cpp','lib/EpdFont/EpdFontFamily.cpp','lib/EpdFont/SdCardFont.cpp','lib/EpdFont/FontDecompressor.cpp','lib/Memory/BuildScratch.cpp','lib/Utf8/Utf8.cpp','lib/MiniBidi/BidiUtils.cpp','lib/MiniBidi/minibidi.c','lib/InflateReader/InflateReader.cpp','lib/uzlib/src/tinflate.c','lib/uzlib/src/uzlib_checksums.c']
objects=[]
for i,s in enumerate(src[:]):
 if s.endswith('.c'):
  obj=str(e/('c'+str(i)+'.o'))
  run('c-compile-'+str(i),['gcc','-std=c11','-g']+(['-fsanitize=address,undefined','-fno-pie'] if args.sanitize else [])+['-I'+str(ROOT/j) for j in incs]+['-c',str(ROOT/s),'-o',obj]);objects.append(obj);src.remove(s)
run('renderer-compile',flags+['-I'+str(ROOT/i) for i in incs]+[str(ROOT/i) for i in src]+objects+['-o',str(e/'renderer-probe')])
run('renderer-execute',[str(e/'renderer-probe')])
incs=['test/font_catalog_index/stubs','src']
run('profile-compile',flags+['-I'+str(ROOT/i) for i in incs]+[str(ROOT/'test/crosspoint_migration/font-finish/profile_probe.cpp'),str(ROOT/'src/TtfRenderProfileStore.cpp'),'-o',str(e/'profile-probe')])
with tempfile.TemporaryDirectory(prefix='font-profile-',dir=os.environ.get('TMPDIR')) as tmp:
 run('profile-execute',[str(e/'profile-probe'),tmp])
incs=['test/crosspoint_migration/font-finish/system-stubs','test/crosspoint_migration/font-finish/renderer-stubs','test/crosspoint_migration/font-finish/catalog-stubs','lib/GfxRenderer','lib/EpdFont','test/epub_grayscale/raster_stubs','test/memory_policy/stubs','test/sd_card_font/stubs','test/crosspoint_migration/settings/stubs','lib/Memory','lib/MemoryBudget','lib/MemoryBudget','lib/Serialization','src']
incs+=['/root/github/.host-data/crosssmudge/gallery-native-deps/sticky-simulator/ArduinoJson/src']
for vector in [False,True]:
 run('system-syntax-'+str(int(vector)),flags+['-fsyntax-only','-DARDUINOJSON_ENABLE_ARDUINO_STRING=0']+(['-DBOARD_HAS_PSRAM'] if vector else [])+['-I'+str(ROOT/i) for i in incs]+[str(ROOT/'src/SdCardFontSystem.cpp'),str(ROOT/'src/FontInstaller.cpp'),str(ROOT/'lib/EpdFont/SdCardFontManager.cpp')])
run('installer-compile',flags+['-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-DARDUINOJSON_ENABLE_ARDUINO_STRING=0']+['-I'+str(ROOT/i) for i in incs]+[str(ROOT/'test/crosspoint_migration/font-finish/installer_probe.cpp'),str(ROOT/'src/FontInstaller.cpp'),'-o',str(e/'installer-probe')])
run('installer-execute',[str(e/'installer-probe')])
