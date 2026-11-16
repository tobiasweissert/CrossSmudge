from pathlib import Path
import argparse,subprocess,shutil
T=Path(__file__).resolve().parent;p=argparse.ArgumentParser();p.add_argument('--output',required=True,type=Path);p.add_argument('--app-root',type=Path,default=T.parents[1]/'apps');a=p.parse_args();O=a.output.resolve();O.mkdir(parents=True,exist_ok=True)
def run(script,*args):subprocess.run(['python3',str(T/script),*map(str,args)],check=True)
run('native.py','--output',O/'native')
run('run.py','--output',O,'--fixture','baseline.lua','--app-root',a.app_root);shutil.copyfile(O/'results.json',O/'baseline-results.json')
run('run.py','--output',O,'--fixture','interaction.lua','--app-root',a.app_root,'--skip-build');shutil.copyfile(O/'results.json',O/'interaction-results.json')
run('run.py','--output',O,'--fixture','queued.lua','--app-root',a.app_root,'--skip-build');shutil.copyfile(O/'results.json',O/'queued-results.json')
run('verify.py','--output',O,'--native',O/'native/native')
run('preservation.py','--output',O/'preservation','--runner',O/'tooling/runner')
run('red_baseline.py','--output',O/'immutable-red','--runner',O/'tooling/runner')
