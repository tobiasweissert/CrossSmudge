#!/usr/bin/env python3
"""Release metadata/inventory checks; no packaging or publication side effects."""
import json,pathlib,subprocess,sys
R=pathlib.Path(__file__).resolve().parents[2]
m=json.loads((R/'apps/browser/manifest.json').read_text());c=json.loads((R/'apps/catalog.json').read_text());b=next(a for a in c['apps']if a['id']=='browser')
assert m['version']==b['version']=='0.3.1','public Browser must be version 0.3.1'
assert m['min_api']==b['min_api']==6,'Direct/Offline still work on API6'
assert b['files']==sorted(p.name for p in (R/'apps/browser').iterdir()if p.is_file()),'complete SRP1 runtime inventory'
subprocess.run([sys.executable,str(R/'test/integration_catalog/check_catalog.py')],check=True)
assert 'configuration' not in m['description'].lower()
print('PASS public Browser0.3.1 complete inventory/min_api6/unrelated catalog preservation')
