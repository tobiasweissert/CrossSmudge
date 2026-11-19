from pathlib import Path
import argparse,json,subprocess,sys
R=Path(__file__).resolve().parents[2];sys.path.insert(0,str(R/'test/crossplay_ui_parity'));from run import png
p=argparse.ArgumentParser();p.add_argument('--captures',type=Path,required=True);p.add_argument('--native',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True);rows=[]
for model in sorted(a.captures.rglob('*.wave')):
 capture=model.with_suffix('.pgm');view=model.with_suffix('.txt').read_text();relative=model.relative_to(a.captures);native=a.output/relative.with_suffix('.pgm');native.parent.mkdir(parents=True,exist_ok=True);subprocess.run([str(a.native),view,'480','800',str(native),str(model)],check=True);png(native,native.with_suffix('.png'))
 one=capture.read_bytes().split(b'\n',3)[3];two=native.read_bytes().split(b'\n',3)[3];assert len(one)==len(two)==384000
 changed=[i for i,(x,y)in enumerate(zip(one,two))if x!=y]
 # The only scored-call deviation is the exact union of old/new footnote ink.
 # Compare the entire corrected frame; no rectangular exclusion for this case.
 fields=list(map(int,model.read_text().splitlines()[0].split()));scored_call=view=='call'and fields[6]==0
 if scored_call:
  entry=model.parent.name;folder=a.captures/'caption/wavelength'/entry
  old=(folder/'caption-old.pgm').read_bytes().split(b'\n',3)[3];new=(folder/'caption-new.pgm').read_bytes().split(b'\n',3)[3]
  assert len(old)==len(new)==len(two)
  expected=bytearray(two)
  for i,pixel in enumerate(old):
   if pixel==0:assert two[i]==0;expected[i]=255
  for i,pixel in enumerate(new):
   if pixel==0:expected[i]=0
  assert one==bytes(expected),'Scored call differs outside exact truthful-caption glyph replacement'
  two=bytes(expected)
 outside=[i for i in ([]if scored_call else changed)if not(16<=i%480<464 and(view=='menu'and(508<=i//480<526 or 744<=i//480<784)or view=='summary'and 705<=i//480<734))];row=dict(capture=str(relative.with_suffix('.pgm')),view=view,changed=len(changed),scored_caption_exact_replacement=scored_call,outside_adaptation=len(outside),bounds=([min(i%480 for i in outside),min(i//480 for i in outside),max(i%480 for i in outside),max(i//480 for i in outside)]if outside else None));rows.append(row)
 if outside:print(row)
(a.output/'pixels.json').write_text(json.dumps(rows,indent=2)+'\n');print('frames',len(rows),'full0diff',sum(r['changed']==0 for r in rows),'outside',sum(r['outside_adaptation']for r in rows));assert all(r['outside_adaptation']==0 for r in rows)
