#include "PresentationLayout.h"
#include "services/BrowserCore.h"
#include <cassert>
#include <iostream>
#include <memory>
using namespace sdapps::presentation;
struct Target: ui::DrawTarget {
 ui::Size measureText(ui::FontId,const char* s,ui::TextStyle) const override {int n=0;for(;*s;++s)if((uint8_t(*s)&0xc0)!=0x80)++n;return {int16_t(n*8),24};}
 int16_t lineHeight(ui::FontId)const override{return 24;}
 void fill(ui::Rect,ui::Paint,uint8_t,uint8_t)override{}
 void stroke(ui::Rect,ui::Paint,uint8_t,uint8_t,uint8_t)override{}
 void line(ui::Point,ui::Point,uint8_t,ui::Paint)override{}
 void triangle(ui::Point,ui::Point,ui::Point,ui::Paint)override{}
 void text(ui::Rect r,const char* s,ui::TextStyle style)override{assert(measureText(style.font,s,style).width<=r.width);}
 void bitmap(ui::Rect,ui::BitmapRef,ui::BitmapMode,ui::Paint,ui::Rotation)override{}
};
int main(){
 Target t;auto m=std::make_unique<Model>();
 std::string content;for(int i=0;i<3000;++i)content+="word café 日本語 repeated link text\n\n";
 memcpy(m->text,content.c_str(),content.size()+1);m->length=content.size();
 for(bool chat:{false,true})for(auto size:{ui::Size{480,800},ui::Size{800,480}})for(int inset:{0,8,24}){
  auto g=geometry(rect(inset,inset,size.width-2*inset,size.height-2*inset),chat,24);
  assert(g.body.height>0 && g.body.bottom()<g.footer.y);
  if(chat)assert(g.composer.bottom()<=size.height-inset);
  m->dirty=true;paginate(*m,g,t,{});assert(m->pages[m->pageCount]==m->length);
  uint32_t covered=0;
  for(uint32_t p=0;p<m->pageCount;++p){m->page=p;assert(m->pages[p]==covered);
   int y=0;while(covered<m->pages[p+1]){auto l=nextLine(*m,covered,g.body.width,t,{});assert(l.next>covered);covered=l.next;y+=32;}
   assert(y<=g.body.height);drawBody(*m,g,t,{},[](ui::Rect,int32_t){});
  }assert(covered==content.size());
 }
 auto parsed=stickyweb::parseHtml("<p>same <a href='/a'>same &amp; <b>nested</b></a> <a href='/b'>same</a><a href='/a'>same</a></p>","https://example.com/");
 assert(parsed.ok && parsed.document.spans.size()==3 && parsed.document.links.size()==2);
 const auto& d=parsed.document;assert(d.text.substr(d.spans[0].start,d.spans[0].end-d.spans[0].start)=="same & nested");
 assert(d.spans[0].link==d.spans[2].link && d.spans[0].link!=d.spans[1].link);
 std::cout<<"geometry, full byte coverage, UTF-8 and occurrence spans passed\n";
}
