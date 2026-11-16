from pathlib import Path
import argparse,subprocess,json,hashlib
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();O=a.output.resolve();O.mkdir(parents=True,exist_ok=True);C=Path('/root/github/crossplay');F=Path('/root/github/.host-data/crosssmudge/ported-ui-parity/native-catalog-references');raw=(F/'native_catalog.cpp').read_text();s=raw[:raw.index('#include <FreeInkUIGfxRenderer.h>')]
s+='\n#include <FreeInkUIGfxRenderer.h>\n#include <sstream>\n'
for f in ['ui/ToyboxFonts.cpp','wavelength/WavelengthScreens.h','wavelength/WavelengthCore.h']:s+='#include "'+str(C/'src/apps_local'/f)+'"\n'
s+=r'''
int main(int argc,char**argv){if(argc<5)return 2;std::string view=argv[1];int w=atoi(argv[2]),h=atoi(argv[3]);std::string out=argv[4];nativeTrace=fopen((out+".trace").c_str(),"wb");GfxRenderer r;r.orientation=w<h?GfxRenderer::Portrait:GfxRenderer::LandscapeCounterClockwise;memset(r.frame,255,sizeof(r.frame));toybox::ensureFonts(r);freeink::ui::GfxRendererTarget target(r,true);target.setFont(0,11);target.setFont(1,13);target.setFont(2,15);auto device=target.deviceContext();freeink::ui::InputSnapshot input{};toybox::Interactions interactions;toybox::Frame frame(target,device,input,interactions);toybox::Screen screen(frame,toybox::themeTokens());
int round=1,total=0,scored=0,guess=10,secret=10,points=0,practice=1,showCall=0,right=0,revealed=0,ever=0,nudge=0,progress=0,only=0,abandoned=0;wavelength::Record rec{};std::string top="CHARMING",bottom="ANNOYING",top2="COLD",bottom2="HOT";
if(argc>5){std::ifstream f(argv[5]);f>>round>>total>>scored>>guess>>secret>>points>>practice>>showCall>>right>>revealed>>ever>>nudge>>progress>>only>>abandoned>>rec.rounds>>rec.points;for(int i=0;i<5;i++)f>>rec.buckets[i];f.ignore();std::getline(f,top);std::getline(f,bottom);std::getline(f,top2);std::getline(f,bottom2);}
wavelengthui::Spectrum pair{top.c_str(),bottom.c_str()};
if(view=="menu"){wavelengthui::MenuModel m{};m.record=&rec;m.sessionInProgress=progress;m.sessionRound=round;m.sessionTotal=total;m.sessionScored=scored;wavelengthui::renderMenu(screen,m);}
else if(view=="help")wavelengthui::renderHowTo(screen);
else if(view=="pass"){wavelengthui::PassModel m{};m.roundNumber=round;m.total=total;m.practice=practice;m.abandoned=abandoned>0;m.abandonedCount=abandoned;wavelengthui::renderPassLeft(screen,m);}
else if(view=="pick"){wavelengthui::PickModel m{};m.first=pair;m.second={top2.c_str(),bottom2.c_str()};m.roundNumber=round;m.practice=practice;m.onlyOne=only;wavelengthui::renderPick(screen,m);}
else if(view=="peek"){wavelengthui::PeekModel m{};m.spectrum=pair;m.target=secret;m.revealed=revealed;m.everRevealed=ever;m.nudgeHold=nudge;wavelengthui::renderPeek(screen,m);}
else if(view=="clue"){wavelengthui::ClueModel m{};m.spectrum=pair;m.roundNumber=round;m.practice=practice;wavelengthui::renderClue(screen,m);}
else if(view=="dial"){wavelengthui::DialModel m{};m.spectrum=pair;m.guess=guess;m.roundNumber=round;m.practice=practice;wavelengthui::renderDial(screen,m);}
else if(view=="call"){wavelengthui::CallModel m{};m.spectrum=pair;m.guess=guess;m.practice=practice;wavelengthui::renderCall(screen,m);}
else if(view=="reveal"){wavelengthui::RevealModel m{};m.spectrum=pair;m.guess=guess;m.target=secret;m.points=points;m.practice=practice;m.showCall=showCall;m.callWasRight=right;m.roundNumber=round;m.total=total;wavelengthui::renderReveal(screen,m);}
else if(view=="pause"){wavelengthui::PauseModel m{};m.roundNumber=round;m.total=total;m.practice=practice;m.abandoned=abandoned;wavelengthui::renderPause(screen,m);}
else if(view=="summary"){wavelengthui::SummaryModel m{};m.record=&rec;m.rounds=scored;m.total=total;m.averageTenths=scored?(total*10+scored/2)/scored:0;m.nextRound=round;m.abandoned=abandoned;wavelengthui::renderSummary(screen,m);}
else return 11;
FILE*f=fopen(out.c_str(),"wb");fprintf(f,"P5\n%d %d\n255\n",w,h);for(int y=0;y<h;y++)for(int x=0;x<w;x++){int px,py;rotateCoordinates(r.orientation,x,y,&px,&py,r.panelWidth,r.panelHeight);fputc(r.frame[py*r.panelWidthBytes+px/8]&(128>>(px%8))?255:0,f);}fclose(f);fclose(nativeTrace);return 0;}
'''
(O/'native.cpp').write_text(s);base=json.loads((F/'compile-command.json').read_text());flags=[x for x in base[1:]if not x.endswith('.cpp')and not x.endswith('.o')];flags=flags[:flags.index('-o')]
units=['lib/EpdFont/EpdFont.cpp','lib/EpdFont/EpdFontFamily.cpp','lib/Utf8/Utf8.cpp','freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp','src/apps_local/wavelength/WavelengthScreens.cpp','src/apps_local/wavelength/WavelengthCore.cpp']
for rel in units:
 cwd=C;rev='d3a49ce8e85f9bd0d3e0c774a54adc267fd7547b';path=rel
 if rel.startswith('freeink-sdk/'):rev=subprocess.check_output(['git','rev-parse',rev+':freeink-sdk'],cwd=C,text=True).strip();cwd=C/'freeink-sdk';path=rel[len('freeink-sdk/'):]
 assert(C/rel).read_bytes()==subprocess.check_output(['git','show',rev+':'+path],cwd=cwd)
cmd=['c++',*flags,str(O/'native.cpp'),*[str(C/x)for x in units],'-o',str(O/'native')];(O/'sources.json').write_text(json.dumps({x:hashlib.sha256((C/x).read_bytes()).hexdigest()for x in units},indent=2))
r=subprocess.run(cmd,text=True,capture_output=True,timeout=180);(O/'compile.log').write_text(r.stdout+r.stderr);print(r.stderr[-5000:]);raise SystemExit(r.returncode)
