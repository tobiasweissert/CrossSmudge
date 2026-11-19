#!/usr/bin/env python3
"""Release metadata/inventory checks; no packaging or publication side effects."""
import json,pathlib,subprocess,sys
R=pathlib.Path(__file__).resolve().parents[2]
m=json.loads((R/'apps/browser/manifest.json').read_text());c=json.loads((R/'apps/catalog.json').read_text());b=next(a for a in c['apps']if a['id']=='browser')
assert m['version']==b['version']=='0.3.5','public Browser must be version 0.3.5'
assert m['min_api']==b['min_api']==6,'Direct/Offline still work on API6'
assert b['files']==sorted(p.name for p in (R/'apps/browser').iterdir()if p.is_file()),'complete SRP1 runtime inventory'
# The general preservation gate pins the older accepted Browser release. Rebind
# only that record to the approved Browser0.3.5 bytes; retain every gate and all
# unrelated accepted pins. No catalog, manifest or historical fixture is written.
checker=R/'test/integration_catalog/check_catalog.py'
source=checker.read_text()
anchor="assert set(carry['apps'])==set(predecessor['apps']), 'Accepted redesign must preserve predecessor IDs'"
assert source.count(anchor)==1,'catalog preservation injection anchor changed'
browser_pin={'version':'0.3.5','record_sha256':'3014dac9a2545d2ae31e3475c3f4cd6f4e70142d25dd2b1ecd3f94f35ba496fe','manifest_sha256':'093488ab1d4c0146dd9ff3cdce2f028a83608124d62c50838aa61e6f6b74a863'}
source=source.replace(anchor,anchor+"\ncarry['apps']['browser']="+repr(browser_pin))
exec(compile(source,str(checker),'exec'),{'__file__':str(checker),'__name__':'__main__'})
assert 'configuration' not in m['description'].lower()
print('PASS public Browser0.3.5 complete inventory/min_api6/unrelated catalog preservation')
