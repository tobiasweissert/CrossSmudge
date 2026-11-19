"""Shared exact raster/identity plus actual production orientation bindings, isolated host output."""
from pathlib import Path
import argparse,subprocess,re,json,hashlib
R=Path(__file__).resolve().parents[2];p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();O=a.output.resolve();O.mkdir(parents=True,exist_ok=True)
subprocess.run(['python3',str(R/'test/crossplay_ui_parity/build_capture.py'),'--output',str(O)],check=True)
s=(R/'src/activities/apps/lua/LuaRunner.cpp').read_text()
def fun(source,start):
 i=source.index(start);b=source.index('{',i);j=b+1;n=1
 while n:n+=(source[j]=='{')-(source[j]=='}');j+=1
 return source[i:j]
cpp=(O/'production_adapter.cpp').read_text()
# The canonical builder already extracts these production bindings. Older
# adapters need the extension; never define the same methods twice.
if 'extern "C" int prod_set_orientation('not in cpp:
 cpp=cpp.replace(' int getScreenWidth()const', ' Orientation getOrientation()const{return orientation;}void setOrientation(Orientation next){orientation=next;}\n int getScreenWidth()const')
 for api,name in [('set_orientation','l_setOrientation'),('get_orientation','l_getOrientation')]:cpp+='\n'+fun(s,'int LuaRunner::'+name+'(').replace('int LuaRunner::'+name+'(','extern "C" int prod_'+api+'(')+'\n'
(O/'oriented_adapter.cpp').write_text(cpp)
c=(O/'runner.c').read_text()
if '{"set_orientation",prod_set_orientation}'not in c:c=c.replace('static const luaL_Reg api[]={','int prod_set_orientation(lua_State*);int prod_get_orientation(lua_State*);\nstatic const luaL_Reg api[]={ {"set_orientation",prod_set_orientation},{"get_orientation",prod_get_orientation},')
(O/'oriented_runner.c').write_text(c);inc='-I'+str(R/'lib/Lua/src')
subprocess.run(['c++','-O2','-std=c++17',inc,'-c',str(O/'oriented_adapter.cpp'),'-o',str(O/'oriented_adapter.o')],check=True)
subprocess.run(['cc','-O2',inc,'-c',str(O/'oriented_runner.c'),'-o',str(O/'oriented_runner.o')],check=True)
subprocess.run(['c++',str(O/'oriented_adapter.o'),str(O/'oriented_runner.o'),str(O/'production_identity.o'),*[str(O/(p.stem+'.o'))for p in sorted((R/'lib/Lua/src').glob('*.c'))],'-lm','-o',str(O/'runner')],check=True)
(O/'orientation-proof.json').write_text(json.dumps({'source_sha256':hashlib.sha256(s.encode()).hexdigest(),'binding':'actual Lua l_setOrientation/l_getOrientation methods; setter/getter class scaffolding updates original primitive framebuffer rotation; no Lua mock'},indent=2)+'\n')
