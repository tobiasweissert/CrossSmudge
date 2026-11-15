#!/usr/bin/env python3
"""Standalone per-app check. Uses only this directory and the bundled Lua sources."""
import argparse, os, pathlib, subprocess, tempfile, hashlib
HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[2]
APP = HERE.name
p = argparse.ArgumentParser()
p.add_argument('--source', type=pathlib.Path, default=ROOT/'apps'/APP/'main.lua')
p.add_argument('--expect-red', action='store_true')
p.add_argument('--runner', type=pathlib.Path)
p.add_argument('--sanitize', action='store_true', help='Build bundled Lua and runner with ASan/UBSan')
p.add_argument('--geometry-baseline-ref', default='9779d579e94a7c5c13ac5387f22babe1a473baa7')
p.add_argument('--baseline-ref', help='Verify Sticky red and identical non-Sticky traces against this git ref')
a = p.parse_args()
scratch = pathlib.Path(os.environ.get('TMPDIR', '/root/.hermes/cache/scratch'))
with tempfile.TemporaryDirectory(prefix=APP+'-lua-', dir=scratch) as tmp:
    runner = a.runner or pathlib.Path(tmp)/'runner'
    if a.runner and a.sanitize:
        p.error('--sanitize requires compiling its own runner')
    if not a.runner:
        flags = ['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie'] if a.sanitize else []
        subprocess.run(['cc','-std=c11','-O1',*flags,'-I'+str(ROOT/'lib/Lua/src'),str(HERE/'runner.c'),
                        *map(str,sorted((ROOT/'lib/Lua/src').glob('*.c'))),'-lm','-o',str(runner)],check=True)
    baseline = pathlib.Path(tmp)/'baseline.lua'
    if a.baseline_ref:
        baseline.write_bytes(subprocess.check_output(['git','show',a.baseline_ref+':apps/'+APP+'/main.lua'],cwd=ROOT))
    for device in ['sticky','x4pro','x4','x3','simulator','missing']:
        for mode in (['active'] if device=='sticky' else ['active','ignored']):
            r=subprocess.run([str(runner),str(HERE/'behavior.lua'),str(a.source),device,mode],capture_output=True,text=True)
            red = a.expect_red and device=='sticky'
            assert (r.returncode != 0) if red else (r.returncode == 0), r.stderr
            if red:
                assert 'expected' in r.stderr, r.stderr
                print('EXPECTED RED',device,mode,r.stderr.strip())
            else:
                print('PASS',device,mode,hashlib.sha256(r.stdout.encode()).hexdigest(),r.stderr.strip())
            if a.baseline_ref:
                before=subprocess.run([str(runner),str(HERE/'behavior.lua'),str(baseline),device,mode],capture_output=True,text=True)
                if device=='sticky':
                    assert before.returncode != 0 and 'expected' in before.stderr, before.stderr
                    print('BASELINE RED',before.stderr.strip())
                else:
                    assert before.returncode == 0, before.stderr
                    assert before.stdout == r.stdout, 'non-Sticky behavior changed: '+device+' '+mode
                    print('BASELINE IDENTICAL',device,mode)
    # Geometry fixture is part of this routine entrypoint, including sanitizer mode.
    if not a.expect_red:
        baseline.write_bytes(subprocess.check_output(['git','show',a.geometry_baseline_ref+':apps/hourglass/main.lua'],cwd=ROOT))
        for device in ['sticky','x4pro','x4','x3','simulator','missing']:
            mode = 'active' if device == 'sticky' else 'trace'
            args = [str(runner),str(HERE/'geometry.lua')]
            current = subprocess.run([*args,str(a.source),device,mode],capture_output=True,text=True)
            before = subprocess.run([*args,str(baseline),device,mode],capture_output=True,text=True)
            assert current.returncode == 0, current.stdout + current.stderr
            print('GEOMETRY CURRENT',device,current.stderr.strip())
            print('\n'.join(line for line in current.stdout.splitlines() if line.startswith(('PASS ','TOTAL '))))
            if device == 'sticky':
                expected = {f'FAIL sticky {bounds} {case}' for bounds in ['480x800','800x480'] for case in [
                    'upper-portion-repro','row1-upper-left','row1-upper-right','row1-miss-below',
                    'row1-miss-left','row1-miss-right','row2-upper-left','row2-upper-right',
                    'row2-miss-below','row2-miss-left','row2-miss-right','gap-outside-nontap']}
                observed = {line.split(': ',1)[0] for line in before.stdout.splitlines() if line.startswith('FAIL ')}
                assert before.returncode == 1 and observed == expected, before.stdout + before.stderr
                print('GEOMETRY BASELINE EXPECTED RED',a.geometry_baseline_ref)
                print(before.stdout, before.stderr)
            else:
                assert before.returncode == 0, before.stdout + before.stderr
                assert before.stdout == current.stdout, 'geometry legacy trace changed: '+device
                print('GEOMETRY BASELINE IDENTICAL',device,hashlib.sha256(current.stdout.encode()).hexdigest())
