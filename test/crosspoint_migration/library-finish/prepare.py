#!/usr/bin/env python3
"""Prepare small pinned host-test inputs. Never initializes/downloads an SDK.
GTest is fetched only if missing; SDK UI comes from existing immutable Git objects.
"""
import hashlib,io,json,os,subprocess,tarfile,urllib.request
from pathlib import Path
scratch=Path(os.environ.get('TMPDIR','/root/.hermes/cache/scratch'));g=scratch/'library-finish-deps';g.mkdir(exist_ok=True)
if not(g/'googletest-1.17.0/googletest/include/gtest/gtest.h').exists():
 data=urllib.request.urlopen('https://codeload.github.com/google/googletest/tar.gz/refs/tags/v1.17.0',timeout=30).read()
 assert hashlib.sha256(data).hexdigest()=='65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c'
 tarfile.open(fileobj=io.BytesIO(data),mode='r:gz').extractall(g,filter='data')
sdk=Path(os.environ.get('LIBRARY_SDK_GIT_DIR','/root/github/crosssmudge/.git/modules/freeink-sdk'));ref='634a3f8c067a3714a6a7c622015138af096ab5d2'
out=scratch/'library-finish-sdk';ui=scratch/'library-finish-ui';ui.mkdir(exist_ok=True)
def git(*args):return subprocess.check_output(['git','--git-dir='+str(sdk),*args],timeout=30)
paths=git('ls-tree','-r','--name-only',ref,'--','libs/ui/FreeInkUI').decode().splitlines();manifest={}
for f in paths:
 if not f.endswith(('.h','.hpp')):continue
 p=out/f;p.parent.mkdir(parents=True,exist_ok=True);data=git('show',ref+':'+f);p.write_bytes(data);manifest[f]=hashlib.sha256(data).hexdigest()
for f,p in [('libs/ui/FreeInkUI/src/FreeInkUI.cpp',out/'FreeInkUI.cpp'),('libs/assets/Icons/include/Icon.h',ui/'Icon.h')]:
 data=git('show',ref+':'+f);p.write_bytes(data);manifest[f]=hashlib.sha256(data).hexdigest()
(out/'inputs.json').write_text(json.dumps({'sdk':ref,'sha256':manifest},indent=2));print('Pinned GTest + existing-object SDK UI inputs ready')
