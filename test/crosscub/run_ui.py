#!/usr/bin/env python3
"""Current Sticky Lua screen/input audit through compiled production raster APIs.
Runs the existing current-design fixtures, not superseded legacy UI assertions.
Capture output is bounded to the final real raster per case; no host-font mock.
"""
from pathlib import Path
import argparse,json,os,shutil,subprocess,tempfile,struct,zlib,sys
R=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--runner',type=Path,required=True);a=p.parse_args();O=a.output.resolve();O.mkdir(parents=True,exist_ok=True)
cases=[]
def add(group,apps,tests):
 for app in apps:
  for test in tests:cases.append((group,app,test))
add('three_original_boards',['cp-connectfour','cp-knucklebones','cp-checkers'],['callbacks.lua','lifecycle.lua','gameplay.lua','results.lua','long_records.lua','malformed_records.lua','failed_records.lua'])
add('four_original_boards',['cp-go','cp-hex','cp-minesweeper','cp-yahtzee'],['callbacks.lua','records_bounds.lua'])
add('four_original_boards',['cp-go','cp-hex'],['settings.lua'])
add('four_original_boards',['cp-minesweeper','cp-yahtzee'],['failed_records.lua'])
for app,tests in {'go':['go_doors.lua','go_gameplay.lua','go_sizes.lua','go_repeat.lua','go_preferences.lua'],'hex':['hex_gameplay.lua','hex_repeat.lua','hex_preferences.lua','hex_completion_recovery.lua','hex_older_completion.lua'],'minesweeper':['mines_lessons.lua','mines_chord.lua','mines_gameplay.lua','mines_controls.lua'],'yahtzee':['yahtzee_results.lua','yahtzee_lessons.lua','yahtzee_joker.lua','yahtzee_gameplay.lua','yahtzee_complete.lua']}.items():add('four_original_boards',['cp-'+app],tests)
add('social_original_ui',['cp-insider'],['insider.lua','insider_stale.lua','insider_stale_role.lua','insider_stale_vote.lua'])
add('social_original_ui',['cp-forehead'],['forehead.lua','forehead_epoch.lua','forehead_epoch_persistent.lua','forehead_save.lua','forehead_stale_ready.lua'])
add('wavelength_original_ui',['cp-wavelength'],['baseline.lua','lifecycle.lua','privacy.lua','distances.lua','mode_duplicate.lua','abandon_limit.lua','states.lua','records.lua','queued.lua','caption.lua','queued_buttons.lua','single.lua'])
add('hearts_jaipur_original_ui',['cp-hearts','cp-jaipur'],['baseline.lua','interaction.lua','queued.lua'])
add('seasalt_toybattle_original_ui',['cp-seasalt','cp-toybattle'],['baseline.lua','interaction.lua','lifecycle.lua'])
add('seasalt_toybattle_original_ui',['cp-seasalt'],['records.lua','end_states.lua'])
add('seasalt_toybattle_original_ui',['cp-toybattle'],['maps.lua','toy_endings.lua'])
add('murdle_original_ui',['cp-murdle'],['callbacks.lua','lifecycle.lua','stale_back.lua'])
add('wordle_original_ui',['wordle'],['callbacks.lua','records.lua','missing_bank.lua','failed_write.lua','long_history.lua'])
add('crossplay_ui_parity',['cp-battleship'],['battleship.lua','battleship_report.lua','battleship_footer.lua','battleship_placement.lua','battleship_capture_placement.lua','battleship_partial.lua','battleship_buttons.lua','battleship_focus.lua','battleship_menu.lua','battleship_records.lua','battleship_cycles.lua','battleship_new_game.lua','battleship_capture_results.lua','battleship_targets.lua','battleship_capture_aim.lua','battleship_keyboard_aim.lua','battleship_import_own_view.lua'])
add('crossplay_ui_parity',['solitaire'],['solitaire.lua','solitaire_capture.lua','solitaire_selection.lua','solitaire_fans.lua','solitaire_menu.lua','solitaire_records.lua','solitaire_mapped.lua'])
# The standard existing raster fixture asserts production identity/library shape.
prefix="assert(smudge.get_device()=='sticky');assert(math.maxinteger==2147483647 and io==nil and package==nil and debug==nil);fixtureWrite=smudge.write_file\n"
prefix+="local nativeCapture=capture;local count=0;local prior;capture=function(name)count=count+1;if prior then fixtureWrite(prior,'',false)end;nativeCapture(name);prior=name;print('FRAME',count,name)end\n"
def png(pgm,target):
 raw=pgm.read_bytes();_,wh,_,pix=raw.split(b'\n',3);w,h=map(int,wh.split())
 def chunk(k,b):return struct.pack('>I',len(b))+k+b+struct.pack('>I',zlib.crc32(k+b))
 target.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,0,0,0,0))+chunk(b'IDAT',zlib.compress(b''.join(b'\0'+pix[y*w:(y+1)*w]for y in range(h))))+chunk(b'IEND',b''))
rows=[]
for group,app,test in cases:
 for w,h in [(480,800),(800,480)]:
  name=f'{group}-{app}-{Path(test).stem}-{w}x{h}';folder=R/'test'/group
  with tempfile.TemporaryDirectory(prefix='case-',dir=O)as tmp:
   work=Path(tmp)/'app';shutil.copytree(R/'apps'/app,work)
   for helper in folder.glob('*.lua'):
    shutil.copyfile(helper,work/('fixture-'+helper.name))
    if helper.name.startswith('export_'):shutil.copyfile(helper,work/helper.name)
   (work/'crosscub-fixture.lua').write_text(prefix+'APP='+repr(app.removeprefix('cp-'))+'\n'+(folder/test).read_text())
   try:t=subprocess.run([str(a.runner.resolve()),'crosscub-fixture.lua','2097152',str(w),str(h)],cwd=work,capture_output=True,text=True,timeout=180);code=t.returncode;out=t.stdout;err=t.stderr
   except subprocess.TimeoutExpired:code=124;out='';err='case deadline exceeded'
   images=[p for p in work.glob('*.pgm')if p.stat().st_size>0]
   if images:png(max(images,key=lambda p:p.stat().st_mtime_ns),O/(name+'.png'))
   metrics={}
   try:metrics=json.loads(out.strip().splitlines()[-1])
   except (ValueError,IndexError):pass
   rows.append(dict(group=group,app=app,test=test,width=w,height=h,exit=code,metrics=metrics,stdout=out,stderr=err,frames=sum(line.startswith('FRAME')for line in out.splitlines())))
   (O/(name+'.log')).write_text(out+err)
  (O/'results.json').write_text(json.dumps(rows,indent=2)+'\n');print(name,code,err.strip()[:200],flush=True)
print('TOTAL',len(rows),'FAILED',sum(row['exit']!=0 for row in rows),'FRAMES',sum(row['frames']for row in rows))
sys.exit(any(row['exit']!=0 for row in rows))
