"""Exact shipped Lua ABI, budgeted callbacks and real board hit geometry."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
HERE=Path(__file__).resolve().parent
parser=argparse.ArgumentParser()
parser.add_argument('--output',type=Path,default=Path('/tmp/board-visual-ports.json'))
parser.add_argument('--captures',type=Path,default=Path('/tmp/board-visual-captures'))
args=parser.parse_args()
apps=['battleship','checkers','connectfour','hex','knucklebones','yahtzee','go','toybattle']
records=[]
with tempfile.TemporaryDirectory(prefix='board-visual-ports-') as scratch:
    work=Path(scratch)
    runner=work/'lua'
    subprocess.run(['cc','-O2','-I'+str(ROOT/'lib/Lua/src'),str(ROOT/'test/crossplay_ports/runner.c'),*map(str,sorted((ROOT/'lib/Lua/src').glob('*.c'))),'-lm','-o',str(runner)],check=True)
    args.captures.mkdir(parents=True,exist_ok=True)
    prefix=(ROOT/'test/replacement_wordle/render.lua').read_text().split('smudge.get_date=')[0]
    prefix+="smudge.line=function(x1,y1,x2,y2,color,weight)add(string.format('<path d=\"M %d %d L %d %d\" fill=\"none\" stroke=\"%s\" stroke-width=\"%d\"/>',x1,y1,x2,y2,color==false and 'white' or 'black',weight or 1))end\n"
    for app in apps:
        for budget in [76800,524288,2097152]:
            for width,height in [(480,800),(800,480)]:
                fixture=work/(app+str(budget)+str(width))
                shutil.copytree(ROOT/'apps'/('cp-'+app),fixture)
                result=subprocess.run([str(runner),str(HERE/('low_memory.lua' if budget<100000 else 'graph_behavior.lua' if app in ['go','toybattle'] else 'behavior.lua')),str(budget),str(width),str(height)],cwd=fixture,env=dict(os.environ,BOARD_APP=app),text=True,capture_output=True,timeout=120)
                record=dict(app=app,budget=budget,width=width,height=height,returncode=result.returncode,stdout=result.stdout,stderr=result.stderr)
                records.append(record)
                print(app,budget,width,'PASS' if not result.returncode else result.stderr,flush=True)
                if budget==2097152 and result.returncode==0 and app not in ['go','toybattle']:
                    scenarios={
                        'fresh':"dofile('main.lua');on_init();on_draw();on_button('confirm',true);on_draw();add('</svg>');on_exit()\n",
                        'position':"local M=dofile('core.lua');local B=dofile('board.lua');local F=dofile('graphics.lua');F.init();local g=M.new(123456);"+{
                            'connectfour':"for _,c in ipairs({4,3,4,3,4,2,4})do assert(M.action(g,c))end;",
                            'checkers':"for n=1,2 do assert(M.play(g,M.choose(g)))end;g.selected=M.moves(g)[1]:byte(1);",
                            'knucklebones':"for n=1,9 do assert(M.action(g,M.choose(g)))end;",
                            'battleship':"for row=1,5 do assert(M.action(g,1,row))end;for shot=1,4 do assert(M.fire(g,shot));assert(M.fire(g,M.choose(g)))end;",
                            'hex':"for _,p in ipairs({{6,6},{5,6},{6,5},{5,5},{7,5},{4,6},{7,4},{4,5}})do assert(M.action(g,p[1],p[2]));B.record((p[2]-1)*11+p[1])end;",
                            'yahtzee':"for n=1,6 do assert(M.roll(g));assert(M.take(g,M.choose_box(g.cards[g.turn],g.d)))end;assert(M.roll(g));assert(M.hold(g,1));",
                        }[app]+"smudge.clear();F.header(M.title or 'YAHTZEE');B.draw(M,g,1,6,nil,F,width,height);add('</svg>')\n"
                    }
                    scenarios['home-saved']=scenarios['position'].split('smudge.clear();')[0]+"assert(dofile('storage.lua').save('1|'..M.encode(g)));dofile('main.lua');on_init();on_draw();add('</svg>')"
                    scenarios['home']="dofile('main.lua');on_init();on_draw();add('</svg>')"
                    scenarios['settings']="dofile('main.lua');on_init();on_draw();on_button('down',true);on_button('confirm',true);on_draw();add('</svg>')"
                    scenarios['result']="local M=dofile('core.lua');local B=dofile('board.lua');local F=dofile('graphics.lua');F.init();local g=M.new(123456);"+{
                        'connectfour':"for _,c in ipairs({4,3,4,3,4,2,4})do assert(M.action(g,c))end;",
                        'checkers':"g.status=3;",
                        'knucklebones':"g.status=1;",
                        'battleship':"for row=1,5 do assert(M.action(g,1,row))end;g.status=1;",
                        'hex':"for r=0,10 do g.b[r*11+6]=1 end;g.status=1;",
                        'yahtzee':"for n=1,26 do assert(M.roll(g));assert(M.take(g,M.choose_box(g.cards[g.turn],g.d)))end;",
                    }[app]+"smudge.clear();F.header(M.title or 'YAHTZEE');B.draw(M,g,1,6,nil,F,width,height);add('</svg>')"
                    if app in ['checkers','connectfour','knucklebones','yahtzee']:
                        for page in [1,3]:scenarios['help-'+str(page)]="local B=dofile('board.lua');local F=dofile('graphics.lua');F.init();smudge.clear();B.lesson("+str(page)+",F,width,height);add('</svg>')"
                    for scenario,body in scenarios.items():
                        (fixture/'capture.lua').write_text(prefix+body)
                        for state in fixture.glob('state-*.dat'):state.unlink()
                        rendered=subprocess.run([str(runner),'capture.lua',str(budget),str(width),str(height)],cwd=fixture,text=True,capture_output=True,timeout=120)
                        assert rendered.returncode==0,rendered.stderr
                        target=args.captures/(app+'-'+scenario+'-'+str(width)+'x'+str(height)+'.svg')
                        shutil.copyfile(fixture/'render.svg',target)
                        if shutil.which('rsvg-convert'):subprocess.run(['rsvg-convert',str(target),'-o',str(target.with_suffix('.png'))],check=True)

                if budget==2097152 and result.returncode==0 and app in ['go','toybattle']:
                    scenarios={'fresh':"dofile('main.lua');on_init();on_draw();add('</svg>')"}
                    if app=='go':
                        for n in [9,13,19]:
                            scenarios['position-'+str(n)]="local M=dofile('core.lua');local B=dofile('board.lua');local F=dofile('graphics.lua');F.init();local g=M.new(123456,"+str(n)+");for i=1,12 do local p=(i*7)%#g.b+1;if M.trial(g,p)then M.play(g,p)end end;smudge.clear();B.draw(M,g,nil,F,width,height);add('</svg>')"
                        scenarios['counting']="local M=dofile('core.lua');local B=dofile('board.lua');local F=dofile('graphics.lua');F.init();local g=M.new(123456);for _,p in ipairs({31,40,32,41,33,42,23,50})do assert(M.play(g,p))end;M.play(g,0);M.play(g,0);M.mark(g,40);smudge.clear();B.draw(M,g,nil,F,width,height);add('</svg>')"
                    else:
                        for page in [1,5,7,8]:scenarios['help-'+str(page)]="local B=dofile('board.lua');local F=dofile('graphics.lua');local H=dofile('lesson.lua');F.init();smudge.clear();H.draw("+str(page)+",F,width,height,B);add('</svg>')"
                        for n in [0,2,6,8,9]:
                            scenarios['map-'+str(n)]="local M=dofile('core.lua');local B=dofile('board.lua');local F=dofile('graphics.lua');F.init();local g=M.new(123456,"+str(n)+");for i=1,6 do M.apply(g,M.ai(g))end;smudge.clear();B.draw(M,g,nil,nil,F,width,height);add('</svg>')"
                    for scenario,body in scenarios.items():
                        (fixture/'capture.lua').write_text(prefix+body)
                        for state in fixture.glob('state-*.dat'):state.unlink()
                        rendered=subprocess.run([str(runner),'capture.lua',str(budget),str(width),str(height)],cwd=fixture,text=True,capture_output=True,timeout=120)
                        assert rendered.returncode==0,rendered.stderr
                        target=args.captures/(app+'-'+scenario+'-'+str(width)+'x'+str(height)+'.svg')
                        shutil.copyfile(fixture/'render.svg',target)
                        if shutil.which('rsvg-convert'):subprocess.run(['rsvg-convert',str(target),'-o',str(target.with_suffix('.png'))],check=True)

args.output.write_text(json.dumps(records,indent=2)+'\n')
assert all(record['returncode']==0 for record in records)
