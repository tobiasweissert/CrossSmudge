from pathlib import Path
import argparse,json,subprocess,sys
R=Path(__file__).resolve().parents[2];sys.path.insert(0,str(R/'test/crossplay_ui_parity'));from run import png
p=argparse.ArgumentParser();p.add_argument('--output',required=True,type=Path);p.add_argument('--native',required=True,type=Path);a=p.parse_args();O=a.output;N=O/'native-comparisons';N.mkdir(exist_ok=True);rows=[];F=Path('/root/github/.host-data/crosssmudge/ported-ui-parity/native-catalog-references/captures')
def compare(app,entry,name,ref,excluded=None):
 src=O/app/entry/(name+'.pgm');ha,dim,mx,pa=src.read_bytes().split(b'\n',3);hb,db,mb,pb=ref.read_bytes().split(b'\n',3);assert dim==db and mx==mb and len(pa)==len(pb);w,h=map(int,dim.split());d=[i for i in range(len(pa))if pa[i]!=pb[i]];outside=d if excluded is None else[i for i in d if not(excluded[0]<=i%w<excluded[2]and excluded[1]<=i//w<excluded[3])];row=dict(app=app,entry=entry,state=name,original=str(ref),pixels=w*h,changed=len(d),outside_declared_adaptation=len(outside),adaptation_region=excluded);rows.append(row);assert not outside,row
for entry in ['480x800','800x480']:
 for app,count,w,h in [('hearts',4,800,480),('jaipur',8,480,800)]:
  for page in range(1,count+1):
   ref=N/f'{app}-help-{page}.pgm';subprocess.run([str(a.native),app+'-help',str(w),str(h),str(ref),str(page-1)],check=True);png(ref,ref.with_suffix('.png'));compare(app,entry,'help-'+str(page),ref)
 for name in ['menu','board']:
  compare('jaipur',entry,name,F/('jaipur'if name=='menu'else'jaipur-board')/'menu-480x800.pgm')
 for name in ['board','selected-one','selected-three','after-pass','playing','trick-taken','hand-over','game-over','passing-0','passing-1','passing-2']:
  state=O/'hearts'/entry/(('state'if name=='board'else name)+'.txt')
  if not state.exists():continue
  ref=N/f'hearts-{entry}-{name}.pgm';subprocess.run([str(a.native),'hearts-board','800','480',str(ref),str(state)],check=True);png(ref,ref.with_suffix('.png'));compare('hearts',entry,name,ref)
 ref=N/'hearts-menu.pgm';subprocess.run([str(a.native),'hearts','800','480',str(ref)],check=True);png(ref,ref.with_suffix('.png'));compare('hearts',entry,'menu',ref,(280,400,768,468))
(O/'pixels.json').write_text(json.dumps(rows,indent=2)+'\n');print(len(rows),'native comparisons; all full-frame exact except declared Hearts local-strength label')
