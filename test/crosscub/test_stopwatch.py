#!/usr/bin/env python3
from pathlib import Path
import subprocess,tempfile,shutil,unittest
R=Path(__file__).resolve().parents[2]
class StopwatchFit(unittest.TestCase):
 def test_modified_app_has_matching_patch_metadata(self):
  import json
  manifest=json.loads((R/'apps/stopwatch/manifest.json').read_text())
  record=next(a for a in json.loads((R/'apps/catalog.json').read_text())['apps'] if a['id']=='stopwatch')
  self.assertEqual(manifest['id'],'stopwatch')
  self.assertEqual(manifest['version'],'1.0.1')
  self.assertEqual(record['version'],manifest['version'])
 def test_both_orientations_touch_save_and_bounds(self):
  with tempfile.TemporaryDirectory(prefix='crosscub-stopwatch-')as tmp:
   p=Path(tmp);runner=p/'runner'
   subprocess.run(['cc','-O2','-I'+str(R/'lib/Lua/src'),str(R/'test/crossplay_ports/runner.c'),*map(str,sorted((R/'lib/Lua/src').glob('*.c'))),'-lm','-o',str(runner)],check=True)
   for w,h in [(480,800),(800,480)]:
    case=p/str(w);shutil.copytree(R/'apps/stopwatch',case)
    t=subprocess.run([str(runner),str(R/'test/crosscub/stopwatch_fit.lua'),'2097152',str(w),str(h)],cwd=case,capture_output=True,text=True)
    with self.subTest(width=w,height=h):
     self.assertEqual(t.returncode,0,f'{w}x{h}\n'+t.stdout+t.stderr)
if __name__=='__main__':unittest.main()
