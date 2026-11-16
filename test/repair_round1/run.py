"""Production callbacks/rendering, vendored int32/float32 VM, native 2 MiB budget."""
from pathlib import Path
import os, subprocess, tempfile
R=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as d:
 runner=Path(d)/'lua'
 subprocess.run(['cc','-O1','-I'+str(R/'lib/Lua/src'),str(R/'test/network_apps/runner.c'),*map(str,sorted((R/'lib/Lua/src').glob('l*.c'))),'-lm','-ldl','-o',str(runner)],check=True)
 for case in ['hn_memory','hn_cursor','notes_loss','notes_resume','forehead']:
  print('CASE',case,flush=True)
  subprocess.run([str(runner),str(Path(__file__).parent/(case+'.lua'))],cwd=R,env=dict(os.environ,LUA_APP_LIMIT='2097152'),check=True)
