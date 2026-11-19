#!/usr/bin/env python3
"""Offline real-declaration host compiler driver; hardware doubles only."""
import argparse, fcntl, hashlib, json, os, pathlib, subprocess, tempfile
R=pathlib.Path(__file__).resolve().parents[3]
H=pathlib.Path(__file__).resolve().parent
S=R/'freeink-sdk'
DEPS=pathlib.Path('/root/github/crosspoint-reader/.pio/libdeps/sticky')
SIM=pathlib.Path('/root/github/crosssmudge/.pio/libdeps/sticky-simulator/simulator/src')
E=pathlib.Path('/root/github/.host-data/crosssmudge/crosspoint-rebase-1655/integration/target-adapters')
def main():
 global E
 p=argparse.ArgumentParser();p.add_argument('name');p.add_argument('sources',nargs='+');p.add_argument('--syntax',action='store_true');p.add_argument('--sanitize',action='store_true');p.add_argument('--evidence',type=pathlib.Path,default=E);a=p.parse_args()
 E=a.evidence
 E.mkdir(parents=True,exist_ok=True)
 with open('/root/.hermes/cache/scratch/crosspoint-qualification.lock','w') as lock:
  fcntl.flock(lock,fcntl.LOCK_EX)
  with tempfile.TemporaryDirectory(prefix='target-adapters-',dir=os.environ['TMPDIR']) as tmp:
   tmp=pathlib.Path(tmp)
   inc=[H/'hardware',R/'src',R/'include',R/'lib/hal',DEPS/'SdFat/src',DEPS/'ArduinoJson/src',SIM]
   inc += [x for x in (R/'lib').iterdir() if x.is_dir()]
   inc += sorted((R/'lib').rglob('include'))
   inc += sorted(x for x in (S/'libs').rglob('include') if x.is_dir())
   inc += [S/'libs/display/FreeInkDisplay/src']
   inc.remove(SIM); inc.append(SIM)
   import importlib.util
   spec=importlib.util.spec_from_file_location('gen',R/'scripts/gen_i18n.py');gen=importlib.util.module_from_spec(spec);spec.loader.exec_module(gen)
   loaded=gen.load_translations(str(R/'lib/I18n/translations'))
   gen.generate_keys_header(*loaded[:4], str(tmp/'I18nKeys.h'), False, {'EN'})
   inc.insert(1,tmp)
   def method(path,signature):
    text=(R/path).read_text(); start=text.index(signature); first=text.index('{',start);depth=1;end=first+1
    while depth:
     depth += (text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]
   support='#include <GfxRenderer.h>\n#include <CrossPointSettings.h>\n'
   for sig in ['int GfxRenderer::getScreenWidth()', 'int GfxRenderer::getScreenHeight()', 'void GfxRenderer::tapToLogical(', 'void GfxRenderer::freeBwBufferChunks()', 'void GfxRenderer::freeBitmapScratchBuffers()']:
    support+=method('lib/GfxRenderer/GfxRenderer.cpp',sig)+'\n'
   support+=method('src/CrossPointSettings.cpp','uint8_t CrossPointSettings::defaultUiScale()')
   # Emit real theme RTTI for sanitizer references in the input translation unit.
   # The theme key function is not exercised; display/GPIO remain external.
   support+='\n#include "components/themes/BaseTheme.h"\n#include <HalGPIO.h>\nextern HalGPIO gpio;\n'
   support+=method('src/components/themes/BaseTheme.cpp','void BaseTheme::fillBatteryIcon(')
   (tmp/'support.cpp').write_text(support)
   if not a.syntax:a.sources += [str(tmp/'support.cpp'),str(R/'src/components/TouchRegistry.cpp')]
   gtest=pathlib.Path('/root/.hermes/cache/scratch/library-finish-deps/googletest-1.17.0/googletest')
   inc += [gtest/'include',gtest]
   cmd=['g++','-std=c++20','-DFREEINK_DEVICE_STICKY','-DCROSSPOINT_EMULATED=0','-DARDUINOJSON_ENABLE_ARDUINO_STRING=0','-DARDUINOJSON_ENABLE_ARDUINO_STREAM=0','-DARDUINOJSON_ENABLE_PROGMEM=0','-DENABLE_ARDUINO_FEATURES=0','-DCROSSSMUDGE_APP_CAP_TOUCH=1','-DCROSSINK_APP_CAP_USB_DRIVE=0','-DSPI_DRIVER_SELECT=3','-DENABLE_ARDUINO_SERIAL=0','-DUSE_FCNTL_H=0','-ffunction-sections','-fdata-sections','-g']
   cmd += ['-I'+str(x) for x in inc]
   if a.sanitize:cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
   cmd+=a.sources
   if a.syntax:cmd+=['-fsyntax-only']
   else:cmd+=['-Wl,--gc-sections','-o',str(tmp/'tests')]
   result=subprocess.run(cmd,capture_output=True,text=True)
   (E/(a.name+'.log')).write_text(result.stdout+result.stderr)
   record={'command':cmd,'compile_exit':result.returncode}
   if result.returncode==0 and not a.syntax:
    record['binary_sha256']=hashlib.sha256((tmp/'tests').read_bytes()).hexdigest()
    run=subprocess.run([str(tmp/'tests'),'--gtest_output=xml:'+str(E/(a.name+'.xml'))],capture_output=True,text=True)
    record['native_exit']=run.returncode
    (E/(a.name+'-run.log')).write_text(run.stdout+run.stderr)
    print(run.stdout+run.stderr)
   (E/(a.name+'.json')).write_text(json.dumps(record,indent=2)+'\n')
   print(result.stdout+result.stderr);print(json.dumps(record))
   return record.get('native_exit',result.returncode)
if __name__=='__main__':raise SystemExit(main())
