"""Compare production callback captures to the actual immutable native screens."""
from pathlib import Path
import argparse,subprocess,json,sys
T=Path(__file__).resolve().parent;R=T.parents[1];sys.path.insert(0,str(R/'test/crossplay_ui_parity'));from run import png
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--native',type=Path,required=True);a=p.parse_args();O=a.output.resolve();N=O/'native-pixels';N.mkdir(parents=True,exist_ok=True);rows=[]
def compare(src,kind,arg='',excluded=None):
 rel=src.relative_to(O);ref=N/rel;ref.parent.mkdir(parents=True,exist_ok=True);subprocess.run([str(a.native),kind,'480','800',str(ref),str(arg)],check=True)
 _,size,maxval,pa=src.read_bytes().split(b'\n',3);_,rs,rm,pb=ref.read_bytes().split(b'\n',3);assert size==rs and maxval==rm and len(pa)==len(pb)
 width,height=map(int,size.split());diff=[i for i in range(len(pa))if pa[i]!=pb[i]];outside=diff if excluded is None else[i for i in diff if not(excluded[0]<=i%width<excluded[2]and excluded[1]<=i//width<excluded[3])]
 row=dict(capture=str(rel),native=kind,model=str(arg),pixels=len(pa),changed=len(diff),outside_declared_adaptation=len(outside),adaptation_region=excluded);rows.append(row);assert not outside,row;png(ref,ref.with_suffix('.png'))
for entry in ['480x800','800x480']:
 for app,count in [('seasalt',6),('toybattle',8)]:
  folder=O/'baseline'/app/entry;compare(folder/'menu.pgm',app)
  for page in range(1,count+1):compare(folder/f'help-{page}.pgm',app+'-help',page-1)
 compare(O/'baseline/toybattle'/entry/'setup.pgm','toybattle-setup','',(315,204,415,229))
 for page in [1,2]:compare(O/'baseline/toybattle'/entry/f'maps-{page}.pgm','toybattle-maps',page-1)
 for name in ['baseline','interaction','maps','base_questions','toy_endings']:
  for app in ['seasalt','toybattle']:
   folder=O/name/app/entry
   if not folder.exists():continue
   for model in sorted(folder.glob('*.model')):
    result=model.stem.endswith('result');compare(model.with_suffix('.pgm'),'seasalt-board'if app=='seasalt'else'toybattle-result'if result else'toybattle-model',model)
 for name in ['interaction','end_states']:
  for model in sorted((O/name/'seasalt'/entry).glob('*.round')):compare(model.with_suffix('.pgm'),'seasalt-round',model)
 for model in sorted((O/'records/seasalt'/entry).glob('*.menu')):compare(model.with_suffix('.pgm'),'seasalt',model)
 for folder in ['baseline','interaction','maps']:
  source=O/folder/'toybattle'/entry
  for src in sorted(source.glob('*brief.pgm')):
   if src.stem=='tb-brief':arg='0 1'
   else:arg=src.stem.split('-')[1]+' 1'
   compare(src,'toybattle-brief',arg)
 for src in sorted((O/'interaction/seasalt'/entry).glob('ss-call-*.pgm')):
  # Actual point count at the recorded call callback, independently decoded by the old core fixture.
  point=(src.with_suffix('.points')).read_text().strip();compare(src,'seasalt-call',point)
(O/'pixels.json').write_text(json.dumps(rows,indent=2)+'\n');print(len(rows),'native comparisons;',sum(x['changed']==0 for x in rows),'whole frames exact; two declared fixed-AI labels')
