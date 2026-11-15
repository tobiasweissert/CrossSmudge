#pragma once
#include "PresentationLayout.h"
#include <cstdio>
namespace sdapps::presentation {
namespace fui=freeink::ui;
template<class Hit>
void drawScreen(Model& d,ui::DrawTarget& target,const ui::ThemeTokens& theme,ui::Rect safe,const char* askLabel,const char* sendLabel,Hit hit) {
  const bool chat=d.binding.kind==CP_CHAT_V2;
  auto style=theme.bodyText;style.maxLines=1;
  const auto g=geometry(rect(safe.x,safe.y,safe.width,safe.height),chat,target.lineHeight(style.font),target.lineHeight(theme.smallText.font)+4);
  if(d.titleFont!=theme.titleText.font){d.titleFont=theme.titleText.font;d.dirty=true;}
  paginate(d,g,target,style);d.hitCount=0;
  const auto black=fui::Paint::solid(fui::Color::Black);
  const auto label=[&](fui::Rect bounds,const char* text){target.text(bounds,text,style);};
  target.line({g.header.x,g.header.bottom()},{g.header.right(),g.header.bottom()},1,black);
  const auto menu=[&](fui::Rect bounds){int x=bounds.x+12,y=bounds.y+bounds.height/2-8;
    for(int i=0;i<3;++i)target.line({int16_t(x),int16_t(y+i*8)},{int16_t(x+24),int16_t(y+i*8)},2,black);};
  if(chat){
    menu(g.left);hit(g.left,Control::Menu);hit(g.right,Control::NewChat);
    auto titleStyle=style;titleStyle.bold=true;
    target.text(rect(g.center.x,g.center.y,g.center.width,g.center.height/2),d.title,titleStyle);
    target.text(rect(g.center.x,g.center.y+g.center.height/2,g.center.width,g.center.height/2),"ChatGPT",theme.smallText);
    target.stroke(rect(g.right.x+12,g.right.y+16,24,24),black,2,2);
    target.line({int16_t(g.right.x+18),int16_t(g.right.y+32)},{int16_t(g.right.x+36),int16_t(g.right.y+14)},3,black);
  }else{
    int x=g.left.x+25,y=g.left.y+g.left.height/2;
    target.line({int16_t(x+5),int16_t(y-10)},{int16_t(x-5),int16_t(y)},2,black);
    target.line({int16_t(x-5),int16_t(y)},{int16_t(x+5),int16_t(y+10)},2,black);hit(g.left,Control::Back);
    target.stroke(g.center,black,2,20);hit(g.center,Control::Address);menu(g.right);hit(g.right,Control::Menu);
    auto& host=d.scratch;const char* begin=strstr(d.address,"://");begin=begin?begin+3:d.address;
    size_t n=0;while(begin[n] && begin[n]!='/' && begin[n]!='?' && begin[n]!='#' && n<sizeof(host)-1)++n;
    memcpy(host,begin,n);host[n]=0;label(rect(g.center.x+12,g.center.y+8,g.center.width-24,g.center.height-16),host);
  }
  if(d.binding.busy && !chat){label(g.body,d.status);}
  else{
    drawBody(d,g,target,style,[&](fui::Rect bounds,int32_t action){
      if(d.hitCount<4096)d.hits[d.hitCount++]={bounds,action};
    });
    hit(g.body,Control::Body);
  }
  char page[32];snprintf(page,sizeof(page),"%u / %u",unsigned(d.page+1),unsigned(d.pageCount));
  auto small=theme.smallText;small.align=fui::TextAlign::Center;
  target.text(g.page,page,small);
  target.text(g.status,d.status,theme.smallText);
  if(chat){
    target.line({g.footer.x,g.footer.y},{g.footer.right(),g.footer.y},1,black);
    target.stroke(g.composer,black,2,20);hit(g.composer,Control::Edit);target.fill(g.action,black,20);hit(g.action,Control::ComposeAction);
    auto& draft=d.scratch;const char* source=d.draft[0]?d.draft:askLabel;size_t count=strnlen(source,sizeof(draft)-1);while(count && (uint8_t(source[count])&0xc0)==0x80)--count;memcpy(draft,source,count);draft[count]=0;for(size_t i=0;i<count;++i)if(uint8_t(draft[i])<32)draft[i]=' ';
    label(rect(g.composer.x+12,g.composer.y+6,g.composer.width-24,g.composer.height-12),draft);
    if(d.draft[0]){auto send=style;send.color=fui::Color::White;send.align=fui::TextAlign::Center;target.text(g.action,sendLabel,send);}
    else{const auto white=fui::Paint::solid(fui::Color::White);int x=g.action.x+g.action.width/2,y=g.action.y+8;
      target.stroke(rect(x-5,y,10,20),white,2,5);target.stroke(rect(x-10,y+10,20,17),white,2,8);
      target.line({int16_t(x),int16_t(y+27)},{int16_t(x),int16_t(y+33)},2,white);}
    
  }
}
}
