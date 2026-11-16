#!/usr/bin/env python3
"""Isolated fixture dispatcher over unchanged frozen native renderer backend."""
from pathlib import Path
import argparse,subprocess,json,hashlib
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
N=Path('/root/github/.host-data/crosssmudge/ported-ui-parity/native-catalog-references');base=(N/'native_catalog.cpp').read_text();prefix=base[:base.index('int main(')]
main=r'''
int main(int argc,char**argv){if(argc<5)return 2;std::string app=argv[1],out=argv[4];int w=atoi(argv[2]),h=atoi(argv[3]);nativeTrace=fopen((out+".trace").c_str(),"wb");GfxRenderer r;r.orientation=w<h?GfxRenderer::Portrait:GfxRenderer::LandscapeCounterClockwise;r.panelWidth=800;r.panelHeight=480;r.panelWidthBytes=100;memset(r.frame,255,sizeof(r.frame));toybox::ensureFonts(r);calc::ensureCalcFonts(r);freeink::ui::GfxRendererTarget target(r,true);target.setFont(0,10);target.setFont(1,12);target.setFont(2,13);auto device=target.deviceContext();freeink::ui::InputSnapshot input{};toybox::Interactions interactions;toybox::Frame frame(target,device,input,interactions);toybox::Screen screen(frame,toybox::themeTokens());
if(app=="sudoku-board"||app=="sudoku-pencil"||app=="sudoku-conflict"||app=="sudoku-result"||app=="sudoku-record"){
 FILE*in=fopen(argv[5],"rb");if(!in)return 20;char given[82]{},solution[82]{},value[82]{};if(fscanf(in,"%81s %81s %81s",given,solution,value)!=3)return 21;sudoku::Game g{};for(int i=0;i<81;i++){g.puzzle.given[i]=given[i]-'0';g.entry[i]=given[i]=='0'?value[i]-'0':0;unsigned n;if(fscanf(in,"%u",&n)!=1)return 22;g.note[i]=n;}int level,elapsed,hints,hardest,count,best,newBest,digit,history;if(fscanf(in,"%d%d%d%d%d%d%d%d%d",&level,&elapsed,&hints,&hardest,&count,&best,&newBest,&digit,&history)!=9)return 23;fclose(in);g.puzzle.level=static_cast<sudoku::Level>(level);g.armed=digit;g.undoCount=history;g.solvedFlag=app=="sudoku-result"||app=="sudoku-record";
 if(app=="sudoku-board"||app=="sudoku-pencil"||app=="sudoku-conflict"){sudokuui::BoardModel m{};m.game=g;sudokuui::buildBoard(screen,m);}
 else if(app=="sudoku-result"){sudokuui::ResultModel m{};m.game=g;m.level=g.puzzle.level;m.hardest=static_cast<sudoku::Technique>(hardest);m.elapsedMs=elapsed*1000;m.hintsUsed=hints;m.newBest=newBest;m.bestMs=best<0?0:best*1000;m.solvedAtThisLevel=count;for(auto d:g.puzzle.given)m.clues+=d!=0;sudokuui::buildResult(screen,m);}
 else{sudokuui::MenuModel m{};m.hasGame=true;m.selected=0;m.game=g;m.level=g.puzzle.level;m.record.solved[level]=count;m.record.bestMs[level]=best<0?0:best*1000;sudokuui::buildMenu(screen,m);}
}else if(app.rfind("sudoku-help",0)==0){sudokuui::HowToModel m{};m.page=app.back()-'1';sudokuui::buildHowTo(screen,m);}
else if(app=="notes-confirm"){notesui::ConfirmModel m{};m.title="NOTE 1";m.menuIcon=&icon_go_settings_32;m.prose="Remove all checked lines? The original import file stays unchanged.";notesui::buildConfirm(screen,m);}
else if(app=="notes"||app=="notes-note"||app=="notes-check"){
 if(app=="notes"){
  notesui::DeckItem rows[8]{};std::string names[8];for(int i=0;i<8;i++){names[i]="Note "+std::to_string(i+1);rows[i].title=names[i].c_str();}rows[0].done=1;rows[0].total=4;rows[0].tally="1/4";
  notesui::DeckModel m{};m.items=rows;m.count=8;m.pageLabel=w==480?"1 / 2":"1 / 4";notesui::buildDeck(screen,m);
 }else{
  notesui::Task rows[]={{"Shopping",false},{"Bread",app=="notes-check"},{"Milk",true},{"A readable paragraph.",false}};
  notesui::NoteModel m{};m.title="NOTE 1";m.menuIcon=&icon_go_settings_32;m.tasks=rows;m.count=4;m.done=app=="notes-check"?2:1;m.total=4;m.anyDone=true;m.pageLabel=w==800?"1 / 2":nullptr;notesui::buildNote(screen,m);
 }
}else if(app=="calculator-pending"||app=="calculator-result"){
 CalculatorActivity a(r);a.engine.press(calc::Key::D7);a.engine.press(calc::Key::Mul);if(app=="calculator-result"){a.engine.press(calc::Key::D4);a.engine.press(calc::Key::Equals);}a.drawChrome();auto g=calc::padGeometry(w,h);a.drawDisplay(g);a.drawPad(g);
}else return 11;
FILE*f=fopen(out.c_str(),"wb");fprintf(f,"P5\n%d %d\n255\n",w,h);for(int y=0;y<h;y++)for(int x=0;x<w;x++){int px,py;rotateCoordinates(r.orientation,x,y,&px,&py,r.panelWidth,r.panelHeight);fputc(r.frame[py*r.panelWidthBytes+px/8]&(128>>(px%8))?255:0,f);}fclose(f);fclose(nativeTrace);return 0;}
'''
f=a.output/'native_utility.cpp';f.write_text(prefix+main)
cmd=json.loads((N/'compile-command.json').read_text());cmd=[str(f)if x==str(N/'native_catalog.cpp')else str(a.output/'native-utility')if x==str(N/'native-catalog')else x for x in cmd];cmd=[x for x in cmd if not('src/apps_local/'in x and x.endswith('.cpp')and not any(t in x for t in('/sudoku/','/notes/','/link/','/player/')))]
(a.output/'compile-command.json').write_text(json.dumps(cmd,indent=2)+'\n');t=subprocess.run(cmd,capture_output=True,text=True);(a.output/'compile.log').write_text(t.stdout+t.stderr);print(t.returncode,t.stderr[-3000:]);t.check_returncode()
(a.output/'provenance.json').write_text(json.dumps(dict(frozen_backend_sha256=hashlib.sha256(prefix.encode()).hexdigest(),native_source_sha256=hashlib.sha256(f.read_bytes()).hexdigest(),source_revision='d3a49ce8e85f9bd0d3e0c774a54adc267fd7547b',actual_native_models=True),indent=2)+'\n')
for app in ['sudoku-help'+str(i)for i in range(1,6)]+['notes','notes-note','notes-check','notes-confirm','calculator-pending','calculator-result']:
 for w,h in[(480,800),(800,480)]:
  out=a.output/'captures'/app;out.mkdir(parents=True,exist_ok=True);subprocess.run([str(a.output/'native-utility'),app,str(w),str(h),str(out/f'{w}x{h}.pgm')],check=True)
