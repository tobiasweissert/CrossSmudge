"""Extend frozen native harness in separate evidence; frozen artifacts remain untouched."""
import argparse,hashlib,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
N=Path('/root/github/.host-data/crosssmudge/ported-ui-parity/native-catalog-references')
s=(N/'native_catalog.cpp').read_text();s=s.replace('if(!knownFixture){','if(app.rfind("three-",0)==0)knownFixture=true;if(!knownFixture){')
extra=''
for title,ns in [('connectfour','c4ui'),('knucklebones','knuckleui'),('checkers','checkui')]:
 for page in range(3):
  extra+=f'if(app=="three-{title}-help{page+1}"){{{ns}::HowToModel m{{}};m.page={page};{ns}::buildHowTo(screen,m);}}\n'
extra+=r'''
if(app.rfind("three-",0)==0&&app.find("-map")!=std::string::npos){
 FILE* in=fopen(inputPath.c_str(),"rb");if(!in)return 20;char cells[128]={},history[128]={};int turn,status,picked,die,idle;fscanf(in,"%127s %d %d %d %d %d %127s",cells,&turn,&status,&picked,&die,&idle,history);fclose(in);
 if(app=="three-connectfour-map"){
  connectfour::Game g{};connectfour::start(g);for(int row=0;row<6;row++)for(int col=0;col<7;col++)g.cell[col][row]=cells[row*7+col]-'0';g.turn=turn;g.outcome=static_cast<connectfour::Outcome>(status);
  if(strlen(history)){g.lastColumn=history[strlen(history)-1]-'1';int row=0;while(row<6&&g.cell[g.lastColumn][row])row++;g.lastRow=row-1;}
  if(status){c4ui::ResultModel m{};m.game=g;m.outcome=g.outcome;connectfour::winningLine(g,g.turn,m.line);c4ui::buildResult(screen,m);}else{c4ui::BoardModel m{};m.game=g;m.open=connectfour::openColumns(g);m.yourTurn=turn==1;c4ui::buildBoard(screen,m);}
 }
 if(app=="three-knucklebones-map"){
  knuckleui::BoardModel m{};for(int col=0;col<3;col++)for(int row=0;row<3;row++){m.yours.cell[col][row]=cells[col*3+row]-'0';m.theirs.cell[col][row]=cells[9+col*3+row]-'0';}m.die=die;m.yourTurn=turn==1;
  if(status){knuckleui::ResultModel r{};r.yourScore=knucklebones::score(m.yours);r.theirScore=knucklebones::score(m.theirs);knuckleui::buildResult(screen,r);}else knuckleui::buildBoard(screen,m);
 }
 if(app=="three-checkers-map"){
  checkui::BoardModel m{};uint8_t map[]={0,1,3,5,7};for(int i=0;i<64;i++)m.game.cell[i]=map[cells[i]-'0'];m.game.turn=turn-1;m.game.idlePlies=idle;m.picked=picked-1;m.yourTurn=turn==1;m.movable=checkers::movableSquares(m.game);m.mustTake=checkers::captureAvailable(m.game);m.yourPieces=checkers::pieceCount(m.game,checkers::kLight);m.theirPieces=checkers::pieceCount(m.game,checkers::kDarkSeat);
  if(picked)m.destinationCount=checkers::destinations(m.game,picked-1,m.destinations,m.takenMasks,checkers::kMaxMoves);
  if(status){checkui::ResultModel r{};r.outcome=static_cast<checkers::Outcome>(status);r.yourPieces=m.yourPieces;r.theirPieces=m.theirPieces;checkui::buildResult(screen,r);}else checkui::buildBoard(screen,m);
 }
}
'''
# Bounded main: the frozen renderer/backend remains byte-for-byte above it.
main=s.index('int main(');prefix=s[:main]
s=prefix+'''int main(int argc,char**argv){if(argc!=5&&argc!=6)return 2;std::string app=argv[1];int w=atoi(argv[2]),h=atoi(argv[3]);std::string out=argv[4],inputPath=argc==6?argv[5]:"";nativeTrace=fopen((out+".trace").c_str(),"wb");GfxRenderer r;r.orientation=w<h?GfxRenderer::Portrait:GfxRenderer::LandscapeCounterClockwise;r.panelWidth=800;r.panelHeight=480;r.panelWidthBytes=100;memset(r.frame,255,sizeof(r.frame));toybox::ensureFonts(r);freeink::ui::GfxRendererTarget target(r,true);target.setFont(0,10);target.setFont(1,12);target.setFont(2,13);auto device=target.deviceContext();freeink::ui::InputSnapshot input{};toybox::Interactions interactions;toybox::Frame frame(target,device,input,interactions);toybox::Screen screen(frame,toybox::themeTokens());
'''+extra+'''FILE*f=fopen(out.c_str(),"wb");fprintf(f,"P5\\n%d %d\\n255\\n",w,h);for(int y=0;y<h;y++)for(int x=0;x<w;x++){int px,py;rotateCoordinates(r.orientation,x,y,&px,&py,r.panelWidth,r.panelHeight);fputc(r.frame[py*r.panelWidthBytes+px/8]&(128>>(px%8))?255:0,f);}fclose(f);fclose(nativeTrace);return 0;}'''

known=[f'three-{title}-{kind}'for title in ('connectfour','knucklebones','checkers')for kind in ('help1','help2','help3','map','menu-map')]
s=s.replace('std::string app=argv[1];int w=', 'std::string app=argv[1];bool known=false;for(const auto* id:{'+','.join('\"'+x+'\"'for x in known)+'})if(app==id)known=true;if(!known)return 11;int w=')
s=s.replace('int turn,status,picked,die,idle;fscanf(in,"%127s %d %d %d %d %d %127s",cells,&turn,&status,&picked,&die,&idle,history);fclose(in);','int turn,status,picked,die,idle,wins=0,losses=0,draws=0;char line[256]={};if(!fgets(line,256,in))return 21;sscanf(line,"%127s",cells);if(!fgets(line,256,in)||sscanf(line,"%d %d %d %d %d",&turn,&status,&picked,&die,&idle)!=5)return 22;if(fgets(line,256,in))sscanf(line,"%127s",history);if(fgets(line,256,in))sscanf(line,"%d %d %d",&wins,&losses,&draws);else{wins=status==1;losses=status==2;draws=status==3;}fclose(in);bool menu=app.find("-menu-map")!=std::string::npos;')
for title in ('connectfour','knucklebones','checkers'):
 s=s.replace(f'app=="three-{title}-map"',f'(app=="three-{title}-map"||app=="three-{title}-menu-map")')
s=s.replace('if(status){c4ui::ResultModel', 'if(menu){c4ui::MenuModel m{};m.hasHistory=true;m.lastCells=&g.cell[0][0];int line[4];connectfour::winningLine(g,g.turn,line);m.lastLine=line;m.lastOutcome=status==3?2:status-1;m.wins=wins;m.losses=losses;m.draws=draws;c4ui::buildMenu(screen,m);}else if(status){c4ui::ResultModel')
s=s.replace('if(status){knuckleui::ResultModel','if(menu){knuckleui::MenuModel r{};r.hasHistory=true;r.lastYours=m.yours;r.lastTheirs=m.theirs;r.wins=wins;r.losses=losses;r.draws=draws;knuckleui::buildMenu(screen,r);}else if(status){knuckleui::ResultModel')
s=s.replace('if(status){checkui::ResultModel','if(menu){checkui::MenuModel r{};r.hasHistory=true;r.lastCells=m.game.cell;r.lastOutcome=status==3?2:status-1;r.lastYourPieces=m.yourPieces;r.lastTheirPieces=m.theirPieces;r.wins=wins;r.losses=losses;r.draws=draws;checkui::buildMenu(screen,r);}else if(status){checkui::ResultModel')
s=s.replace('bool menu=app.find', 'if((strlen(cells)!=42&&strlen(cells)!=18&&strlen(cells)!=64)||turn<1||turn>2||status<0||status>3||picked<0||picked>64||die<0||die>6||idle<0||idle>255)return 23;bool menu=app.find')
f=a.output/'native_three.cpp';f.write_text(s)
cmd=json.loads((N/'compile-command.json').read_text());cmd=[str(f)if x==str(N/'native_catalog.cpp')else str(a.output/'native-three')if x==str(N/'native-catalog')else x for x in cmd]
cmd=[x for x in cmd if not ('src/apps_local/'in x and x.endswith('.cpp') and not any(t in x for t in ('/connectfour/','/knucklebones/','/checkers/','/link/','/player/')))]
(a.output/'compile-command.json').write_text(json.dumps(cmd,indent=2)+'\n')
r=subprocess.run(cmd,text=True,capture_output=True);(a.output/'compile.log').write_text(r.stdout+r.stderr);print(r.returncode,r.stderr[-2000:]);r.check_returncode()
(a.output/'provenance.json').write_text(json.dumps(dict(frozen_source_sha256=hashlib.sha256((N/'native_catalog.cpp').read_bytes()).hexdigest(),derived_source_sha256=hashlib.sha256(f.read_bytes()).hexdigest(),source_revision='d3a49ce8e85f9bd0d3e0c774a54adc267fd7547b',scope='Unmodified original Screens HowToModel page0/1/2; native backend frozen catalogue source copied with isolated fixture dispatcher only'),indent=2)+'\n')
