"""Original Murdle builders/models against the pinned native renderer scaffold."""
from pathlib import Path
import argparse,hashlib,json,subprocess
p=argparse.ArgumentParser();p.add_argument('--reference',type=Path,required=True);p.add_argument('--native-source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();C=a.reference.resolve();S=a.native_source.resolve();O=a.output.resolve();O.mkdir(parents=True,exist_ok=True)
raw=(S/'native_catalog.cpp').read_bytes();assert hashlib.sha256(raw).hexdigest()=='7a9257a8c716b0a71b6c922367dcfcff442281a71497119d21140d2d27d699f4'
base=raw.decode().split('int main(int argc,char**argv)')[0]
base+=r'''
int main(int argc,char**argv){if(argc!=5)return 2;int w=atoi(argv[1]),h=atoi(argv[2]);std::ifstream data(argv[3]);int id,seed,phase,face,page,struck,nextTier,attempts,blocked,chosen[4];std::string mode,rawMarks;data>>id>>seed>>phase>>mode>>face>>page>>struck>>nextTier>>attempts;for(auto&v:chosen)data>>v;data>>blocked>>rawMarks;
 murdle::Puzzle p{};murdle::Scratch scratch{};auto tier=murdle::Tier((id-1)/16);auto shape=murdle::shapeOf(tier);uint8_t cast[4][4]{};if(!murdle::drawCast(seed,shape,cast)||!murdle::generate(tier,seed,cast,murdle::attrMasksFor(cast,shape),scratch,p))return 8;murdle::Marks marks{};marks.reset(shape);int offset=0;for(int a=0;a<shape.cats-1;a++)for(int b=a+1;b<shape.cats;b++)for(int r=0;r<shape.items;r++)for(int c=0;c<shape.items;c++)marks.enter(a,r,b,c,murdle::Mark(rawMarks[offset++]-'0'));
 GfxRenderer r;r.orientation=w<h?GfxRenderer::Portrait:GfxRenderer::LandscapeCounterClockwise;r.panelWidth=800;r.panelHeight=480;r.panelWidthBytes=100;memset(r.frame,255,sizeof(r.frame));toybox::ensureFonts(r);freeink::ui::GfxRendererTarget target(r,true);target.setFont(0,10);target.setFont(1,12);target.setFont(2,13);auto device=target.deviceContext();freeink::ui::InputSnapshot input{};toybox::Interactions interactions;toybox::Frame frame(target,device,input,interactions);toybox::Screen screen(frame);
 if(mode=="menu"){murdleui::MenuModel m{};m.hasCase=true;m.caseSolved=phase==1;m.tier=tier;m.caseNumber=id;m.puzzle=&p;m.marks=&marks;m.cluesTicked=__builtin_popcount(struck);murdleui::buildMenu(screen,m);}
 else if(mode=="case"){murdleui::CaseModel m{};m.puzzle=&p;m.marks=&marks;m.face=murdleui::Face(face-1);m.page=page-1;m.struck=struck;m.caseNumber=id;m.solved=phase==1;m.notice=blocked?murdletext::kBlockedNotice:nullptr;murdleui::buildCase(screen,m);}
 else if(mode=="settings"){murdleui::SettingsModel m{};m.tier=murdle::Tier(nextTier-1);m.caseOpen=true;murdleui::buildSettings(screen,m);}
 else if(mode=="confirm")murdleui::buildConfirmNew(screen);
 else if(mode=="help"){murdleui::HowToModel m{};murdleui::buildHowTo(screen,m);}
 else if(mode=="accuse"){murdleui::AccuseModel m{};m.puzzle=&p;for(int c=0;c<4;c++)m.picks[c]=chosen[c]<1?255:chosen[c]-1;murdleui::buildAccuse(screen,m);}
 else if(mode=="verdict"){murdleui::VerdictModel m{};m.puzzle=&p;m.right=phase==1;m.wrongAccusations=attempts-(phase==1?1:0);for(int c=0;c<4;c++)m.picks[c]=chosen[c]-1;murdleui::buildVerdict(screen,m);}
 else return 9;
 FILE*f=fopen(argv[4],"wb");fprintf(f,"P5\n%d %d\n255\n",w,h);for(int y=0;y<h;y++)for(int x=0;x<w;x++){int px,py;rotateCoordinates(r.orientation,x,y,&px,&py,r.panelWidth,r.panelHeight);fputc(r.frame[py*r.panelWidthBytes+px/8]&(128>>(px%8))?255:0,f);}fclose(f);return 0;}
'''
(O/'native_murdle.cpp').write_text(base)
paths=['src/apps_local/murdle/'+name+ext for name in ['MurdleScreens','MurdleCore','MurdleText','MurdleCast']for ext in ['.h','.cpp']]
for path in paths:assert (C/path).read_bytes()==subprocess.check_output(['git','show','d3a49ce8e85f9bd0d3e0c774a54adc267fd7547b:'+path],cwd=C)
old=json.loads((S/'compile-command.json').read_text());command=old[:old.index('-o')];command=[str(O/'native_murdle.cpp')if x==str(S/'native_catalog.cpp')else x for x in command];command=[x for x in command if not('/src/apps_local/'in x)or '/murdle/'in x];command+=['-o',str(O/'native-murdle')];subprocess.run(command,check=True)
(O/'inputs.json').write_text(json.dumps({'scaffold_sha256':hashlib.sha256(raw).hexdigest(),'original':{path:hashlib.sha256((C/path).read_bytes()).hexdigest()for path in paths}},indent=2)+'\n')
