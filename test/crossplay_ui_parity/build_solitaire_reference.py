"""Compile original Solitaire builders and core, with explicit mapped data only.
Requires the production raster and original font scaffold from build_native.py.
"""
import argparse,json,hashlib,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--reference',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();C=a.reference.resolve();O=a.output.resolve()
paths=['src/apps_local/solitaire/SolitaireCore.cpp','src/apps_local/solitaire/SolitaireCore.h','src/apps_local/solitaire/SolitaireScreens.cpp','src/apps_local/solitaire/SolitaireScreens.h','src/apps_local/cards/CardArt.cpp','src/apps_local/cards/CardArt.h','src/apps_local/cards/CardSuits.h']
hashes={}
for path in paths:
 data=(C/path).read_bytes();assert data==subprocess.check_output(['git','show','d3a49ce8e85f9bd0d3e0c774a54adc267fd7547b:'+path],cwd=C);hashes[path]=hashlib.sha256(data).hexdigest()
s=(O/'native_reference.cpp').read_text();s=s[:s.index('class BattleshipActivity')]
# Use original truncation too: source Win intentionally clips a long button.
gfx=(C/'lib/GfxRenderer/GfxRenderer.cpp').read_text()
start=gfx.index('std::string GfxRenderer::truncatedText(');brace=gfx.index('{',start);end=brace+1;depth=1
while depth:
 if gfx[end]=='{':depth+=1
 if gfx[end]=='}':depth-=1
 end+=1
old='std::string truncatedText(int id,const char*s,int width,EpdFontFamily::Style st)const{if(getTextWidth(id,s,st)>width){fprintf(stderr,"Unexpected native truncation: %s\\n",s);abort();}return s;}'
assert old in s
s=s.replace(old,'std::string truncatedText(int,const char*,int,EpdFontFamily::Style)const; int resolveTextFontId(int id,const char*,EpdFontFamily::Style)const{return id;}')
s+=gfx[start:end]+'\n'
s+='\n#include "'+str(C/'src/apps_local/solitaire/SolitaireScreens.h')+'"\n'
s+=r'''
int main(int argc,char**argv){if(argc!=6)return 2;std::string kind=argv[1];int w=atoi(argv[2]),h=atoi(argv[3]);GfxRenderer r;r.orientation=w<h?GfxRenderer::Portrait:GfxRenderer::LandscapeCounterClockwise;r.panelWidth=800;r.panelHeight=480;r.panelWidthBytes=100;memset(r.frame,255,sizeof(r.frame));
 freeink::ui::GfxRendererTarget target(r,true);target.setFont(0,10);target.setFont(1,20);target.setFont(2,30);auto device=target.deviceContext();freeink::ui::InputSnapshot input{};toybox::Interactions interactions;toybox::Frame frame(target,device,input,interactions);auto tokens=toybox::themeTokens();tokens.headerHeight=solitaireui::kHeaderBand;toybox::Screen screen(frame,tokens);
 std::ifstream state(argv[4]);solitaire::Game::Save z{};int mode,moves,selected,card,played,wins,streak;std::string results;state>>mode>>moves>>selected>>card>>played>>wins>>streak>>results;z.seed=123456;z.moves=moves;z.drawThree=mode;for(int p=0;p<13;p++){int n;state>>n;if(n<0||n>24)return 4;z.counts[p]=n;for(int i=0;i<n;i++){int c;state>>c;z.piles[p][i]=c;}}
 solitaire::Game g;bool replayed=false;
 if(moves==1){g.deal(123456,mode);g.drawFromStock();solitaire::Game::Save actual{};g.save(actual);replayed=true;for(int p=0;p<13;p++){if(actual.counts[p]!=z.counts[p])replayed=false;for(int i=0;i<z.counts[p];i++)if(actual.piles[p][i]!=z.piles[p][i])replayed=false;}}
 if(!replayed&&!g.restore(z))return 5;
 if(kind=="menu"){solitaireui::MenuModel m{};m.hasSave=selected!=0;m.savedMoves=moves;m.drawThree=mode;m.played=played;m.wins=wins;m.streak=streak;for(int i=0;i<16;i++)if(i<int(results.size())&&results[i]>='1'&&results[i]<='2')m.recent[i]=results[i]-'0';solitaireui::buildMenu(screen,m);}
 else if(kind=="win"){solitaireui::WinModel m{};m.moves=moves;m.wins=wins;m.streak=streak;m.drawThree=mode;solitaireui::buildWin(screen,m);}
 else {solitaireui::BoardModel m{};m.game=&g;m.selectedPile=selected;m.selectedCard=card;solitaireui::Layout l{};solitaireui::buildBoard(screen,m,l);}
 FILE*f=fopen(argv[5],"wb");fprintf(f,"P5\n%d %d\n255\n",w,h);for(int y=0;y<h;y++)for(int x=0;x<w;x++){int px,py;rotateCoordinates(r.orientation,x,y,&px,&py,r.panelWidth,r.panelHeight);fputc(r.frame[py*r.panelWidthBytes+px/8]&(128>>(px%8))?255:0,f);}fclose(f);return 0;}
'''
(O/'solitaire_reference.cpp').write_text(s);(O/'solitaire-inputs.json').write_text(json.dumps(hashes,indent=2)+'\n')
cmd=['c++','-O2','-std=c++17','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I'+str(O),'-I'+str(Path(__file__).resolve().parents[2]/'lib/Lua/src'),'-I'+str(C/'lib/EpdFont'),'-I'+str(C/'lib/Utf8'),'-I'+str(C/'freeink-sdk/libs/ui/FreeInkUI/include'),'-I'+str(C/'freeink-sdk/libs/assets/Icons/include'),str(O/'solitaire_reference.cpp'),*[str(C/x)for x in ['lib/EpdFont/EpdFont.cpp','lib/EpdFont/EpdFontFamily.cpp','lib/Utf8/Utf8.cpp','freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp','src/apps_local/solitaire/SolitaireCore.cpp','src/apps_local/solitaire/SolitaireScreens.cpp','src/apps_local/cards/CardArt.cpp']],'-o',str(O/'solitaire-reference')]
subprocess.run(cmd,check=True)
