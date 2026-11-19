#!/usr/bin/env python3
from pathlib import Path
import argparse,hashlib,json,re,subprocess
R=Path(__file__).resolve().parents[3];T=Path(__file__).resolve().parent
p=argparse.ArgumentParser();p.add_argument('--evidence',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();O=a.evidence;O.mkdir(parents=True,exist_ok=True)
web=(R/'src/network/CrossPointWebServer.cpp').read_text();dav=(R/'src/network/WebDAVHandler.cpp').read_text()
pattern=r'bool networkCredentialPath\(const char\* path\) \{.*?\n\}'
guard=re.search(pattern,web,re.S).group();assert guard==re.search(pattern,dav,re.S).group();(O/'credential.inc').write_text(guard)
commands=[];logs=[];results=[]
def run(name,cmd,execute=False):
 commands.append(cmd);r=subprocess.run(cmd,capture_output=True,text=True,timeout=60);log=r.stdout+r.stderr;rc=r.returncode
 if not rc and execute:
  r=subprocess.run([str(O/name)],capture_output=True,text=True,timeout=20);log+=r.stdout+r.stderr;rc=r.returncode
 (O/(name+'.log')).write_text(log);print(name,rc,log);results.append({'suite':name,'returncode':rc})
base=['g++','-std=c++20','-Wall','-Wextra','-Werror','-fno-exceptions','-fno-rtti']
if a.sanitize:base+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
run('security',base+['-I'+str(R/'src'),'-I'+str(O),str(T/'security.cpp'),'-o',str(O/'security')],True)
# Signature-only source extractor: checks real .cpp declarations against real
# header, not network function execution or an entire target compilation.
D=O/'header-stubs';D.mkdir(exist_ok=True)
(D/'ArduinoJson.h').write_text('#pragma once\nstruct JsonDocument {};\n')
(D/'HalStorage.h').write_text('#pragma once\nstruct HalFile {};\n')
(D/'WebServer.h').write_text('#pragma once\n#include <string>\nusing String=std::string;\nclass WebServer {};\n')
(D/'WebSocketsServer.h').write_text('#pragma once\nclass WebSocketsServer {};\nenum WStype_t {WStype_TEXT};\n')
sigs=re.findall(r'^([^\n;{}]*CrossPointWebServer::~?\w+\([^{}]*?)\s*\{',web,re.M)
assert len(sigs)>60,len(sigs)
source='#include "network/CrossPointWebServer.h"\n'+ '\n'.join(sig+' {__builtin_unreachable();}' for sig in sigs)+'\n'
(O/'header-signatures.cpp').write_text(source)
run('web-header',base+['-Wno-unused-parameter']+['-I'+str(D),'-I'+str(R/'src'),'-fsyntax-only',str(O/'header-signatures.cpp')])
# Actual FsHelpers header + production safe-component implementation: duplicate
# String wrapper definitions would fail this translation unit.
(D/'WString.h').write_text('#pragma once\n#include <string>\nclass String {std::string s; public: explicit String(const char* p):s(p){} const char* c_str()const{return s.c_str();} size_t length()const{return s.size();}};\n')
fs=(R/'lib/FsHelpers/FsHelpers.cpp').read_text()
body=re.search(r'bool isSafePathComponent\(std::string_view name\) \{.*?\n\}',fs,re.S).group()
probe='#include <FsHelpers.h>\n#include <cassert>\nnamespace FsHelpers {\n'+body+'\n}\nint main(){for(auto p:{"", ".", "..", "a/b", "a\\\\b"})assert(!FsHelpers::isSafePathComponent(String(p))); for(auto p:{"book.epub", "volume..2.epub", "notes...txt"})assert(FsHelpers::isSafePathComponent(String(p)));}\n'
(O/'fs.cpp').write_text(probe)
run('fs-header',base+['-I'+str(D),'-I'+str(R/'lib/FsHelpers'),str(O/'fs.cpp'),'-o',str(O/'fs-header')],True)
print('checked',len(sigs),'frozen implementation signatures')
inputs=[R/'lib/FsHelpers/FsHelpers.h',R/'lib/FsHelpers/FsHelpers.cpp',R/'src/network/CrossPointWebServer.h',R/'src/network/ProtectedPaths.h',R/'src/network/CrossPointWebServer.cpp',R/'src/network/WebDAVHandler.cpp',T/'security.cpp',Path(__file__)]
(O/'interfaces.json').write_text(json.dumps({'results':results,'commands':commands,'signature_count':len(sigs),'inputs':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}},indent=2))
raise SystemExit(any(r['returncode'] for r in results))
