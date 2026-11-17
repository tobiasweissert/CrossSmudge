#!/usr/bin/env python3
from pathlib import Path
import runpy,subprocess,tempfile,unittest,json
R=Path(__file__).resolve().parents[2]
class Release(unittest.TestCase):
 def test_artifact_names_carry_product_identity(self):
  scope=runpy.run_path(str(R/'scripts/rename_firmware.py'))
  class Env(dict):
   def GetProjectOption(self,key):return {'custom_firmware_device_type':'sticky','custom_release_version':'1.6.5.4','custom_rc_branch':'release/1.6.5.4','custom_rc_hash':'abcdef0'}.get(key)
  with tempfile.TemporaryDirectory(prefix='crosscub-artifact-') as tmp:
   p=Path(tmp);(p/'firmware.bin').write_bytes(b'synthetic test; not firmware')
   scope['rename_firmware']([], [str(p/'firmware.bin')], Env(PROJECT_DIR=tmp,PIOENV='sticky'))
   self.assertTrue((p/'crosscub-sticky.bin').is_file())
   self.assertTrue((p/'crosscub-sticky-v1.6.5.4.bin').is_file())
 def test_catalog_rejects_unowned_variant(self):
  with tempfile.TemporaryDirectory(prefix='crosscub-release-') as tmp:
   p=Path(tmp);bad=p/'firmware-foreign-v1.bin';bad.write_bytes(b'synthetic')
   r=subprocess.run(['python3',str(R/'scripts/generate_release_catalog.py'),'--firmware',str(bad),'--output',str(p/'catalog'),'--repo','tobiasweissert/CrossSmudge','--version','1'],capture_output=True)
   self.assertNotEqual(r.returncode,0,'other-board release accepted')
if __name__=='__main__':unittest.main()
