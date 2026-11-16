#!/usr/bin/env python3
"""Re-render production UI and require complete generic handoff error guidance."""
from pathlib import Path
import argparse,json,re,subprocess,xml.etree.ElementTree as ET
r=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--existing',action='store_true');a=p.parse_args()
if not a.existing:subprocess.run(['python3',str(r/'test/airdrop/ui_render.py'),'--output',str(a.output)],check=True)
line=next(x for x in (r/'lib/I18n/translations/english.yaml').read_text().splitlines()if x.startswith('STR_AIRDROP_BROWSER_FAILED:'))
expected=' '.join(json.loads(line.split(':',1)[1].strip()).split())
checked=[]
for name in ['browser-retry-portrait-normal.svg','browser-retry-portrait-large.svg','browser-retry-landscape-normal.svg','browser-retry-landscape-large.svg']:
 root=ET.parse(a.output/name).getroot();shown=' '.join(' '.join((n.text or '')for n in root if n.tag.endswith('text')).split())
 assert expected in shown,'Incomplete handoff guidance: '+name+' '+shown
 checked.append(name)
print(json.dumps({'exit':0,'full_error_guidance_profiles':checked,'scope':'actual production content/pinned SDK host SVG with declared approximate font metrics, not panel/font/e-ink proof'}))
