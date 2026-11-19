#!/usr/bin/env python3
"""Execute exact production adapter/parser/wrappers over explicit ESP/HAL doubles.
Not real TLS qualification; JSON helpers are not compiled by this bounded host lane.
"""
import argparse, hashlib, json, os, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
HERE=Path(__file__).resolve().parent
DEFAULT=Path('/root/github/.host-data/crosssmudge/crosspoint-rebase-1655/integration/plugin-transport/tests')
def main():
 p=argparse.ArgumentParser();p.add_argument('--evidence',type=Path,default=DEFAULT);a=p.parse_args();a.evidence.mkdir(parents=True,exist_ok=True)
 src=(ROOT/'src/util/PluginHttp.cpp').read_text()
 helpers=src[src.index('namespace {'):src.index('namespace pluginhttp {')]
 escape=src[src.index('std::string escapeUrl('):src.index('std::string inPluginDir(')]
 wrappers=src[src.index('void setNetworkEnabled('):src.index('bool mintPasswordToken(')]
 unit=a.evidence/'production.cpp';unit.write_text('#include "host.h"\n#include "PluginHttp.h"\n'+helpers+'namespace pluginhttp {\n'+escape+wrappers+'}\n#include "cases.cpp"\n')
 hashes={str(x.relative_to(ROOT)):hashlib.sha256(x.read_bytes()).hexdigest() for x in [ROOT/'src/util/PluginHttp.cpp',ROOT/'src/util/PluginHttp.h',HERE/'host.h',HERE/'ArduinoJson.h',HERE/'WString.h',HERE/'cases.cpp',HERE/'run.py']}
 for mode in ('unqualified-normal','unqualified-sanitize','normal','sanitize'):
  exe=a.evidence/mode
  cmd=['c++','-DCROSSCUB_VERIFIED_PLUGIN_TRANSPORT=1','-std=c++17','-Wall','-Wextra','-Werror','-fno-exceptions','-fno-rtti','-g','-I'+str(HERE),'-I'+str(ROOT/'src/util'),str(unit),'-o',str(exe)]
  if mode.startswith('unqualified-'):cmd.remove('-DCROSSCUB_VERIFIED_PLUGIN_TRANSPORT=1')
  if mode.endswith('sanitize'):cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
  compile=subprocess.run(cmd,capture_output=True,text=True,timeout=90)
  run=None
  if compile.returncode==0:run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=15,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1'})
  log=compile.stdout+compile.stderr+(run.stdout+run.stderr if run else '')
  (a.evidence/(mode+'.log')).write_text(log)
  receipt={'mode':mode,'command':cmd,'compile_exit':compile.returncode,'run_exit':run.returncode if run else None,'source_hashes':hashes,'boundary':'exact extracted production ESP adapter + HTTP parser + buffer/file wrappers over DNS/TLS/HAL doubles; not cryptographic qualification'}
  (a.evidence/(mode+'.json')).write_text(json.dumps(receipt,indent=2)+'\n')
  print(log);assert compile.returncode==0 and run and run.returncode==0,receipt
if __name__=='__main__':main()
