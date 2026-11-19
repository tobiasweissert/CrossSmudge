from pathlib import Path
import argparse,subprocess
T=Path(__file__).resolve().parent;p=argparse.ArgumentParser();p.add_argument('--output',required=True,type=Path);p.add_argument('--app-root',type=Path,default=T.parents[1]/'apps');a=p.parse_args();O=a.output.resolve();O.mkdir(parents=True,exist_ok=True)
def run(script,*args):subprocess.run(['python3',str(T/script),*map(str,args)],check=True)
run('native.py','--output',O/'native');run('build.py','--output',O/'tooling')
for fixture,app in [('baseline',None),('interaction',None),('maps','toybattle'),('base_questions','toybattle'),('toy_endings','toybattle'),('end_states','seasalt'),('records','seasalt'),('lifecycle',None)]:
 args=['--output',O/fixture,'--fixture',fixture+'.lua','--skip-build','--tooling',O/'tooling','--app-root',a.app_root];args+=['--app',app]if app else[];run('run.py',*args)
run('verify.py','--output',O,'--native',O/'native/native')
run('preservation.py','--output',O/'preservation','--runner',O/'tooling/runner');run('red_baseline.py','--output',O/'immutable-red','--runner',O/'tooling/runner')
