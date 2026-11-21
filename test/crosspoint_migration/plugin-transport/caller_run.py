#!/usr/bin/env python3
"""Bounded real Activity methods + pinned SDK parser, explicit host seams.
Fixture source JSON is never modified. Not installed-binary/ESP TLS evidence.
"""
import argparse, hashlib, json, os, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
HERE=Path(__file__).resolve().parent
SDK=Path('/root/github/crosscub-crosspoint-rebase/freeink-sdk')
JSON=Path('/root/github/.host-data/crosssmudge/crosspoint-rebase-1655/host-deps/arduinojson-src/src')
def function(src, signature):
 start=src.index(signature)
 return src[start:src.index('\n}',start)+2]+'\n'
def main():
 p=argparse.ArgumentParser();p.add_argument('--evidence',type=Path,required=True);p.add_argument('--fixtures',type=Path,required=True);a=p.parse_args();a.evidence.mkdir(parents=True,exist_ok=True)
 s=(ROOT/'src/util/PluginHttp.cpp').read_text();c=(ROOT/'src/activities/plugins/PluginCatalogActivity.cpp').read_text();h=(ROOT/'src/activities/plugins/PluginCatalogActivity.h').read_text()
 parser=SDK/'libs/network/CatalogList/src/JsonListParser.cpp'
 skeleton=(HERE/'caller-skeleton.inc').read_text()
 policy=c[c.index('const std::string& pick('):c.index('// Plugin folder name')]+function(c,'std::string persistentNoticeId(')
 types=h[h.index('  struct Manifest {'):h.index('  struct ItemDetails {')]
 methods='\n'.join(function(c,sig) for sig in ['bool PluginCatalogActivity::loadManifest(', 'bool PluginCatalogActivity::loadBrowseListIndex(', 'void PluginCatalogActivity::openList(', 'bool PluginCatalogActivity::wantsListPicker(', 'void PluginCatalogActivity::startBrowse(', 'bool PluginCatalogActivity::fetchBrowseResponse(', 'void PluginCatalogActivity::fetchPage(', 'void PluginCatalogActivity::retryBrowse(', 'const std::string& PluginCatalogActivity::activeBrowseUrl(', 'const std::string& PluginCatalogActivity::activeBrowseBody(', 'bool PluginCatalogActivity::parseUpdateFirstBrowseResponse('])
 skeleton=skeleton.replace('// @PRODUCTION_POLICY@',policy).replace('// @PRODUCTION_TYPES@',types).replace('// @PRODUCTION_METHODS@',methods)
 helpers=s[s.index('namespace {'):s.index('namespace pluginhttp {')]
 jsonhelpers=s[s.index('namespace pluginhttp {'):s.index('// A manifest may not')]
 wrappers=s[s.index('void setNetworkEnabled('):s.index('bool mintPasswordToken(')]
 unit=a.evidence/'caller-production.cpp';unit.write_text('#include "host.h"\n#include <ArduinoJson.h>\n#include "PluginHttp.h"\n'+helpers+jsonhelpers+wrappers+'}\n'+skeleton)
 inputs=[ROOT/'src/util/PluginHttp.cpp',ROOT/'src/util/PluginHttp.h',ROOT/'src/activities/plugins/PluginCatalogActivity.cpp',ROOT/'src/activities/plugins/PluginCatalogActivity.h',ROOT/'src/util/FirmwareIdentity.h',HERE/'host.h',HERE/'caller-skeleton.inc',HERE/'caller_run.py',parser]
 fixtures=[a.fixtures/'v0.1.17-device.json',a.fixtures/'catalog-lists.json',a.fixtures/'community-catalog.json']
 hashes={str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in inputs+fixtures}
 for mode in ('unqualified-normal','unqualified-sanitize','normal','sanitize'):
  exe=a.evidence/mode
  cmd=['c++','-D_GLIBCXX_ASSERTIONS','-DARDUINOJSON_ENABLE_ARDUINO_STRING=0','-std=c++17','-Wall','-Wextra','-Werror','-fno-exceptions','-fno-rtti','-g','-I'+str(JSON),'-I'+str(HERE),'-I'+str(ROOT/'src'),'-I'+str(ROOT/'src/util'),'-I'+str(SDK/'libs/network/CatalogList/include'),'-I'+str(SDK/'libs/network/JsonSax/include'),str(unit),str(parser),str(SDK/'libs/network/JsonSax/src/StreamingJsonParser.cpp'),'-o',str(exe)]
  if not mode.startswith('unqualified-'):cmd+=['-DCROSSCUB_VERIFIED_PLUGIN_TRANSPORT=1']
  if mode.endswith('sanitize'):cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
  compile=subprocess.run(cmd,capture_output=True,text=True,timeout=90)
  run=None
  if compile.returncode==0:run=subprocess.run([str(exe),*map(str,fixtures)],capture_output=True,text=True,timeout=15,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1'})
  log=compile.stdout+compile.stderr+(run.stdout+run.stderr if run else '')
  (a.evidence/(mode+'.log')).write_text(log)
  receipt={'mode':mode,'command':cmd,'compile_exit':compile.returncode,'run_exit':run.returncode if run else None,'source_hashes':hashes,'boundary':'verbatim Activity/helper/ranking bodies + actual SDK JsonListParser; transport/UI/storage/auth/platform seams; not installed binary or ESP crypto'}
  (a.evidence/(mode+'.json')).write_text(json.dumps(receipt,indent=2)+'\n')
  print(log);assert compile.returncode==0 and run and run.returncode==0,receipt
  exe.unlink()
if __name__=='__main__':main()
