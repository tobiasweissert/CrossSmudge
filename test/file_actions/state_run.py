#!/usr/bin/env python3
"""Offline production CrossPointState + actual ArduinoJson, controlled HalStorage IO."""
from pathlib import Path
import fcntl
import subprocess
import tempfile
import sys
ROOT=Path(__file__).resolve().parents[2]
HERE=Path(__file__).resolve().parent
JSON=Path('/root/github/.host-data/crosssmudge/main-integration/dependency-snapshot/ArduinoJson/src')
def run():
 import argparse
 parser=argparse.ArgumentParser()
 parser.add_argument('--sanitize',action='store_true')
 parser.add_argument('--ui',action='store_true')
 parser.add_argument('--state-source',type=Path,default=ROOT/'src/CrossPointState.cpp')
 parser.add_argument('--browser-source',type=Path)
 args=parser.parse_args()
 with open('/root/.hermes/cache/scratch/crosscub-review-probes.lock','a') as lock:
  try: fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
  except BlockingIOError:
   print('BLOCKED shared compiler lock busy'); return 75
  with tempfile.TemporaryDirectory(prefix='crosscub-state-') as tmp:
   tmp=Path(tmp)
   (tmp/'HalStorage.h').write_text((HERE/'state_boundaries.h').read_text())
   (tmp/'Logging.h').write_text('#pragma once\n#define LOG_ERR(...) ((void)0)\n#define LOG_INF(...) ((void)0)\n#define LOG_DBG(...) ((void)0)\n')
   (tmp/'ObfuscationUtils.h').write_text('#pragma once\n')
   flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if '--sanitize' in sys.argv else []
   subject=HERE/'state_persistence.cpp'
   if '--ui' in sys.argv:
    from state_ui_source import source
    subject=tmp/'state_ui_probe.cpp';subject.write_text(source(args.browser_source))
   command=['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-unused-function','-O0' if args.sanitize else '-O1','-g',*flags,'-DARDUINOJSON_ENABLE_ARDUINO_STRING=0', '-I'+str(tmp),'-I'+str(HERE),'-I'+str(ROOT/'test/persistable_store_atomic/stubs'),'-isystem',str(JSON),'-I'+str(ROOT/'lib/Serialization'),'-I'+str(ROOT/'src'),str(args.state_source),str(ROOT/'lib/Serialization/PersistableStore.cpp'),str(subject),'-o',str(tmp/'probe')]
   print('COMMAND:',' '.join(command),flush=True)
   result=subprocess.run(command,timeout=90)
   if result.returncode:return result.returncode
   if '--ui' in sys.argv:
    return subprocess.run([str(tmp/'probe')],timeout=30).returncode
   snapshot=str(tmp/'snapshot')
   result=subprocess.run([str(tmp/'probe'),snapshot],timeout=30)
   if result.returncode:return result.returncode
   for mode,path in [('--reopen-missing',snapshot),('--reopen-corrupt',snapshot),('--reopen-uncertain',snapshot+'.uncertain')]:
    result=subprocess.run([str(tmp/'probe'),mode,path],timeout=30)
    if result.returncode:return result.returncode
   return 0
if __name__=='__main__':sys.exit(run())
