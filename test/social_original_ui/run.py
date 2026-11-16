"""Actual callbacks, native mapped pixels, original glyphs and bounded VM.

The frozen scaffold path is an explicit evidence input, not a renderer substitute.
"""
from pathlib import Path
import argparse,hashlib,json,shutil,subprocess,tempfile,sys
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1];sys.path.insert(0,str(ROOT/'test/crossplay_ui_parity'))
from run import TRACE,png
from compare_battleship import read_png
p=argparse.ArgumentParser();p.add_argument('--reference',type=Path,required=True);p.add_argument('--native-source',type=Path,required=True);p.add_argument('--tooling',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--apps-root',type=Path,default=ROOT/'apps');p.add_argument('--skip-build',action='store_true');a=p.parse_args();O=a.output.resolve();O.mkdir(parents=True,exist_ok=True)
if not a.skip_build:
 for builder in ['build_native.py','build_forehead.py']:subprocess.run(['python3',str(HERE/builder),'--reference',str(a.reference),'--native-source',str(a.native_source),'--output',str(O/'native')],check=True)
results=[]
for app in ['insider','forehead']:
 for width,height in [(480,800),(800,480)]:
  with tempfile.TemporaryDirectory(prefix=app+'-',dir=O)as t:
   work=Path(t)/'app';shutil.copytree(a.apps_root/('cp-'+app),work);inputs={str(f.relative_to(work)):hashlib.sha256(f.read_bytes()).hexdigest()for f in work.rglob('*')if f.is_file()};(work/'fixture.lua').write_text(TRACE+(HERE/(app+'.lua')).read_text());r=subprocess.run([str(a.tooling/'capture-runner'),'fixture.lua','2097152',str(width),str(height)],cwd=work,capture_output=True,text=True,timeout=120)
   dest=O/'captures'/f'{app}-{width}';dest.mkdir(parents=True,exist_ok=True);(dest/'callbacks.log').write_text(r.stdout+r.stderr);row={'app':app,'width':width,'height':height,'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr,'inputs':inputs};results.append(row)
   if r.returncode:continue
   metrics=json.loads(r.stdout.strip().splitlines()[-1]);assert metrics['ok']and metrics['after_close']==0 and metrics['peak_bytes']<=2097152
   for state in sorted(work.glob('*.state')):
    source=state.with_suffix('.pgm');candidate=dest/(state.stem+'.png');png(source,candidate);shutil.copy(state,dest/state.name);trace=source.with_suffix('.pgm.trace');shutil.copy(trace,dest/trace.name);native=dest/(state.stem+'-native.pgm');nr=subprocess.run([str(O/'native'/('native-social'if app=='insider'else'native-forehead')),str(read_png(candidate)[0]),str(read_png(candidate)[1]),str(state),str(native)],capture_output=True,text=True);record={'test':state.stem,'entry_width':width,'returncode':nr.returncode,'stderr':nr.stderr};results.append(record)
    if nr.returncode:continue
    png(native,native.with_suffix('.png'));w,h,before=read_png(native.with_suffix('.png'));cw,ch,after=read_png(candidate);assert(w,h)==(cw,ch);record['pixel_differences']=sum(x!=y for x,y in zip(before,after));record['returncode']=int(record['pixel_differences']!=0)
for fixture in ['forehead_save.lua','forehead_epoch.lua','forehead_epoch_persistent.lua','insider_stale.lua','insider_stale_vote.lua','insider_stale_role.lua','forehead_stale_ready.lua']:
 for width,height in [(480,800),(800,480)]:
  with tempfile.TemporaryDirectory(prefix='forehead-save-',dir=O)as t:
   work=Path(t)/'app';shutil.copytree(a.apps_root/('cp-insider'if fixture.startswith('insider')else'cp-forehead'),work);(work/'fixture.lua').write_text((HERE/fixture).read_text());r=subprocess.run([str(a.tooling/'capture-runner'),'fixture.lua','2097152',str(width),str(height)],cwd=work,capture_output=True,text=True,timeout=120)
   results.append({'test':fixture,'entry_width':width,'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
report={'scope':'Insider/Forehead original mapped supported screens. Actual compiled production raster and host storage adapter; not physical acceptance.','results':results};(O/'results.json').write_text(json.dumps(report,indent=2)+'\n');print('cases',len(results),'failed',sum(bool(r['returncode'])for r in results));assert all(r['returncode']==0 for r in results)
