"""Read frozen native scaffold; compile original Screens + actual fonts in a private output.
Only Hearts state adapter is new; no original/native freeze is modified."""
from pathlib import Path
import argparse,json,subprocess,hashlib,re
p=argparse.ArgumentParser();p.add_argument('--output',required=True,type=Path);a=p.parse_args();O=a.output.resolve();O.mkdir(parents=True,exist_ok=True)
F=Path('/root/github/.host-data/crosssmudge/ported-ui-parity/native-catalog-references');s=(F/'native_catalog.cpp').read_text()
# State adapter translates saved old-model encodings, never re-deals a different deck.
helper=r'''
static uint8_t mapped(int c){return uint8_t((((c-1)/13)<<4)|(((c-1)%13+1)%13));}
static void loadHearts(hearts::Game&g,const std::string&path,std::string&selected){
 std::ifstream file(path,std::ios::binary);std::string raw((std::istreambuf_iterator<char>(file)),{});std::vector<std::string>v;size_t last=0;for(size_t i=0;i<=raw.size();i++)if(i==raw.size()||raw[i]=='|'){v.push_back(raw.substr(last,i-last));last=i+1;}if(v.size()!=15){fprintf(stderr,"invalid Hearts model\n");exit(12);}
 g.phase=static_cast<hearts::Phase>(stoi(v[1]));for(int c=1;c<=52;c++){int owner=v[2][c-1]-'0';if(owner>=1&&owner<=4)g.hands[owner-1].add(mapped(c));else if(owner>4)g.taken[owner-5]+=hearts::penaltyOf(mapped(c));}
 g.turn=static_cast<hearts::Seat>(stoi(v[3])-1);g.trick.leader=static_cast<hearts::Seat>(stoi(v[4])-1);g.trick.count=v[5].size();for(int i=0;i<(int)v[5].size();i++)g.trick.played[(stoi(v[4])+i-1)%4]=mapped(uint8_t(v[5][i]));g.heartsBroken=stoi(v[6]);g.trickNumber=stoi(v[7]);g.handNumber=stoi(v[8]);selected=v[9];for(int i=0;i<4;i++)g.total[i]=stoi(v[10+i]);
}
'''
# actual game field verified at generation time, fail closed if upstream changes
helper=helper.replace('v.size()!=15','v.size()!=14')
s=s.replace('int main(int argc',helper+'\nint main(int argc')
a0=s.index('if(app=="hearts-board")');b=s.index('\n',a0)
s=s[:a0]+r'''if(app=="hearts-board") {hearts::Game g{};std::string picked;loadHearts(g,inputPath,picked);heartsui::BoardModel m{};m.game=&g;const char*names[]={"YOU","WEST","NORTH","EAST"};for(int s=0;s<4;s++){m.seats[s].name=names[s];m.seats[s].initial=names[s][0];m.seats[s].total=g.total[s];m.seats[s].taken=g.taken[s];if(g.phase==hearts::Phase::TrickTaken&&s==int(g.trick.winner()))m.seats[s].taken+=g.trick.points();m.seats[s].cardsLeft=g.hands[s].count;m.seats[s].isMe=s==0;m.seats[s].isTurn=g.phase==hearts::Phase::Playing&&int(g.turn)==s;}
for(int i=0;i<g.hands[0].count;i++){m.legal[i]=g.phase==hearts::Phase::Passing||(g.turn==hearts::Seat::South&&hearts::isLegalPlay(g,hearts::Seat::South,g.hands[0].cards[i]));for(unsigned char c:picked)if(mapped(c)==g.hands[0].cards[i]){m.picked[i]=true;m.pickedCount++;}}
char status[128],sub[128];if(g.phase==hearts::Phase::Passing){snprintf(status,sizeof(status),"PICK THREE TO PASS %s",(const char*[]){"LEFT","RIGHT","ACROSS","NOBODY"}[g.handNumber%4]);snprintf(sub,sizeof(sub),"%d OF 3",m.pickedCount);}else{if(g.phase==hearts::Phase::TrickTaken){int s=int(g.trick.winner()),pts=g.trick.points();snprintf(status,sizeof(status),"%s %s",names[s],s==0?"TAKE IT":"TAKES IT");if(pts)snprintf(status+strlen(status),sizeof(status)-strlen(status),", %d POINT%s",pts,pts==1?"":"S");}else if(g.turn!=hearts::Seat::South)snprintf(status,sizeof(status),"%s IS THINKING",names[int(g.turn)]);else if(g.trick.empty())snprintf(status,sizeof(status),"%s",g.trickNumber==0?"LEAD THE TWO OF CLUBS":"YOUR LEAD");else snprintf(status,sizeof(status),"FOLLOW %s",(const char*[]){"CLUBS","DIAMONDS","SPADES","HEARTS"}[int(g.trick.ledSuit())]);snprintf(sub,sizeof(sub),"HEARTS %s   QUEEN %s",g.heartsBroken?"BROKEN":"UNBROKEN","UNSEEN");}m.status=status;m.subStatus=sub;m.showConfirm=g.phase==hearts::Phase::Passing;m.confirmEnabled=m.pickedCount==3;heartsui::Layout layout{};heartsui::buildBoard(screen,m,layout);}
''' +s[b:]
# Keep only the two owned titles; all source units compile fresh here.
branch=s[s.index('if(app=="hearts-board")'):s.index('\nif(',s.index('if(app=="hearts-board")')+5)]
branch=branch.replace('heartsui::buildBoard(screen,m,layout);}', '''if(int(g.phase)>=3){for(int s=0;s<4;s++)if(g.taken[s]==26){g.lastHand.moon=true;g.lastHand.shooter=static_cast<hearts::Seat>(s);}for(int s=0;s<4;s++){g.lastHand.taken[s]=g.taken[s];g.lastHand.scored[s]=g.lastHand.moon?(s==int(g.lastHand.shooter)?0:26):g.taken[s];g.lastHand.total[s]=g.total[s];}heartsui::ScoreModel score{};score.game=&g;score.gameOver=g.phase==hearts::Phase::GameOver;for(int s=0;s<4;s++)score.seats[s]=m.seats[s];heartsui::buildScore(screen,score);}else heartsui::buildBoard(screen,m,layout);}''')
prefix=s[:s.index('#include <FreeInkUIGfxRenderer.h>')]
includes='\n#include <FreeInkUIGfxRenderer.h>\n'
for f in ['hearts/HeartsScreens.h','jaipur/JaipurScreens.h','ui/ToyboxFonts.cpp']:includes+='#include "/root/github/crossplay/src/apps_local/'+f+'"\n'
main=r'''int main(int argc,char**argv){if(argc<5)return 2;std::string app=argv[1];int w=atoi(argv[2]),h=atoi(argv[3]);std::string out=argv[4],inputPath=argc>5?argv[5]:"";nativeTrace=fopen((out+".trace").c_str(),"wb");GfxRenderer r;r.orientation=w<h?GfxRenderer::Portrait:GfxRenderer::LandscapeCounterClockwise;memset(r.frame,255,sizeof(r.frame));toybox::ensureFonts(r);freeink::ui::GfxRendererTarget target(r,true);target.setFont(0,10);target.setFont(1,12);target.setFont(2,13);auto device=target.deviceContext();freeink::ui::InputSnapshot input{};toybox::Interactions interactions;toybox::Frame frame(target,device,input,interactions);auto tokens=toybox::themeTokens();if(app.rfind("hearts",0)==0)tokens.headerHeight=56;toybox::Screen screen(frame,tokens);
if(app=="hearts"){heartsui::MenuModel m{};heartsui::buildMenu(screen,m);}
if(app=="hearts-help"){heartsui::HowToModel m{};m.page=atoi(inputPath.c_str());heartsui::buildHowTo(screen,m);}
if(app=="jaipur-help"){jaipurui::TutorialModel m{};m.page=atoi(inputPath.c_str());jaipurui::buildTutorial(screen,m);}
'''
end=s[s.index('FILE*f=fopen(out.c_str()'):]
s=prefix+includes+helper+main+branch+end
(O/'native.cpp').write_text(s)
cmd=json.loads((F/'compile-command.json').read_text());cmd=[x for x in cmd if not x.endswith('.o') and (not x.endswith('.cpp') or any(k in x for k in ['/EpdFont/','/Utf8/','/FreeInkUI/','/hearts/','/jaipur/','/cards/CardArt','/link/LinkScreens','/player/','native_catalog.cpp']))]
cmd=[str(O/'native.cpp')if x==str(F/'native_catalog.cpp')else x for x in cmd];cmd[cmd.index('-o')+1]=str(O/'native')
# Verify every original compilation unit against the immutable owner source.
for x in cmd:
 if x.startswith('/root/github/crossplay/')and Path(x).is_file():
  rel=str(Path(x).relative_to('/root/github/crossplay'));cwd='/root/github/crossplay';rev='d3a49ce8e85f9bd0d3e0c774a54adc267fd7547b'
  if rel.startswith('freeink-sdk/'):
   rev=subprocess.check_output(['git','rev-parse',rev+':freeink-sdk'],cwd=cwd,text=True).strip();cwd+='/freeink-sdk';rel=rel[len('freeink-sdk/'):]
  expected=subprocess.check_output(['git','show',rev+':'+rel],cwd=cwd);assert Path(x).read_bytes()==expected,'modified original '+rel
(O/'source-sha256.json').write_text(json.dumps({x:hashlib.sha256(Path(x).read_bytes()).hexdigest()for x in cmd if Path(x).is_file()},indent=2))
r=subprocess.run(cmd,text=True,capture_output=True,timeout=180);(O/'compile.log').write_text(r.stdout+r.stderr);print(r.stderr[-6000:]);raise SystemExit(r.returncode)
