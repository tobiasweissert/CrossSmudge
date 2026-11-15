// Production screen/layout + real EpdFont glyph raster harness. Hardware,
// GfxRenderer fallback, bidi, rotation and clipping are NOT emulated.
#include "PresentationScreen.h"
#include <EpdFont.h>
#include <Utf8.h>
#include "builtinFonts/ubuntu_12_regular.h"
#include "builtinFonts/ubuntu_12_bold.h"
#include <fstream>
#include <memory>
#include <vector>
#include <cmath>
#include <cassert>
using namespace sdapps::presentation;
struct Raster:ui::DrawTarget {
 int w,h;std::vector<unsigned char> pixels;
 std::vector<std::pair<std::string,ui::Rect>> drawn;
 Raster(int width,int height):w(width),h(height),pixels(w*h,255){}
 EpdFont font(ui::TextStyle style)const{return EpdFont(style.bold?&ubuntu_12_bold:&ubuntu_12_regular);}
 ui::Size measureText(ui::FontId,const char* s,ui::TextStyle style)const override{int w=0,h=0;font(style).getTextDimensions(s,&w,&h);return {int16_t(w),int16_t(h)};}
 int16_t lineHeight(ui::FontId)const override{return ubuntu_12_regular.advanceY;}
 void pixel(int x,int y,ui::Color c){if(x>=0 && x<w && y>=0 && y<h)pixels[y*w+x]=c==ui::Color::Black?0:255;}
 void fill(ui::Rect r,ui::Paint p,uint8_t radius=0,uint8_t corners=255)override{
  for(int y=r.y;y<r.bottom();++y)for(int x=r.x;x<r.right();++x){int dx=std::max({r.x+int(radius)-x,0,x-(r.right()-int(radius)-1)}),dy=std::max({r.y+int(radius)-y,0,y-(r.bottom()-int(radius)-1)});int corner=y<r.y+r.height/2?(x<r.x+r.width/2?1:2):(x<r.x+r.width/2?8:4);if(!radius || !(corners&corner) || dx*dx+dy*dy<=radius*radius)pixel(x,y,p.color);}
 }
 void stroke(ui::Rect r,ui::Paint p,uint8_t width,uint8_t radius=0,uint8_t=255)override{
  fill(r,p,radius);fill(rect(r.x+width,r.y+width,r.width-2*width,r.height-2*width),ui::Paint::solid(p.color==ui::Color::Black?ui::Color::White:ui::Color::Black),radius>width?radius-width:0);
 }
 void line(ui::Point a,ui::Point b,uint8_t width,ui::Paint p)override{int n=std::max(std::abs(b.x-a.x),std::abs(b.y-a.y));for(int i=0;i<=n;++i)fill(rect(a.x+(b.x-a.x)*i/std::max(1,n),a.y+(b.y-a.y)*i/std::max(1,n),width,width),p);}
 void triangle(ui::Point,ui::Point,ui::Point,ui::Paint)override{}
 void bitmap(ui::Rect,ui::BitmapRef,ui::BitmapMode,ui::Paint,ui::Rotation)override{}
 void text(ui::Rect r,const char* s,ui::TextStyle style)override{
  drawn.push_back({s,r});
  auto f=font(style);int x=r.x,y=r.y+f.data->ascender;int prevAdvance=0;uint32_t prev=0;
  if(style.align==ui::TextAlign::Center)x+=(r.width-measureText(style.font,s,style).width)/2;
  while(*s){const auto* bytes=reinterpret_cast<const unsigned char*>(s);uint32_t cp=utf8NextCodepoint(&bytes);s=reinterpret_cast<const char*>(bytes);cp=f.applyLigatures(cp,s);const auto* glyph=f.getGlyph(cp);if(!glyph)continue;
   if(prev)x+=fp4::toPixel(prevAdvance+f.getKerning(prev,cp));
   for(int j=0;j<glyph->height;++j)for(int i=0;i<glyph->width;++i){int bit=j*glyph->width+i;if(f.data->bitmap[glyph->dataOffset+(bit>>3)]&(0x80>>(bit&7))){int px=x+glyph->left+i,py=y-glyph->top+j;if(px<r.right() && py<r.bottom())pixel(px,py,style.color);}}
   prev=cp;prevAdvance=glyph->advanceX;
  }
 }
 void save(const char* path){std::ofstream f(path,std::ios::binary);f<<"P5\n"<<w<<" "<<h<<"\n255\n";f.write(reinterpret_cast<char*>(pixels.data()),pixels.size());}
};
int main(int argc,char** argv){
 assert(argc==2);auto model=std::make_unique<Model>();auto& m=*model;
 ui::ThemeTokens theme;theme.bodyText.font=1;theme.smallText.font=0;
 for(bool chat:{false,true})for(bool landscape:{false,true}){
  Raster t(landscape?800:480,landscape?480:800);m={};m.binding.kind=chat?CP_CHAT_V2:CP_BROWSER_V2;
  strcpy(m.title,"Oven temperature");strcpy(m.address,"https://en.wikipedia.org/wiki/Electronic_paper");
  if(chat){const char* user="Convert 350 °F to Celsius for the oven\n";strcpy(m.text,user);m.runCount=1;m.runs[0]={0,uint32_t(strlen(user)),CP_USER};strcat(m.text,"\n350 °F is about 177 °C.\n\nIn a fan oven, set it roughly 20 °C lower, so about 160 °C.\n\nMost dials move in steps of 10: use 180 °C conventional or 160 °C fan.");}
  else{strcpy(m.text,"Electronic paper\n\nElectronic paper, also called e-paper or electronic ink, is a display technology that mimics the look of ordinary ink on paper.\n\nUnlike backlit flat-panel displays that emit light, an electronic paper display reflects ambient light, like paper. Many types hold static text and images without electricity.");m.runs[0]={0,18,2};m.runCount=1;const char* at=strstr(m.text,"display technology");m.spans[0]={uint32_t(at-m.text),uint32_t(at-m.text+18),1000};m.spanCount=1;}
  m.length=strlen(m.text);m.dirty=true;
  std::vector<ui::Rect> controls;
  drawScreen(m,t,theme,rect(0,0,t.w,t.h),"Ask anything","Send",[&](ui::Rect r,Control){controls.push_back(r);});
  assert(controls.size()<=24);std::string name=std::string(argv[1])+"/"+(chat?"chat":"browser")+(landscape?"-landscape":"-portrait")+".pgm";t.save(name.c_str());
  if(chat){
   strcpy(m.draft,"Reviewed transcript, ready to send");strcpy(m.status,"Review transcription, then press Send explicitly");
   Raster draft(t.w,t.h);drawScreen(m,draft,theme,rect(0,0,t.w,t.h),"Ask anything","Send",[](ui::Rect,Control){});draft.save((name+"-draft.pgm").c_str());
   for(int i=0;i<100;++i)strcat(m.text,"\n\nA long assistant reply remains reachable through bounded pages, with the composer fixed below the text.");
   m.length=strlen(m.text);m.dirty=true;
   Raster first(t.w,t.h);drawScreen(m,first,theme,rect(0,0,t.w,t.h),"Ask anything","Send",[](ui::Rect,Control){});first.save((name+"-long-first.pgm").c_str());
   m.page=m.pageCount-1;Raster last(t.w,t.h);drawScreen(m,last,theme,rect(0,0,t.w,t.h),"Ask anything","Send",[](ui::Rect,Control){});last.save((name+"-long-last.pgm").c_str());
  }else{
   m.binding.busy=1;strcpy(m.status,"Loading - Back cancels");Raster loading(t.w,t.h);drawScreen(m,loading,theme,rect(0,0,t.w,t.h),"Ask anything","Send",[](ui::Rect,Control){});loading.save((name+"-loading.pgm").c_str());
   m.binding.busy=0;strcpy(m.status,"Download failed - previous page preserved");Raster error(t.w,t.h);drawScreen(m,error,theme,rect(0,0,t.w,t.h),"Ask anything","Send",[](ui::Rect,Control){});error.save((name+"-error.pgm").c_str());
  }
 }
 return 0;
}
