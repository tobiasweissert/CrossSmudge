"""Original Forehead builders over mapped actual port fields; no game simulation."""
from pathlib import Path
import argparse,hashlib,json,subprocess
p=argparse.ArgumentParser();p.add_argument('--reference',type=Path,required=True);p.add_argument('--native-source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();C=a.reference.resolve();S=a.native_source.resolve();O=a.output.resolve();O.mkdir(parents=True,exist_ok=True)
raw=(S/'native_catalog.cpp').read_bytes();assert hashlib.sha256(raw).hexdigest()=='7a9257a8c716b0a71b6c922367dcfcff442281a71497119d21140d2d27d699f4'
base=raw.decode().split('int main(int argc,char**argv)')[0]
header='#include "'+str(C/'src/apps_local/forehead/ForeheadScreens.h')+'"'
assert header in base
# Test-only visibility lets Round expose its exact real fields to this mapping.
# No source method or game rule is replaced; accepted Lua history is translated
# 1-based->0-based, then original textAt/markAt/cards accessors render it.
base=base.replace(header,'#define private public\n'+header+'\n#undef private')
base+=r'''
int main(int argc,char**argv){if(argc!=5)return 2;int w=atoi(argv[1]),h=atoi(argv[2]);std::ifstream data(argv[3]);int category,length,left,score,phase,page,confirm,rounds,total,best,played;std::string view,word,bestIn,recent,history,seen;data>>category>>length>>left>>score>>phase>>page>>confirm>>rounds>>total>>best>>played>>view;data.ignore();std::getline(data,word);data>>bestIn>>recent>>history>>seen;
 GfxRenderer r;r.orientation=w<h?GfxRenderer::Portrait:GfxRenderer::LandscapeCounterClockwise;r.panelWidth=800;r.panelHeight=480;r.panelWidthBytes=100;memset(r.frame,255,sizeof(r.frame));toybox::ensureFonts(r);freeink::ui::GfxRendererTarget target(r,true);
 target.setFont(0,11);target.setFont(1,12);target.setFont(2,13);if(phase==1||phase==2){target.setFont(0,14);target.setFont(1,13);target.setFont(2,15);}else if(phase==3){target.setFont(0,11);target.setFont(1,15);target.setFont(2,13);}
 auto device=target.deviceContext();freeink::ui::InputSnapshot input{};toybox::Interactions interactions;toybox::Frame frame(target,device,input,interactions);toybox::Screen screen(frame);
 forehead::Record record{};record.rounds=rounds;record.words=total;record.best=best;record.played=played;for(int i=0;i<17;i++){record.bestIn[i]=atoi(bestIn.substr(i*3,3).c_str());if(record.bestIn[i])record.played|=1u<<i;}if(recent!="-"){record.recentCount=recent.size()/3;for(int i=0;i<record.recentCount;i++)record.recent[i]=atoi(recent.substr(i*3,3).c_str());}
 if(phase==0){if(view=="picker"){foreheadui::PickerModel m{};m.current=category-1;m.page=page-1;m.record=&record;foreheadui::buildPicker(screen,m);}else if(view=="settings"){foreheadui::SettingsModel m{};m.roundSeconds=length;m.confirmingReset=confirm;m.anythingToClear=rounds>0||category!=1||length!=60||seen.find_first_not_of('0')!=std::string::npos;foreheadui::buildSettings(screen,m);}else if(view=="help"){foreheadui::HowToModel m{};m.page=page-1;foreheadui::buildHowTo(screen,m);}else{foreheadui::MenuModel m{};m.category=category-1;m.roundSeconds=length;m.record=&record;foreheadui::buildMenu(screen,m);}}
 else if(phase==2){foreheadui::ReadyModel m{};m.category=category-1;m.roundSeconds=length;foreheadui::buildReady(screen,m);}
 else if(phase==1){foreheadui::PlayModel m{};m.word=word.c_str();m.category=category-1;m.score=score;m.secondsLeft=(left+999)/1000;m.lengthSeconds=length;foreheadui::buildPlay(screen,m);}
 else {forehead::Round round{};round.category_=category-1;round.lengthSeconds_=length;round.got_=score;round.count_=history=="-"?0:history.size()/5;for(int i=0;i<round.count_;i++){round.entries_[i]=std::stoi(history.substr(i*5,4),nullptr,16)-1;round.marks_[i]=forehead::Mark(history[i*5+4]-'1');}foreheadui::ResultModel m{};m.category=category-1;m.score=score;m.page=page-1;m.round=&round;foreheadui::buildResult(screen,m);}
 FILE*f=fopen(argv[4],"wb");fprintf(f,"P5\n%d %d\n255\n",w,h);for(int y=0;y<h;y++)for(int x=0;x<w;x++){int px,py;rotateCoordinates(r.orientation,x,y,&px,&py,r.panelWidth,r.panelHeight);fputc(r.frame[py*r.panelWidthBytes+px/8]&(128>>(px%8))?255:0,f);}fclose(f);return 0;}
'''
(O/'native_forehead.cpp').write_text(base)
paths=['src/apps_local/forehead/ForeheadScreens.cpp','src/apps_local/forehead/ForeheadCore.cpp']
for path in paths:assert(C/path).read_bytes()==subprocess.check_output(['git','show','d3a49ce8e85f9bd0d3e0c774a54adc267fd7547b:'+path],cwd=C)
cmd=json.loads((S/'compile-command.json').read_text());cmd=[str(O/'native_forehead.cpp')if x==str(S/'native_catalog.cpp')else str(O/'native-forehead')if x==str(S/'native-catalog')else x for x in cmd if not x.endswith('.cpp')or x==str(S/'native_catalog.cpp')or x in [str(C/y)for y in ['lib/EpdFont/EpdFont.cpp','lib/EpdFont/EpdFontFamily.cpp','lib/Utf8/Utf8.cpp','freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp']]];cmd[-2:-2]=[str(C/path)for path in paths]
subprocess.run(cmd,check=True);(O/'forehead-inputs.json').write_text(json.dumps({path:hashlib.sha256((C/path).read_bytes()).hexdigest()for path in paths},indent=2)+'\n')
