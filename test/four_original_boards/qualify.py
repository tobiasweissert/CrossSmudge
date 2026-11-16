"""Bounded production Sticky callback and native pixel qualification for four titles."""
import argparse,json,subprocess,sys
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--tooling',type=Path,required=True);p.add_argument('--native',type=Path,required=True);p.add_argument('--extra-native',type=Path,required=True);p.add_argument('--app-root',type=Path);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
entries=[('callbacks.lua',['go','hex','minesweeper','yahtzee']),('settings.lua',['go','hex']),('records_bounds.lua',['go','hex','minesweeper','yahtzee']),('failed_records.lua',['minesweeper','yahtzee'])]
entries += [(name+'.lua',[title])for title,names in {'go':['go_gameplay','go_preferences','go_sizes','go_repeat','go_doors'],'hex':['hex_gameplay','hex_preferences','hex_repeat','hex_completion_recovery','hex_older_completion'],'minesweeper':['mines_gameplay','mines_lessons','mines_controls','mines_chord'],'yahtzee':['yahtzee_gameplay','yahtzee_lessons','yahtzee_results','yahtzee_joker','yahtzee_complete']}.items()for name in names]
results=[]
for fixture,titles in entries:
 out=a.output/Path(fixture).stem;cmd=[sys.executable,str(Path(__file__).with_name('run.py')),'--output',str(out),'--tooling',str(a.tooling),'--native',str(a.native),'--extra-native',str(a.extra_native),'--fixture',fixture,'--titles',*titles]
 if a.app_root:cmd += ['--app-root',str(a.app_root)]
 r=subprocess.run(cmd,text=True,capture_output=True);print(fixture,r.returncode,flush=True);(a.output/(Path(fixture).stem+'.log')).write_text(r.stdout+r.stderr);results.append(dict(fixture=fixture,titles=titles,code=r.returncode,results=str(out/'results.json')))
(a.output/'results.json').write_text(json.dumps(results,indent=2)+'\n');assert all(x['code']==0 for x in results)
