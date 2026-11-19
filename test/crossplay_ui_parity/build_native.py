from pathlib import Path
import subprocess,json,hashlib,re,argparse
parser=argparse.ArgumentParser(description="Compile pinned original native Battleship reference state")
parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[2])
parser.add_argument('--reference',type=Path,required=True)
parser.add_argument('--revision',default='d3a49ce8e85f9bd0d3e0c774a54adc267fd7547b')
parser.add_argument('--output',type=Path,required=True)
a=parser.parse_args();O=a.output.resolve();C=a.reference.resolve();R=a.root.resolve();O.mkdir(parents=True,exist_ok=True)
# Verify every canonical original input before compiling; do not bless moving source.
paths=['src/apps_local/battleship/BattleshipActivity.cpp','src/apps_local/battleship/BattleshipScreens.cpp','src/apps_local/battleship/BattleshipScreens.h','src/apps_local/battleship/BattleshipCore.cpp','src/apps_local/battleship/BattleshipCore.h','src/apps_local/ui/Toybox.h','src/apps_local/ui/ToyboxScreen.h','src/apps_local/ui/ToyboxText.h','src/apps_local/ui/ToyboxTokens.h','src/apps_local/ui/ToyboxMetrics.h','src/apps_local/ui/fonts/toybox_10.h','src/apps_local/ui/fonts/toybox_20.h','src/apps_local/ui/fonts/toybox_30.h','lib/GfxRenderer/GfxRenderer.cpp','lib/EpdFont/EpdFont.cpp','lib/EpdFont/EpdFont.h','lib/EpdFont/EpdFontData.h','lib/EpdFont/EpdFontFamily.cpp','lib/EpdFont/EpdFontFamily.h','lib/Utf8/Utf8.cpp','lib/Utf8/Utf8.h','src/apps_local/link/LinkScreens.cpp','src/apps_local/link/LinkScreens.h','src/apps_local/player/PlayerAvatar.cpp','src/apps_local/player/PlayerName.cpp']
paths=sorted(set(paths+subprocess.check_output(['git','ls-files','src/apps_local/battleship','src/apps_local/ui','src/apps_local/link','src/apps_local/player','lib/EpdFont','lib/Utf8','lib/GfxRenderer/RevealedInteractions.h'],cwd=C,text=True).splitlines()))
inputs={}
for path in paths:
 data=(C/path).read_bytes();expected=subprocess.check_output(['git','show',a.revision+':'+path],cwd=C)
 assert data==expected,'Original reference changed: '+path
 inputs[path]=hashlib.sha256(data).hexdigest()
sdk_sha=subprocess.check_output(['git','rev-parse',a.revision+':freeink-sdk'],cwd=C,text=True).strip()
for path in subprocess.check_output(['git','ls-files','libs/ui/FreeInkUI/include','libs/ui/FreeInkUI/src','libs/assets/Icons/include'],cwd=C/'freeink-sdk',text=True).splitlines():
 data=(C/'freeink-sdk'/path).read_bytes();expected=subprocess.check_output(['git','show',sdk_sha+':'+path],cwd=C/'freeink-sdk')
 assert data==expected,'Original SDK reference changed: '+path
 inputs['freeink-sdk/'+path]=hashlib.sha256(data).hexdigest()
(O/'native-inputs.json').write_text(json.dumps({'revision':a.revision,'sdk_revision':sdk_sha,'files':inputs},indent=2)+'\n')
def function(source,start):
 i=source.index(start);b=source.index('{',i);j=b+1;level=1
 while level:
  if source[j]=='{':level+=1
  if source[j]=='}':level-=1
  j+=1
 return source[i:j]
s=(C/'lib/GfxRenderer/GfxRenderer.cpp').read_text()
# Reuse only class backing/scaffolding from exact production capture. Replace all primitive definitions with ORIGINAL methods.
base=(O/'production_adapter.cpp').read_text();base=base[:base.index('struct Runner')]
a=base.index('static inline void rotateCoordinates(')
base=base[:a]+function(s,'static inline void rotateCoordinates(')+'\n'+function(s,'void GfxRenderer::drawPixel(const')+'\n'+s[s.index('void GfxRenderer::drawLine(int x1'):s.index('void GfxRenderer::drawImage(const')]
includes='''#include <map>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <EpdFontFamily.h>
#include "/root/github/crossplay/src/apps_local/ui/fonts/toybox_10.h"
#include "/root/github/crossplay/src/apps_local/ui/fonts/toybox_20.h"
#include "/root/github/crossplay/src/apps_local/ui/fonts/toybox_30.h"
static EpdFont f10(&toybox_10),f20(&toybox_20),f30(&toybox_30);
'''
base=base.replace('class GfxRenderer{public:',includes+'class GfxRenderer{public:')
extra='''
 std::map<int,EpdFontFamily>fontMap={{10,EpdFontFamily(&f10)},{20,EpdFontFamily(&f20)},{30,EpdFontFamily(&f30)}};
 const std::map<int,EpdFontFamily>&getFontMap()const{return fontMap;}
 int getFontAscenderSize(int id)const{return fontMap.at(id).getData()->ascender;}
 int getLineHeight(int id)const{return fontMap.at(id).getData()->advanceY;}
 int getTextWidth(int id,const char*s,EpdFontFamily::Style style=EpdFontFamily::REGULAR)const{int w,h;fontMap.at(id).getTextDimensions(s,&w,&h,style);return w;}
 void getOrientedViewableTRBL(int*t,int*r,int*b,int*l)const{*t=*r=*b=*l=0;}
 // All reference strings here are uppercase ASCII, no kerning, no fallback and no synthesized style.
 void drawText(int id,int x,int y,const char*s,bool ink=true,EpdFontFamily::Style st=EpdFontFamily::REGULAR)const{
  if(nativeTrace)fprintf(nativeTrace,"TEXT %d %d %d %d %s\\n",id,x,y,int(ink),s);auto&f=fontMap.at(id);auto*d=f.getData(st);int prevAdvance=0;bool first=true;
  for(const unsigned char*p=(const unsigned char*)s;*p;p++){if(*p<32||*p>126){fprintf(stderr,"unsupported native font text\\n");abort();}auto*g=f.getGlyph(*p,st);if(!g)abort();if(!first)x+=fp4::toPixel(prevAdvance);first=false;prevAdvance=g->advanceX;
   for(int yy=0;yy<g->height;yy++)for(int xx=0;xx<g->width;xx++){int bit=yy*g->width+xx;if(d->bitmap[g->dataOffset+bit/8]&(128>>(bit%8)))drawPixel(x+g->left+xx,y+d->ascender-g->top+yy,ink);}
  }
 }
 void drawTextRotated90CW(int,int,int,const char*,bool,EpdFontFamily::Style)const{abort();}
 std::string truncatedText(int id,const char*s,int width,EpdFontFamily::Style st)const{if(getTextWidth(id,s,st)>width){fprintf(stderr,"Unexpected native truncation: %s\\n",s);abort();}return s;}
 std::vector<std::string> wrappedText(int id,const char*s,int width,int,EpdFontFamily::Style st)const{return {truncatedText(id,s,width,st)};}
 void fillPolygon(const int*,const int*,int,bool)const{abort();}
'''
extra+='\n int clipLeft_=0,clipTop_=0,clipRight_=32767,clipBottom_=32767;\n'
base='#define LOG_ERR(...) ((void)0)\n'+base
base=base.replace(' template<Color C>void fillRectImpl',extra+' template<Color C>void fillRectImpl')
# Make include guard satisfy original SDK GfxRendererTarget.
(O/'GfxRenderer.h').write_text('#pragma once\n')
base+='''
#include <FreeInkUIGfxRenderer.h>
#include "/root/github/crossplay/src/apps_local/battleship/BattleshipScreens.h"
#include "/root/github/crossplay/src/apps_local/battleship/BattleshipCore.h"
struct Rect{int x,y,width,height;};
namespace toybox{
'''+function((C/'src/apps_local/ui/Toybox.h').read_text(),'inline void cornerMarks(')+'''
constexpr int kTileFontId=10;
int drawCapsCentered(const GfxRenderer&r,int id,int x,int y,int h,const char*s,bool ink){auto*g=r.getFontMap().at(id).getGlyph('H');int py=y+(h-g->height)/2-r.getFontAscenderSize(id)+g->top;r.drawText(id,x,py,s,ink);return x;}
}
constexpr int kFleetCell=16,kMissRing=3;
constexpr Color Black=Color::Black,White=Color::White,LightGray=Color::LightGray;
class BattleshipActivity{public:
 struct GridGeometry{int cell,originX,originY;};
 GfxRenderer&renderer;Rect bodySlot; bship::Game game;int mySide=0,aimCell=-1,played=0,won=0,streak=0;char report[64]{},capsule[64]{};bool hasSavedGame=false;BattleshipActivity(GfxRenderer&r):renderer(r){}
 int opponentSide()const{return 1;}bool gameOver()const{return bship::over(game);}
 bool myTurn()const{return game.turn==mySide;}bool inMatch()const{return false;}const char*themWord(){return "THEY";}void saveStats()const{}void saveGame()const{}
 bool canAct()const{return myTurn();}void requestUpdate()const{}void fireAtAimed(){abort();}
 void handleTargetTap(int);void reportShot(bool);const char*capsuleLabel();void finishGame();
 GridGeometry targetGrid()const;GridGeometry fleetGrid()const;static Rect cellRect(const GridGeometry&,int);
 void drawGridLattice(const GridGeometry&,bool)const;void drawTargetGrid(bool)const;void drawFleetGrid()const;void drawRoster(const Rect&,bool)const;
};
'''
activity=(C/'src/apps_local/battleship/BattleshipActivity.cpp').read_text()
for start in ['BattleshipActivity::GridGeometry BattleshipActivity::targetGrid()', 'BattleshipActivity::GridGeometry BattleshipActivity::fleetGrid()', 'Rect BattleshipActivity::cellRect(', 'void BattleshipActivity::drawGridLattice(', 'void BattleshipActivity::drawTargetGrid(', 'void BattleshipActivity::drawFleetGrid(', 'void BattleshipActivity::drawRoster(', 'void BattleshipActivity::reportShot(', 'const char* BattleshipActivity::capsuleLabel()', 'void BattleshipActivity::finishGame()', 'void BattleshipActivity::handleTargetTap(']:base+=function(activity,start)+'\n'
base+='''
int main(int argc,char**argv){if(argc!=5)return 2;int w=atoi(argv[1]),h=atoi(argv[2]);GfxRenderer r;r.orientation=w<h?GfxRenderer::Portrait:GfxRenderer::LandscapeCounterClockwise;r.panelWidth=w<h?h:w;r.panelHeight=w<h?w:h;r.panelWidthBytes=r.panelWidth/8;memset(r.frame,255,sizeof(r.frame));BattleshipActivity app(r);std::ifstream state(argv[3]);std::string a,b,sa,sb;int turn,last;state>>a>>b>>sa>>sb>>turn>>last;if(a.size()!=100||b.size()!=100)return 3;
 for(int side=0;side<2;side++){auto board=side?b:a;auto shots=side?sb:sa;auto&model=app.game.side[side];model.placed=1;for(int ship=0;ship<5;ship++){int first=board.find(char('1'+ship));int next=board.find(char('1'+ship),first+1);model.fleet.ships[ship]={uint8_t(first),uint8_t(next==first+1)};}for(int i=0;i<100;i++)if(shots[i]=='1')bship::markShot(model,i);}
 app.game.turn=turn-1;app.game.lastShot=last;if(app.gameOver())app.game.turn=1-app.game.turn;
 int aim=0,tap=0;state>>aim>>tap;if(aim>0)app.aimCell=aim-1;
 
 freeink::ui::GfxRendererTarget target(r,true);target.setFont(0,10);target.setFont(1,20);target.setFont(2,30);auto device=target.deviceContext();freeink::ui::InputSnapshot input{};toybox::Interactions interactions;toybox::Frame frame(target,device,input,interactions);toybox::Screen screen(frame);bshipui::BoardModel model;if(app.gameOver())app.finishGame();else if(last)app.reportShot(app.game.turn==0);else std::snprintf(app.report,sizeof(app.report),"YOUR MOVE");if(tap>0)app.handleTargetTap(tap-1);model.report=app.report;model.status=app.capsuleLabel();model.gameOver=app.gameOver();model.canFire=app.canAct()&&app.aimCell>=0&&!app.gameOver();auto body=bshipui::buildBoardChrome(screen,model);app.bodySlot={body.x,body.y,body.width,body.height};app.drawTargetGrid(app.gameOver());app.drawFleetGrid();auto fleet=app.fleetGrid();int rx=fleet.originX+fleet.cell*10+toybox::kGutter*2;app.drawRoster({rx,fleet.originY,body.x+body.width-rx,fleet.cell*10},false);
 FILE*f=fopen(argv[4],"wb");fprintf(f,"P5\\n%d %d\\n255\\n",w,h);for(int y=0;y<h;y++)for(int x=0;x<w;x++){int px,py;rotateCoordinates(r.orientation,x,y,&px,&py,r.panelWidth,r.panelHeight);fputc(r.frame[py*r.panelWidthBytes+px/8]&(128>>(px%8))?255:0,f);}fclose(f);printf("native body %d,%d %dx%d target %d,%d cell%d\\n",body.x,body.y,body.width,body.height,app.targetGrid().originX,app.targetGrid().originY,app.targetGrid().cell);
}
'''
# Record every production primitive invocation, without replacing the implementation.
base=base.replace('enum class Color{','static FILE*nativeTrace=nullptr;\nenum class Color{')
def instrument(m):
 args=[]
 for arg in m.group(2).split(','):
  args.append(re.findall(r'[A-Za-z_][A-Za-z_0-9]*',arg)[-1])
 return m.group(0)+'if(nativeTrace)fprintf(nativeTrace,"'+m.group(1)+' '+','.join(['%d']*len(args))+'\\n",'+','.join('int('+a+')' for a in args)+');'
base=re.sub(r'void GfxRenderer::(drawLine|drawRect|drawRoundedRect|fillRect|fillRectDither|fillRoundedRect)\((.*?)\) const \{',instrument,base,flags=re.S)
base=base.replace('int w=atoi(argv[1]),h=atoi(argv[2]);','std::string tracePath=std::string(argv[4])+".trace";nativeTrace=fopen(tracePath.c_str(),"wb");int w=atoi(argv[1]),h=atoi(argv[2]);')
base=base.replace('fclose(f);printf(', 'fclose(f);fclose(nativeTrace);printf(')
base=base.replace('/root/github/crossplay',str(C))
(O/'native_reference.cpp').write_text(base)
cmd=['c++','-O2','-std=c++17','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I'+str(O),'-I'+str(R/'lib/Lua/src'),'-I'+str(C/'lib/EpdFont'),'-I'+str(C/'lib/Utf8'),'-I'+str(C/'freeink-sdk/libs/ui/FreeInkUI/include'),'-I'+str(C/'freeink-sdk/libs/assets/Icons/include'),str(O/'native_reference.cpp'),str(C/'lib/EpdFont/EpdFont.cpp'),str(C/'lib/EpdFont/EpdFontFamily.cpp'),str(C/'lib/Utf8/Utf8.cpp'),str(C/'freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp'),*[str(C/'src/apps_local'/p) for p in ['battleship/BattleshipScreens.cpp','battleship/BattleshipCore.cpp','link/LinkScreens.cpp','player/PlayerAvatar.cpp','player/PlayerName.cpp']],'-o',str(O/'native-reference')]
subprocess.run(cmd,check=True)
