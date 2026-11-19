#!/usr/bin/env python3
"""Actual unified compressed-image renderer orchestration; codec doubles."""
from pathlib import Path
import argparse, os, subprocess, tempfile, json
R=Path(__file__).resolve().parents[3]
p=argparse.ArgumentParser();p.add_argument('--evidence',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();a.evidence.mkdir(parents=True,exist_ok=True)
t=(R/'src/activities/util/BmpViewerActivity.cpp').read_text();start=t.index('bool BmpViewerActivity::renderCompressedImage(');body=t[start:t.index('\n}',start)+2]
s=r'''
#include <string>
#include <vector>
#include <algorithm>
#include <cassert>
#include <cstdio>
struct ImageDimensions {int width=1600,height=900;};struct RenderConfig {int x=0,y=0,maxWidth=0,maxHeight=0;bool useGrayscale=false,useDithering=false,performanceMode=false,useExactDimensions=false;};
namespace HalDisplay {constexpr int FAST_REFRESH=0;}
constexpr int UI_10_FONT_ID=1;constexpr int STR_BACK=0,STR_SET_SLEEP_COVER=1;const char* tr(int id){return id?"Sleep cover":"Back";}
struct GfxRenderer {int width=480,height=800,painted=0,polarity=0;int getScreenWidth()const{return width;}int getScreenHeight()const{return height;}void clearScreen(){}void displayBuffer(int){++painted;}void preserveImagePolarity(int,int,int,int){++polarity;}};
namespace FsHelpers {bool hasJpgExtension(const std::string& p){return p.ends_with(".jpg")||p.ends_with(".jpeg");}}
bool dimensionsOk=true,decodeOk=true;int jpegCalls=0,pngCalls=0,errors=0;RenderConfig actual;
struct JpegToFramebufferConverter {static bool getDimensionsStatic(const std::string&,ImageDimensions&){return dimensionsOk;}bool decodeToFramebuffer(const std::string&,GfxRenderer&,const RenderConfig& c){actual=c;++jpegCalls;return decodeOk;}};
struct PngToFramebufferConverter {static bool getDimensionsStatic(const std::string&,ImageDimensions&){return dimensionsOk;}bool decodeToFramebuffer(const std::string&,GfxRenderer&,const RenderConfig& c){actual=c;++pngCalls;return decodeOk;}};
struct Input {struct Labels {const char* btn1;const char* btn2;const char* btn3;const char* btn4;};const char* withBackArrow(const char* s){return s;}Labels mapLabels(const char* a,const char* b,const char* c,const char* d){return {a,b,c,d};}};
std::string confirmLabel;struct {void drawButtonHints(GfxRenderer&,const char*,const char* b,const char*,const char*){confirmLabel=b;}} GUI;
void drawImageError(GfxRenderer&,Input&,const char*){++errors;}
struct BmpViewerActivity {GfxRenderer renderer;Input mappedInput;std::string filePath="/image.jpg";std::vector<std::string> siblingImages;int currentImageIndex=-1;bool renderCompressedImage();};
'''+body+r'''
int main(){BmpViewerActivity a;for(auto dims:{std::pair{480,800},std::pair{800,480}}){a.renderer.width=dims.first;a.renderer.height=dims.second;for(auto ext:{".jpg",".jpeg",".png"}){a.filePath=std::string("/image")+ext;assert(a.renderCompressedImage());assert(actual.maxWidth<=dims.first&&actual.maxHeight<=dims.second&&actual.useExactDimensions);if(ext!=std::string(".png"))assert(confirmLabel.empty());else assert(!confirmLabel.empty());}}assert(jpegCalls==4&&pngCalls==2);dimensionsOk=false;assert(!a.renderCompressedImage());dimensionsOk=true;decodeOk=false;assert(!a.renderCompressedImage());assert(errors==2&&a.renderer.painted==6);puts("PASS actual JPEG/JPEG-long-extension/PNG decoder dispatch, both orientation fits, no unsupported JPEG sleep action, dimension/decode failures");}
'''
with tempfile.TemporaryDirectory(prefix='routes-image-',dir=os.environ['TMPDIR']) as d:
 d=Path(d);(d/'image.cpp').write_text(s);cmd=['g++','-std=c++20','-Wall','-Wextra','-Werror','-O1','-g']
 if a.sanitize:cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
 cmd += [str(d/'image.cpp'),'-o',str(d/'image')];p=subprocess.run(cmd,capture_output=True,text=True,timeout=60);log=p.stdout+p.stderr;rc=p.returncode
 if not rc:
  p=subprocess.run([str(d/'image')],capture_output=True,text=True,timeout=15);log+=p.stdout+p.stderr;rc=p.returncode
 (a.evidence/'image.log').write_text('COMMAND '+repr(cmd)+'\n'+log+'\nEXIT='+str(rc)+'\n');(a.evidence/'image-results.json').write_text(json.dumps({'rc':rc,'passes':[l for l in log.splitlines() if l.startswith('PASS ')]},indent=2)+'\n');print(log);raise SystemExit(rc)
