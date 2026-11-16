from pathlib import Path
import argparse,subprocess,json,sys
T=Path(__file__).resolve().parent
p=argparse.ArgumentParser();p.add_argument('--app-root',type=Path,default=T.parents[1]/'apps');p.add_argument('--output',type=Path,required=True);a=p.parse_args();O=a.output.resolve();O.mkdir(parents=True,exist_ok=True)
def run(*args):subprocess.run([sys.executable,*map(str,args)],check=True)
run(T/'build.py','--output',O/'tooling');run(T/'native.py','--output',O/'native')
for name in ['states','distances','single','records','privacy','lifecycle','abandon_limit','mode_duplicate','queued','queued_buttons','caption']:
 run(T/'run.py','--output',O/name,'--tooling',O/'tooling','--skip-build','--app-root',a.app_root,'--fixture',name+'.lua')
run(T/'run.py','--output',O/'rules','--tooling',O/'tooling','--skip-build','--app-root',a.app_root,'--fixture','../crossplay_ports/wavelength.lua')
run(T/'verify.py','--captures',O,'--native',O/'native/native','--output',O/'native-pixels')
run(T/'preservation.py','--output',O/'preservation','--runner',O/'tooling/runner','--app-root',a.app_root)
run(T/'red.py','--output',O/'immutable-red','--runner',O/'tooling/runner','--native',O/'native/native')
