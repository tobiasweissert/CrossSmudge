"""Isolated references calling unchanged original builders with original glyphs."""
from pathlib import Path
import argparse,hashlib,json,subprocess
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();O=a.output.resolve();O.mkdir(parents=True,exist_ok=True);N=Path('/root/github/.host-data/crosssmudge/ported-ui-parity/native-catalog-references')
backend=(N/'native_catalog.cpp').read_text().split('int main(')[0];dispatch='';ids=[]
for title,ns,pages in [('minesweeper','mineui',5),('yahtzee','yzui',4)]:
 for page in range(pages):
  name=f'four-{title}-help{page+1}';ids.append(name);dispatch+=f'if(app=="{name}"){{{ns}::HowToModel m{{}};m.page={page};{ns}::buildHowTo(screen,m);}}\n'
for title,ns in [('go','goui'),('hex','hexui')]:
 name=f'four-{title}-settings';ids.append(name);dispatch+=f'if(app=="{name}"){{{ns}::SettingsModel m{{}};{ns}::buildSettings(screen,m);}}\n'
ids.append('four-hex-map');dispatch+=r'''
if(app=="four-hex-map"){
 FILE*in=fopen(argv[5],"rb");if(!in)return 20;char cells[128]={};int turn,status,last,mode,seat;if(fscanf(in,"%127s %d %d %d %d %d",cells,&turn,&status,&last,&mode,&seat)!=6)return 21;char view[16]={};int wins=0,losses=0,progress=0;fscanf(in,"%15s %d %d %d",view,&wins,&losses,&progress);fclose(in);if(strlen(cells)!=121||strspn(cells,"012")!=121||turn<1||turn>2||seat<1||seat>2||last<0||last>121)return 22;
 hexui::BoardModel m{};hex::reset(m.game);for(int i=0;i<121;i++){m.game.put(i,cells[i]-'0');if(cells[i]!='0')m.game.moveNumber++;}m.game.toMove=turn;m.game.winner=status;m.game.lastMove=last?last-1:hex::kNoCell;m.seat=seat;m.sharedDevice=mode==2;
 if(std::string(view)=="menu"){hexui::MenuModel r{};r.boardCells=m.game.cell;r.wins=wins;r.losses=losses;r.inProgress=progress;hexui::buildMenu(screen,r);}else if(status){hexui::ResultModel r{};r.game=m.game;r.seat=seat;r.sharedDevice=m.sharedDevice;hex::winningChain(m.game,r.chain);hexui::buildResult(screen,r);}else hexui::buildBoard(screen,m);
}
'''
ids.append('four-minesweeper-map');dispatch+=r'''
if(app=="four-minesweeper-map"){
 FILE*in=fopen(argv[5],"rb");if(!in)return 20;char cells[128]={},view[16]={};int status,played=0,won=0,flag=0;if(fscanf(in,"%127s %d %15s %d %d %d",cells,&status,view,&played,&won,&flag)<3)return 21;fclose(in);if(strlen(cells)!=80||strspn(cells,"01234567")!=80||status<0||status>3)return 22;
 mineui::BoardModel m{};for(int row=0;row<10;row++)for(int col=0;col<8;col++)m.game.cell[col][row]=cells[row*8+col]-'0';m.game.status=static_cast<minesweeper::Status>(status);m.flagMode=flag;m.showMines=status>=2;
 if(std::string(view)=="result"){mineui::ResultModel r{};r.won=status==2;for(int row=0;row<10;row++)for(int col=0;col<8;col++){auto v=m.game.cell[col][row];if((v&2)&&!(v&1))r.revealed++;if((v&5)==5)r.flagsRight++;}mineui::buildResult(screen,r);}
 else if(std::string(view)=="menu"){mineui::MenuModel r{};r.hasHistory=true;r.lastBoard=m.game;r.wins=won;r.losses=played-won;mineui::buildMenu(screen,r);}
 else mineui::buildBoard(screen,m);
}
'''
ids.append('four-yahtzee-map');dispatch+=r'''
if(app=="four-yahtzee-map"){
 FILE*in=fopen(argv[5],"rb");if(!in)return 20;int v[36];for(int&i:v)if(fscanf(in,"%d",&i)!=1)return 21;char view[16]={};int played=0,won=0,best=0,yahtzees=0,face=0;fscanf(in,"%15s %d %d %d %d %d",view,&played,&won,&best,&yahtzees,&face);fclose(in);if(v[0]<1||v[0]>2||v[1]<0||v[1]>3||v[2]<0||v[2]>31)return 22;
 yzui::CardModel m{};yahtzee::start(m.game,123456);m.game.turn=v[0]-1;m.game.rollsUsed=v[1];m.game.held=v[2];for(int i=0;i<5;i++)m.game.die[i]=v[3+i];for(int player=0;player<2;player++){for(int i=0;i<13;i++)m.game.card[player].box[i]=v[8+player*14+i];m.game.card[player].yahtzeeBonuses=v[8+player*14+13];}m.yourTurn=m.game.turn==0;m.takeable=yahtzee::takeableBoxes(m.game,0);m.joker=yahtzee::jokerForcing(m.game,0);
 if(std::string(view)=="menu"){yzui::MenuModel r{};r.played=played;r.won=won;r.best=best;r.yahtzees=yahtzees;r.yahtzeeFace=face;yzui::buildMenu(screen,r);}else if(yahtzee::over(m.game)){yzui::ResultModel r{};r.yours=m.game.card[0];r.theirs=m.game.card[1];r.yourTotal=yahtzee::total(r.yours);r.theirTotal=yahtzee::total(r.theirs);yzui::buildResult(screen,r);}else yzui::buildCard(screen,m);
}
'''
for title,ns in [('go','goui'),('hex','hexui')]:
 name=f'four-{title}-settings-map';ids.append(name);dispatch+=f'if(app=="{name}"){{FILE*in=fopen(argv[5],"rb");if(!in)return 20;int mode,seat,handicap,board,selected;if(fscanf(in,"%d %d %d %d %d",&mode,&seat,&handicap,&board,&selected)!=5)return 21;fclose(in);{ns}::SettingsModel m{{}};m.opponent=static_cast<{title}::Opponent>(mode==1?0:1);m.playAs=seat;m.selected=selected;'+('m.handicap=handicap;m.boardSize=board;'if title=='go'else'')+f'{ns}::buildSettings(screen,m);}}\n'
ids.append('four-go-map');dispatch+=r'''
if(app=="four-go-map"){
 FILE*in=fopen(argv[5],"rb");if(!in)return 20;int n,turn,phase,moves,passes,last,c1,c2,komi,handicap,seat,solo,aim,black,white;char cells[400]={},dead[400]={};if(fscanf(in,"%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %399s %399s",&n,&turn,&phase,&moves,&passes,&last,&c1,&c2,&komi,&handicap,&seat,&solo,&aim,&black,&white,cells,dead)!=17)return 21;char view[16]={};int wins=0,losses=0,lastWon=0,margin=0,progress=0;fscanf(in,"%15s %d %d %d %d %d",view,&wins,&losses,&lastWon,&margin,&progress);fclose(in);if(!go::isBoardSize(n)||strlen(cells)!=size_t(n*n)||strlen(dead)!=size_t(n*n)||strspn(cells,"012")!=strlen(cells)||strspn(dead,"01")!=strlen(dead))return 22;
 goui::BoardModel m{};go::reset(m.game,n,handicap,komi);for(int i=0;i<n*n;i++){m.game.put(i,cells[i]-'0');if(dead[i]=='1')go::mark(m.game.dead,i);}m.game.toMove=turn;m.game.stage=phase;m.game.moveNumber=moves;m.game.passes=passes;m.game.lastMove=last?last-1:moves?go::kPass:go::kNoPoint;m.game.capturedBy[1]=c1;m.game.capturedBy[2]=c2;m.seat=seat;m.sharedDevice=solo==0;m.yourTurn=solo==0||turn==seat;m.aimed=aim?aim-1:go::kNothingAimed;m.theyPassed=moves>0&&last==0;m.yourTurn=solo==0||turn==seat;
 if(std::string(view)=="menu"){goui::MenuModel r{};uint8_t points[go::kMaxPoints]={};for(int i=0;i<n*n;i++)points[i]=cells[i]-'0';r.boardPoints=points;r.boardSize=n;r.moveNumber=moves;r.inProgress=progress;r.hasHistory=wins+losses>0;r.wins=wins;r.losses=losses;r.lastWon=lastWon;r.lastMarginHalves=margin;goui::buildMenu(screen,r);}else if(phase==1){goui::CountModel r{};r.game=m.game;r.seat=seat;r.sharedDevice=m.sharedDevice;go::territory(r.game,r.owner);r.blackHalves=black;r.whiteHalves=white;goui::buildCount(screen,r);}else if(phase==2){goui::ResultModel r{};r.game=m.game;r.seat=seat;r.sharedDevice=m.sharedDevice;go::territory(r.game,r.owner);r.blackHalves=black;r.whiteHalves=white;goui::buildResult(screen,r);}else goui::buildBoard(screen,m);
}
'''
main=r'''int main(int argc,char**argv){if(argc!=5&&argc!=6)return 2;std::string app=argv[1];bool known=false;for(const auto*id:{KNOWN})if(app==id)known=true;if(!known)return 11;int w=atoi(argv[2]),h=atoi(argv[3]);std::string out=argv[4];nativeTrace=fopen((out+".trace").c_str(),"wb");GfxRenderer r;r.orientation=w<h?GfxRenderer::Portrait:GfxRenderer::LandscapeCounterClockwise;r.panelWidth=800;r.panelHeight=480;r.panelWidthBytes=100;memset(r.frame,255,sizeof(r.frame));toybox::ensureFonts(r);freeink::ui::GfxRendererTarget target(r,true);target.setFont(0,10);target.setFont(1,12);target.setFont(2,13);auto device=target.deviceContext();freeink::ui::InputSnapshot input{};toybox::Interactions interactions;toybox::Frame frame(target,device,input,interactions);toybox::Screen screen(frame,toybox::themeTokens());
DISPATCH
FILE*f=fopen(out.c_str(),"wb");fprintf(f,"P5\n%d %d\n255\n",w,h);for(int y=0;y<h;y++)for(int x=0;x<w;x++){int px,py;rotateCoordinates(r.orientation,x,y,&px,&py,r.panelWidth,r.panelHeight);fputc(r.frame[py*r.panelWidthBytes+px/8]&(128>>(px%8))?255:0,f);}fclose(f);fclose(nativeTrace);return 0;}'''
main=main.replace('KNOWN',','.join('"'+x+'"'for x in ids)).replace('DISPATCH',dispatch);source=O/'native_four.cpp';source.write_text(backend+main)
cmd=json.loads((N/'compile-command.json').read_text());cmd=[str(source)if x==str(N/'native_catalog.cpp')else str(O/'native-four')if x==str(N/'native-catalog')else x for x in cmd];cmd=[x for x in cmd if not('src/apps_local/'in x and x.endswith('.cpp')and not any(t in x for t in('/go/','/hex/','/minesweeper/','/yahtzee/','/link/','/player/')))]
(O/'compile-command.json').write_text(json.dumps(cmd,indent=2)+'\n');r=subprocess.run(cmd,text=True,capture_output=True);(O/'compile.log').write_text(r.stdout+r.stderr);print(r.returncode,r.stderr[-1600:]);r.check_returncode();(O/'provenance.json').write_text(json.dumps(dict(reference='d3a49ce8',backend_prefix_sha256=hashlib.sha256(backend.encode()).hexdigest(),source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),fixture_ids=ids),indent=2)+'\n')
