#!/usr/bin/env python3
"""Execute real OTA admission header with production version-comparator bodies."""
from pathlib import Path
import subprocess,sys,json,hashlib,re
R=Path(__file__).resolve().parents[3];T=Path(__file__).resolve().parent
O=Path('/root/github/.host-data/crosssmudge/crosspoint-rebase-1655/integration/network/tests/policy');O.mkdir(parents=True,exist_ok=True)
s=(R/'src/network/OtaUpdater.cpp').read_text()
parts=['constexpr size_t VERSION_SEGMENT_COUNT = 4;']
parts.append(re.search(r'struct ParsedVersion \{.*?\n\};',s,re.S).group())
for name in ['isDigit','startsWithNumberAfterOptionalV','containsRcMarker','parseVersion','compareVersions']:
 parts.append(re.search(r'(?:bool|int|ParsedVersion) '+name+r'\([^\n]*\) \{.*?(?:\n\}|\})',s,re.S).group() if name=='isDigit' else re.search(r'(?:bool|int|ParsedVersion) '+name+r'\([^\n]*\) \{.*?\n\}',s,re.S).group())
web=(R/'src/network/CrossPointWebServer.cpp').read_text()
dav=(R/'src/network/WebDAVHandler.cpp').read_text()
pattern=r'bool networkCredentialPath\(const char\* path\) \{.*?\n\}'
guard=re.search(pattern,web,re.S).group()
assert guard==re.search(pattern,dav,re.S).group(), 'web and WebDAV credential policies diverged'
(O/'credential.inc').write_text(guard)
(O/'version.inc').write_text('\n'.join(parts))
mode='sanitize' if '--sanitize' in sys.argv else 'normal'
cmd=['g++','-std=c++20','-Wall','-Wextra','-Werror','-fno-exceptions','-fno-rtti','-g','-I'+str(O),'-I'+str(R/'src/network'),str(T/'policy.cpp'),'-o',str(O/mode)]
if mode=='sanitize':cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
p=subprocess.run(cmd,capture_output=True,text=True,timeout=45);log='$ '+' '.join(cmd)+'\n'+p.stdout+p.stderr;rc=p.returncode
if not rc:
 p=subprocess.run([str(O/mode)],capture_output=True,text=True,timeout=15);log+=p.stdout+p.stderr;rc=p.returncode
(O/(mode+'.log')).write_text(log);print(log)
inputs=[T/'policy.cpp',Path(__file__),R/'src/network/CrosscubOtaPolicy.h',R/'src/network/OtaUpdater.cpp',R/'src/network/HttpRedirectPolicy.h',R/'src/network/ProtectedPaths.h',O/'version.inc',O/'credential.inc',R/'src/network/CrossPointWebServer.cpp',R/'src/network/WebDAVHandler.cpp']
(O/(mode+'.json')).write_text(json.dumps({'returncode':rc,'command':cmd,'inputs':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}},indent=2))
sys.exit(rc)
