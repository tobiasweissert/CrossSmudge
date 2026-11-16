"""Compile mapped social screens against immutable original renderer/font scaffold."""
from pathlib import Path
import argparse,hashlib,json,subprocess
p=argparse.ArgumentParser();p.add_argument('--reference',type=Path,required=True);p.add_argument('--native-source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();C=a.reference.resolve();S=a.native_source.resolve();O=a.output.resolve();O.mkdir(parents=True,exist_ok=True)
raw=(S/'native_catalog.cpp').read_bytes();assert hashlib.sha256(raw).hexdigest()=='7a9257a8c716b0a71b6c922367dcfcff442281a71497119d21140d2d27d699f4'
base=raw.decode().split('int main(int argc,char**argv)')[0]
base+=r'''
int main(int argc,char**argv){if(argc!=5)return 2;int w=atoi(argv[1]),h=atoi(argv[2]);std::ifstream data(argv[3]);int players,phase,seat,left,rounds,won,lost,timeout,result,chosen,help;data>>players>>phase>>seat>>left>>rounds>>won>>lost>>timeout>>result>>chosen>>help;std::string roles,word,recent;data>>roles;data.ignore();std::getline(data,word);data>>recent;
 GfxRenderer r;r.orientation=w<h?GfxRenderer::Portrait:GfxRenderer::LandscapeCounterClockwise;r.panelWidth=800;r.panelHeight=480;r.panelWidthBytes=100;memset(r.frame,255,sizeof(r.frame));toybox::ensureFonts(r);freeink::ui::GfxRendererTarget target(r,true);target.setFont(0,10);target.setFont(1,12);target.setFont(2,13);auto device=target.deviceContext();freeink::ui::InputSnapshot input{};toybox::Interactions interactions;toybox::Frame frame(target,device,input,interactions);toybox::Screen screen(frame);
 int master=roles.find('M');int insider=roles.find('I');if(insider==int(std::string::npos))insider=-1;
 if(help){insiderui::TutorialModel m{};m.page=help-1;insiderui::buildTutorial(screen,m);}
 else if(phase==0){insider::Record record{};if(recent!="-")for(char c:recent)record.push(insider::Outcome(c-'0'));record.rounds=rounds;record.won=won;record.lost=lost;record.outOfTime=timeout;insiderui::MenuModel m{};m.players=players;m.record=&record;insiderui::buildMenu(screen,m);}
 else if(phase==1||phase==2){insiderui::PassModel m{};m.players=players;m.seat=seat-1;m.revealed=phase==2;m.lastSeat=seat==players;m.role=roles[seat-1]=='M'?insider::Role::Master:roles[seat-1]=='I'?insider::Role::Insider:insider::Role::Citizen;m.word=word.c_str();insiderui::buildPass(screen,m);}
 else if(phase==3||phase==4){insiderui::QuestionsModel m{};m.players=players;m.masterSeat=master;m.secondsLeft=(left+4999)/5000*5;insiderui::buildQuestions(screen,m);}
 else if(phase==5){insiderui::VoteModel m{};m.players=players;m.masterSeat=master;m.chosen=chosen<0?-2:chosen==0?-1:chosen-1;insiderui::buildVote(screen,m);}
 else {insiderui::RevealModel m{};m.players=players;m.insiderSeat=insider;m.accused=chosen==0?-1:chosen-1;m.outcome=insider::Outcome(result);m.word=word.c_str();insiderui::buildReveal(screen,m);}
 FILE*f=fopen(argv[4],"wb");fprintf(f,"P5\n%d %d\n255\n",w,h);for(int y=0;y<h;y++)for(int x=0;x<w;x++){int px,py;rotateCoordinates(r.orientation,x,y,&px,&py,r.panelWidth,r.panelHeight);fputc(r.frame[py*r.panelWidthBytes+px/8]&(128>>(px%8))?255:0,f);}fclose(f);return 0;}
'''
(O/'native_social.cpp').write_text(base)
paths=['src/apps_local/insider/InsiderScreens.cpp','src/apps_local/insider/InsiderCore.cpp','src/apps_local/insider/InsiderScreens.h','src/apps_local/insider/InsiderArt.h']
for path in paths:assert (C/path).read_bytes()==subprocess.check_output(['git','show','d3a49ce8e85f9bd0d3e0c774a54adc267fd7547b:'+path],cwd=C)
old=json.loads((S/'compile-command.json').read_text());end=old.index('-o');command=old[:end];command=[str(O/'native_social.cpp')if x==str(S/'native_catalog.cpp')else x for x in command];command=[x for x in command if not('/src/apps_local/'in x)or x.endswith(('/insider/InsiderScreens.cpp','/insider/InsiderCore.cpp'))];command += ['-o',str(O/'native-social')];subprocess.run(command,check=True)
(O/'inputs.json').write_text(json.dumps({'frozen_scaffold':hashlib.sha256(raw).hexdigest(),'original':{path:hashlib.sha256((C/path).read_bytes()).hexdigest()for path in paths}},indent=2)+'\n')
