#!/usr/bin/env python3
"""Combined apps + real HTTP confinement policy; native fixture storage/rendering."""
import os, pathlib, shutil, subprocess, tempfile, json
R=pathlib.Path(__file__).resolve().parents[2]
O=pathlib.Path(os.environ['CROSSPLAY_EVIDENCE'])
records=[]
with tempfile.TemporaryDirectory(dir=os.environ['TMPDIR'],prefix='sandbox-') as tmp:
 w=pathlib.Path(tmp); objects=[]
 for source in sorted((R/'lib/Lua/src').glob('*.c')):
  obj=w/(source.stem+'.o');subprocess.run(['cc','-O1','-I'+str(R/'lib/Lua/src'),'-c',str(source),'-o',str(obj)],check=True);objects.append(str(obj))
 miniz=w/'miniz.o';subprocess.run(['cc','-O1','-I'+str(R/'lib/miniz/src'),'-c',str(R/'lib/miniz/src/miniz_impl.c'),'-o',str(miniz)],check=True);objects.append(str(miniz))
 for group in ['crossplay_ports','replacement_chess','replacement_sudoku','replacement_solitaire','replacement_wordle']:
  here=R/'test'/group
  source=(here/'runner.c').read_text()
  source='#include <stdbool.h>\nextern bool integration_allowed(const char*);\n'+source
  for func,denied in [('readfile','lua_pushnil(L);return 1;'),('writefile','lua_pushboolean(L,0);return 1;'),('dofile_app','return luaL_error(L,"path outside app");')]:
   marker='static int '+func+'(lua_State *L){' if 'static int '+func+'(lua_State *L){' in source else 'static int '+func+'(lua_State*L){'
   assert marker in source,(group,func)
   source=source.replace(marker,marker+'if(!integration_allowed(luaL_checkstring(L,1))){'+denied+'}',1)
  # Insert at the final execution boundary, after the fixture API is installed.
  marker='int err=' if 'int err=' in source else 'int err ='
  assert marker in source,group
  source=source.replace(marker,'extern void integration_sandbox(lua_State*); integration_sandbox(L); '+marker,1)
  src=w/(group+'.c');src.write_text(source);obj=w/(group+'.o');binary=w/group
  subprocess.run(['cc','-O1','-I'+str(R/'lib/Lua/src'),'-I'+str(here),'-c',str(src),'-o',str(obj)],check=True)
  subprocess.run(['c++','-O1','-std=c++20','-I'+str(R/'lib/Lua/src'),'-I'+str(R/'lib/LuaHttp'),'-I'+str(R/'lib/LuaBrowser'),'-I'+str(R/'lib/Memory'),'-I'+str(R/'lib/miniz/src'),str(obj),str(R/'test/integration_catalog/sandbox_adapter.cpp'),str(R/'lib/LuaHttp/LuaHttp.cpp'),str(R/'lib/LuaHttp/HttpCore.cpp'),str(R/'lib/LuaHttp/HttpGzip.cpp'),str(R/'lib/hal/HalLuaResources.cpp'),*objects,'-lm','-ldl','-o',str(binary)],check=True)
  apps=sorted(p.name for p in (R/'apps').glob('cp-*') if p.name not in ['cp-hackernews','cp-trivia']) if group=='crossplay_ports' else [group.removeprefix('replacement_')]
  for app in apps:
   with tempfile.TemporaryDirectory(dir=w) as fixture:
    f=pathlib.Path(fixture);shutil.copytree(R/'apps'/app,f,dirs_exist_ok=True)
    for folder in ['storage','legacy','import']:(f/folder).mkdir(exist_ok=True)
    # Test ordinary callback/save/reload paths plus denial of other app/user data.
    check="assert(smudge.read_file('/books/private.epub')==nil);assert(not smudge.write_file('/.smudge/settings.json','bad'));assert(smudge.read_file('../other/save')==nil);"
    (f/'denials.lua').write_text(check)
    denial=subprocess.run([str(binary),'denials.lua','76800'],cwd=f,env={**os.environ,'INTEGRATION_APP':app},capture_output=True,text=True,timeout=30)
    records.append(dict(app=app,test='denials.lua',cap=76800,exit=denial.returncode,stdout=denial.stdout,stderr=denial.stderr))
    if app.startswith('cp-'):
     name=app[3:]
     script='notes_integration.lua' if name=='notes' else 'calculator_integration.lua' if name=='calculator' else 'yahtzee_integration.lua' if name=='yahtzee' else 'five_ui.lua' if name in ['jaipur','seasalt','toybattle','go','murdle'] else 'party_integration.lua' if name in ['insider','forehead','wavelength','hearts'] else 'integration.lua'
     tests=[here/script,here/'journal.lua'];cap=76800
    else:
     tests=[here/'journal.lua',here/('runtime.lua' if app in ['chess','sudoku'] else 'integration.lua')];cap=2097152
    for test in tests:
     if not test.exists():continue
     script=test.read_text()
     # Production read/write denial is nil/false; old fixture adapters throw.
     script=script.replace("assert(not pcall(smudge.write_file,'../outside','bad'))", "assert(not smudge.write_file('../outside','bad'))")
     script=script.replace("assert(not pcall(smudge.read_file,'/books/other.txt'))", "assert(smudge.read_file('/books/other.txt')==nil)")
     (f/'integration_check.lua').write_text(script)
     for width,height in [(480,800),(800,480)]:
      with tempfile.TemporaryDirectory(dir=w) as case:
       c=pathlib.Path(case);shutil.copytree(f,c,dirs_exist_ok=True)
       p=subprocess.run([str(binary),'integration_check.lua',str(cap),str(width),str(height)],cwd=c,env={**os.environ,'INTEGRATION_APP':app},capture_output=True,text=True,timeout=180)
      records.append(dict(app=app,test=test.name,width=width,cap=cap,exit=p.returncode,stdout=p.stdout,stderr=p.stderr))
      print(app,test.name,width,p.returncode,p.stderr.strip(),flush=True)
(O/'sandbox.json').write_text(json.dumps(records,indent=2)+'\n')
assert all(x['exit']==0 for x in records)
