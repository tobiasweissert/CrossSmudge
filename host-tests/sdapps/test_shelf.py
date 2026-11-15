import json,re,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
class Shelf(unittest.TestCase):
 def test_original_registry_is_preserved(self):
  text=(ROOT/'src/apps_local/Shelf.cpp').read_text()
  expected=json.loads(Path(__file__).with_name('original-shelf.json').read_text())
  for key,var in [('Games','kGames'),('Apps','kApps')]:
   block=re.search(r'constexpr shelf::Item '+var+r'\[\] = \{(.*?)\n\};',text,re.S)[1]
   self.assertEqual(re.findall(r'\{"([^"]+)"',block),expected[key])
 def test_external_launch_and_recovery_are_wired(self):
  text=(ROOT/'src/apps_local/Shelf.cpp').read_text()
  self.assertIn('ModuleActivity::create',text)
  self.assertIn('discoverPackages',text)
  self.assertIn('external ? nullptr : parent.items[item].title',text)
if __name__=='__main__':unittest.main()
