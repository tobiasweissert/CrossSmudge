from pathlib import Path
import subprocess,hashlib,json,argparse,re,configparser
parser=argparse.ArgumentParser(description="Build actual production Lua shape/font pixel runner; no PlatformIO")
parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[2])
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args();R=args.root.resolve();O=args.output.resolve();O.mkdir(parents=True,exist_ok=True)
s=(R/'lib/GfxRenderer/GfxRenderer.cpp').read_text();h=(R/'lib/GfxRenderer/GfxRenderer.h').read_text();lua=(R/'src/activities/apps/lua/LuaRunner.cpp').read_text()
def function(source,start):
 i=source.index(start); b=source.index('{',i);level=1;j=b+1
 while level:
  if source[j]=='{':level+=1
  if source[j]=='}':level-=1
  j+=1
 return source[i:j]
methods=s[s.index('void GfxRenderer::drawLine(int x1'):s.index('void GfxRenderer::drawImage(const')]
# includes production implementations of the complete drawLine/Rect/Arc/RoundedRect/fill/dither paths.
proto=h[h.index('  void drawPixel(int'):h.index('  void drawImage(const')]
code='''#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
extern "C" {
#include "lua.h"
#include "lauxlib.h"
}
enum class Color{Clear,Black,White,LightGray,DarkGray};
struct DummyCache{bool isScanning()const{return false;}};
class GfxRenderer{public:
 enum Orientation{Portrait,PortraitInverted,LandscapeClockwise,LandscapeCounterClockwise};
 Orientation orientation=LandscapeCounterClockwise;
 void setOrientation(const Orientation o){orientation=o;}
 Orientation getOrientation()const{return orientation;}
 uint16_t panelWidth=800,panelHeight=480,panelWidthBytes=100;
 mutable uint8_t frame[800*800/8];uint8_t*frameBuffer=frame;
 DummyCache*fontCacheManager_=nullptr;
 bool textClipActive_=false,_stripActive=false;int textClipLeft_=0,textClipTop_=0,textClipRight_=0,textClipBottom_=0,_stripY0=0,_stripRows=0;uint8_t*_stripBuf=nullptr;
 int getScreenWidth()const{return orientation==Portrait||orientation==PortraitInverted?panelHeight:panelWidth;}
 int getScreenHeight()const{return orientation==Portrait||orientation==PortraitInverted?panelWidth:panelHeight;}
 uint8_t*getWriteTarget()const{return frameBuffer;}
 int getWriteOriginY()const{return 0;}int getWriteRows()const{return panelHeight;}
 template<Color C>void fillRectImpl(int,int,int,int)const;
 template<Color C>void drawPixelDither(int,int)const;
 template<Color C>void fillArc(int,int,int,int,int)const;
'''+proto+'};\n'+function(s,'static inline void rotateCoordinates(')+'\n'+function(s,'void GfxRenderer::drawPixel(const')+'\n'+methods
code+='''\nstruct Runner{GfxRenderer renderer_;};static Runner runner;static Runner*s_activeRunner=&runner;
'''
code+=function(lua,'static inline int getInt(')+'\n'+function(lua,'static inline int getOptInt(')+'\n'
funcs={'line':'l_line','thick_line':'l_thickLine','pixel':'l_pixel','rect':'l_rect','rect_dither':'l_rectDither','rounded_rect':'l_roundedRect','circle':'l_circle','get_bounds':'l_getBounds','get_orientation':'l_getOrientation','set_orientation':'l_setOrientation'}
for api,fn in funcs.items():code+=function(lua,'int LuaRunner::'+fn+'(').replace('int LuaRunner::'+fn+'(','extern "C" int prod_'+api+'(')+'\n'
# Storage-loading branch remains host adapter; raster loop is extracted unmodified from l_drawSprite.
blit=lua[lua.index('  for (int cy = 0; cy < h; ++cy)',lua.index('int LuaRunner::l_drawSprite')):lua.index('\nint LuaRunner::l_getMetrics',lua.index('int LuaRunner::l_drawSprite'))]
code+='''extern "C" int prod_bitmap(lua_State*L){int x=getInt(L,1),y=getInt(L,2),w=getInt(L,3),h=getInt(L,4);if(w<=0||h<=0)return 0;size_t len;const uint8_t*spriteData=(const uint8_t*)luaL_checklstring(L,5,&len);size_t stride=(w+7)/8;bool inverted=lua_toboolean(L,6),dither=lua_toboolean(L,7);if(len<stride*h)return luaL_error(L,"short glyph bitmap");auto&renderer=s_activeRunner->renderer_;
'''+blit
code+='''extern "C" void prod_bounds(int w,int h){auto&r=runner.renderer_;r.orientation=w<h?GfxRenderer::Portrait:GfxRenderer::LandscapeCounterClockwise;r.panelWidth=w<h?h:w;r.panelHeight=w<h?w:h;r.panelWidthBytes=r.panelWidth/8;memset(r.frame,255,sizeof(r.frame));}
extern "C" int prod_clear(lua_State*L){memset(runner.renderer_.frame,255,sizeof(runner.renderer_.frame));return 0;}
extern "C" int prod_capture(lua_State*L){auto&r=runner.renderer_;FILE*f=fopen(luaL_checkstring(L,1),"wb");if(!f)return luaL_error(L,"capture open");int w=r.getScreenWidth(),h=r.getScreenHeight();fprintf(f,"P5\\n%d %d\\n255\\n",w,h);for(int y=0;y<h;y++)for(int x=0;x<w;x++){int px,py;rotateCoordinates(r.orientation,x,y,&px,&py,r.panelWidth,r.panelHeight);fputc((r.frame[py*r.panelWidthBytes+px/8]&(128>>(px%8)))?255:0,f);}fclose(f);return 0;}
'''
(O/'production_adapter.cpp').write_text(code)
c=(R/'test/crossplay_ports/runner.c').read_text()
# Touch-present Sticky has no hardware button hint strip (UITheme::getMetrics).
# Other theme values remain explicitly host-adapted, not a full theme renderer.
theme=(R/'src/components/UITheme.cpp').read_text()
assert 'adjustedMetrics.buttonHintsHeight = 0;' in theme
c=c.replace('lua_pushinteger(L,40);lua_setfield(L,-2,"button_hints_height")','lua_pushinteger(L,0);lua_setfield(L,-2,"button_hints_height")')
(O/'theme-adapter.json').write_text(json.dumps({'touch_present':True,'button_hints_height':0,'source_sha256':hashlib.sha256(theme.encode()).hexdigest(),'other_theme_metrics':'host fixture defaults; not physical/theme acceptance'},indent=2)+'\n')
i=c.index('static const luaL_Reg api[]=')
newfuncs=list(funcs)+['bitmap','clear','capture']
c=c[:i]+''.join('int prod_'+fn+'(lua_State*);\n' for fn in newfuncs)+'void prod_bounds(int,int);\n'+c[i:]
c=c.replace('static const luaL_Reg api[]={','static const luaL_Reg api[]={ {\"get_orientation\",prod_get_orientation}, {\"set_orientation\",prod_set_orientation},')
for fn in newfuncs:c=c.replace('{"'+fn+'",draw}','{"'+fn+'",prod_'+fn+'}').replace('{"'+fn+'",bounds}','{"'+fn+'",prod_'+fn+'}')
for unsupported_drawing in ['text','centered_text','header','button_hints']:
 c=c.replace('{"'+unsupported_drawing+'",draw}','{"'+unsupported_drawing+'",unsupported}')
c=c.replace(' int err=luaL_loadfile',' prod_bounds(width,height);lua_pushcfunction(L,prod_capture);lua_setglobal(L,"capture");\n int err=luaL_loadfile')
c=c.replace('lua_setglobal(L,"smudge");','lua_setglobal(L,"smudge");')
# Compile the real device binding under actual configured Sticky-owner flags.
config=configparser.ConfigParser(interpolation=None,strict=False);config.read(R/'platformio.ini')
def option(section,key):
 if config.has_option(section,key):value=config.get(section,key)
 elif config.has_option(section,'extends'):return option(config.get(section,'extends').strip(),key)
 else:return ''
 return re.sub(r'\$\{([^}.]+)\.([^}]+)\}',lambda m:option(m[1],m[2]),value)
board_defines=re.findall(r'-D((?:FREEINK_DEVICE_|CROSSINK_APP_DEVICE_|CROSSINK_APP_CAP_)[A-Z0-9_]+)(?:=([^\s]+))?',option('env:sticky-owner-test','build_flags'))
board_args=['-D'+name+'='+(value or '1') for name,value in board_defines]
cap=(R/'include/AppCapabilities.h').read_text();mapping=re.search(r'#if defined\(FREEINK_DEVICE_X4CLASSIC\).*?#endif',cap,re.S).group()
identity='#include <cstring>\n'+mapping+'\n#include "LuaRuntime.h"\nclass LuaRunner{public:static int l_getDevice(lua_State*);static int l_hasFeature(lua_State*);};\n'+function(lua,'int LuaRunner::l_getDevice(')+'\n'+function(lua,'int LuaRunner::l_hasFeature(')+'\nextern "C" int prod_get_device(lua_State*L){return LuaRunner::l_getDevice(L); }\nextern "C" int prod_open_libraries(lua_State*L){return ink::runtime::openEssential(L); }\n'
identity+='\nextern \"C\" int prod_has_feature(lua_State*L){return LuaRunner::l_hasFeature(L); }\n'
(O/'production_identity.cpp').write_text(identity)
c=c.replace('static const luaL_Reg api[]={','int prod_get_device(lua_State*);\nint prod_has_feature(lua_State*);\nint prod_open_libraries(lua_State*);\nstatic const luaL_Reg api[]={ {"get_device",prod_get_device}, {"has_feature",prod_has_feature},')
start=c.index(' const luaL_Reg libs[]=');end=c.index(' lua_newtable(L);',start)
c=c[:start]+' if(prod_open_libraries(L)!=LUA_OK){fprintf(stderr,"Production Lua libraries failed\\n");lua_close(L);return 3;}\n'+c[end:]
(O/'runner.c').write_text(c)
subprocess.run(['c++','-O2','-std=c++17',*board_args,'-I'+str(R/'lib/Lua/src'),'-I'+str(R/'src/activities/apps/lua'),'-c',str(O/'production_identity.cpp'),'-o',str(O/'production_identity.o')],check=True)
(O/'identity-proof.json').write_text(json.dumps({'profile':'sticky-owner-test','defines':board_args,'function_sha256':hashlib.sha256(function(lua,'int LuaRunner::l_getDevice(').encode()).hexdigest(),'has_feature_sha256':hashlib.sha256(function(lua,'int LuaRunner::l_hasFeature(').encode()).hexdigest(),'libraries_sha256':hashlib.sha256((R/'src/activities/apps/lua/LuaRuntime.h').read_bytes()).hexdigest()},indent=2)+'\n')
subprocess.run(['cc','-O2','-I'+str(R/'lib/Lua/src'),'-c',str(O/'runner.c'),'-o',str(O/'runner.o')],check=True)
subprocess.run(['cc','-O2','-I'+str(R/'lib/Lua/src'),'-c',*[str(p) for p in sorted((R/'lib/Lua/src').glob('*.c'))]],cwd=O,check=True)
subprocess.run(['c++','-O2','-std=c++17','-I'+str(R/'lib/Lua/src'),'-c',str(O/'production_adapter.cpp'),'-o',str(O/'production_adapter.o')],check=True)
subprocess.run(['c++',str(O/'runner.o'),str(O/'production_adapter.o'),str(O/'production_identity.o'),*[str(O/(p.stem+'.o'))for p in sorted((R/'lib/Lua/src').glob('*.c'))],'-lm','-o',str(O/'capture-runner')],check=True)
(O/'production-methods.json').write_text(json.dumps({'source_files':{str(p.relative_to(R)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [R/'lib/GfxRenderer/GfxRenderer.cpp',R/'lib/GfxRenderer/GfxRenderer.h',R/'src/activities/apps/lua/LuaRunner.cpp']},'methods_bytes_sha256':hashlib.sha256(methods.encode()).hexdigest(),'coverage':'actual production drawPixel,rotateCoordinates,complete drawLine/Rect/Arc/RoundedRect/fill/dither implementations; actual Lua shape/getBounds bindings and bitmap raster loop. Host class fields, backing allocation, file read, capture output, library init adapters. NO host-font fallback. Logical output reverses physical orientation. Not device screenshot.'},indent=2)+'\n')
