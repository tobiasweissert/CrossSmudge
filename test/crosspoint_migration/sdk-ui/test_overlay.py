#!/usr/bin/env python3
"""Offline, revision-bound SDK overlay and build-policy checks."""
import hashlib
import os
from pathlib import Path
import runpy
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[3]
SDK=Path(os.environ.get('CROSSCUB_SDK_GIT_DIR','/root/github/crosssmudge/.git/modules/freeink-sdk'))
REV='634a3f8c067a3714a6a7c622015138af096ab5d2'
class Env:
    def __init__(self, project): self.project=project
    def subst(self,v):return self.project
def load():
    # Never let a verification import patch the real SDK or dependency cache.
    with tempfile.TemporaryDirectory(dir=os.environ['TMPDIR'],prefix='sdk-ui-import-') as project:
        return runpy.run_path(str(ROOT/'scripts/patch_wolfssl.py'),init_globals={'Import':lambda name:None,'env':Env(project)})
def main():
    m=load()
    assert 'sdk_secure_net_outputs' in m, 'Missing revision-bound SecureNet migration overlay'
    with tempfile.TemporaryDirectory(dir=os.environ['TMPDIR'],prefix='sdk-ui-overlay-') as td:
        d=Path(td)
        for p in m['SDK_SECURENET_PREIMAGES']:
            f=d/p;f.parent.mkdir(parents=True,exist_ok=True)
            f.write_bytes(subprocess.check_output(['git','--git-dir='+str(SDK),'show',REV+':'+p]))
        outputs=m['sdk_secure_net_outputs'](d)
        assert outputs
        for p,b in outputs.items():(d/p).write_bytes(b)
        assert m['sdk_secure_net_outputs'](d)==outputs, 'Installed output must be accepted unchanged'
        damaged=next(iter(outputs));(d/damaged).write_bytes(outputs[damaged]+b'// unknown peer edit\n')
        before={p:(d/p).read_bytes() for p in outputs}
        try:m['sdk_secure_net_outputs'](d)
        except RuntimeError:pass
        else:raise AssertionError('Unknown SDK source must fail closed')
        assert before=={p:(d/p).read_bytes() for p in outputs},'Planning may not mutate SDK'
        for p,b in outputs.items():
            if p.endswith('SecureClient.cpp'):
                text=b.decode();assert 'WOLFSSL_VERIFY_PEER' in text and 'wolfSSL_CTX_SetMinVersion' in text
                assert 'NO_ASN_TIME' in text and 'wolfSSL_check_domain_name(ssl, host) != WOLFSSL_SUCCESS' in text
            print(p,hashlib.sha256(b).hexdigest())
    print('PASS: exact SDK preimages, deterministic/idempotent outputs, unknown-source refusal without partial writes')
if __name__=='__main__':main()
