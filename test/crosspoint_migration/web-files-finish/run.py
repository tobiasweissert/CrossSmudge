#!/usr/bin/env python3
"""Actual production web/settings and file transaction bodies over explicit host doubles."""
from pathlib import Path
import argparse,hashlib,json,subprocess
R=Path(__file__).resolve().parents[3];T=Path(__file__).resolve().parent
p=argparse.ArgumentParser();p.add_argument('--evidence',required=True,type=Path);p.add_argument('--sanitize',action='store_true');a=p.parse_args();O=a.evidence;O.mkdir(parents=True,exist_ok=True)
def method(path,sig):
 text=(R/path).read_text();start=text.index(sig);return text[start:text.index('\n}',start)+2]
def strip(path):return '\n'.join(l for l in (R/path).read_text().splitlines() if not l.startswith('#'))
info=(R/'src/activities/settings/SettingsActivity.h').read_text();info=info[info.index('enum class SettingType'):info.index('class SettingsActivity')]
web='\n'.join(method('src/network/CrossPointWebServer.cpp',x) for x in ['void CrossPointWebServer::handleGetSettings()', 'void CrossPointWebServer::handlePostSettings()'])
source=(T/'settings.cpp').read_text().replace('// SETTING_INFO',info).replace('// WEB_METHODS',web)
move='src/util/BookMoveUtils.cpp'
body=strip('src/util/BookMoveUtils.h')+'\nnamespace {\nconstexpr char BOOK_MOVE_PENDING[]="/.crosspoint/book-move.pending";\n'
body+='\n'.join(method(move,x) for x in ['bool writeMoveIntent(', 'bool getCachePath('])+'\n}\nnamespace BookMoveUtils {\n'
body+='\n'.join(method(move,x) for x in ['RenameMigrationResult moveFile(', 'RenameMigrationResult migrateRenamedBookState('])+'\n}\nnamespace {\n'
body+=method('src/activities/home/BookActions.cpp','std::string bookStatsCachePath(')+'\n}\nnamespace BookActions {\nbool clearFileMetadata(const std::string&);\n'
body+='\n'.join(method('src/activities/home/BookActions.cpp',x) for x in ['bool clearFileMetadata(', 'bool deleteFile(', 'bool clearBookCache('])+'\n}\n'
mutation=(T/'mutations.cpp').read_text().replace('// FILE_PATHS',strip('src/activities/home/FileActionPaths.h')).replace('// PRODUCTION',body)
results=[]
for name,cpp in [('settings',source),('mutations',mutation)]:
 (O/(name+'.cpp')).write_text(cpp)
 cmd=['g++','-std=c++20','-Wall','-Wextra','-Werror','-fno-exceptions','-fno-rtti','-g','-DARDUINOJSON_ENABLE_ARDUINO_STRING=0','-I/root/github/.host-data/crosssmudge/gallery-native-deps/sticky-simulator/ArduinoJson/src',str(O/(name+'.cpp')),'-o',str(O/name)]
 if a.sanitize:cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
 c=subprocess.run(cmd,capture_output=True,text=True,timeout=70);log=c.stdout+c.stderr;rc=c.returncode
 if not rc:
  c=subprocess.run([str(O/name)],capture_output=True,text=True,timeout=20);log+=c.stdout+c.stderr;rc=c.returncode
 (O/(name+'.log')).write_text(log);print(name,rc,log);results.append({'suite':name,'returncode':rc,'command':cmd})
inputs=[R/'src/SettingsList.h',R/'src/activities/settings/SettingsActivity.h',R/'src/network/CrossPointWebServer.cpp',R/'src/util/BookMoveUtils.h',R/move,R/'src/activities/home/BookActions.h',R/'src/activities/home/BookActions.cpp',R/'src/activities/home/FileActionPaths.h',*sorted(T.glob('*'))]
(O/'receipt.json').write_text(json.dumps({'results':results,'coverage':'extracted production method bodies + actual SettingInfo; ArduinoJson actual headers; SD/store/network/translation doubles, not target or physical FAT','inputs':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs if p.is_file()}},indent=2));raise SystemExit(any(x['returncode'] for x in results))
