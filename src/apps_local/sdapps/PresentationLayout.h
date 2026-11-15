#pragma once
#include <FreeInkUICore.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include "AppAbi.h"
namespace sdapps::presentation {
namespace ui = freeink::ui;
constexpr uint32_t TEXT_CAP = CP_APP_MAX_TEXT * 2 + 128;
constexpr uint32_t PAGE_CAP = 131072;
enum class Control : int16_t { Back, Address, Menu, NewChat, Edit, ComposeAction, Body };
struct Run { uint32_t start, end; uint8_t role; };
struct Span { uint32_t start, end; int32_t action; };
struct Geometry {
  ui::Rect header, body, footer, composer, action, left, center, right, status, page;
};
inline ui::Rect rect(int x,int y,int w,int h) {return {int16_t(x),int16_t(y),int16_t(std::max(0,w)),int16_t(std::max(0,h))};}
inline Geometry geometry(ui::Rect safe, bool chat, int lineHeight, int statusHeight=24) {
  const int pad=24, header=std::max(56,lineHeight*2+8), footer=chat?statusHeight+std::max(52,lineHeight+16)+12:statusHeight*2+8;
  Geometry g{};g.header=rect(safe.x,safe.y,safe.width,header);
  g.left=rect(safe.x,safe.y,48,header);g.right=rect(safe.right()-48,safe.y,48,header);
  g.center=rect(safe.x+48,safe.y+4,safe.width-96,header-8);
  g.footer=rect(safe.x,safe.bottom()-footer,safe.width,footer);
  g.body=rect(safe.x+pad,safe.y+header+pad,safe.width-2*pad,safe.height-header-footer-2*pad);
  g.status=rect(safe.x+8,g.footer.y,safe.width-16,statusHeight);
  g.page=chat?rect(safe.right()-80,g.footer.y,72,statusHeight):rect(safe.x,g.footer.y+statusHeight,safe.width,statusHeight);
  if(chat)g.status.width-=80;
  g.composer=rect(safe.x+8,g.footer.y+statusHeight+6,safe.width-88,footer-statusHeight-12);
  g.action=rect(safe.right()-72,g.composer.y,64,g.composer.height);
  return g;
}
struct Model {
  char text[TEXT_CAP+1]{},title[64]{},address[2049]{},status[192]{},draft[8193]{};
  char scratch[192]{};
  Run runs[CP_APP_MAX_MESSAGES+1]{};Span spans[CP_APP_MAX_SPANS]{};
  uint32_t length=0,runCount=0,spanCount=0,identity=0;
  uint32_t pages[PAGE_CAP+1]{},pageCount=1,page=0;
  struct Hit { ui::Rect bounds; int32_t action; };
  Hit hits[4096]{};uint32_t hitCount=0;
  CpPresentation binding{};
  bool valid=false,dirty=true,followTail=false;
  uint32_t contentHash=0;
  int width=0,height=0,font=0;
  ui::FontId titleFont=0;
};
struct Line {uint32_t start,end,next;int width;uint8_t role;};
inline uint8_t roleAt(const Model& m,uint32_t at) {
  for(uint32_t i=0;i<m.runCount;++i)if(at>=m.runs[i].start && at<m.runs[i].end)return m.runs[i].role;
  return CP_ASSISTANT;
}
inline ui::TextStyle lineStyle(ui::TextStyle style,uint8_t role,ui::FontId titleFont) {
  style.maxLines=1;style.align=ui::TextAlign::Left;style.bold=role==2;if(role==2)style.font=titleFont;style.color=role==CP_USER?ui::Color::White:ui::Color::Black;return style;
}
// Bounded fragments are drawn separately: no shaping/ligature crosses an
// anchor. RTL shaping within each fragment belongs to DrawTarget; fragments
// follow logical document order, not full paragraph-level bidi layout.
constexpr uint32_t LINE_BYTES=156;
inline uint32_t segmentEnd(const Model& m,uint32_t at,uint32_t end,int32_t& action) {
  uint32_t lo=0,hi=m.spanCount;
  while(lo<hi){uint32_t mid=(lo+hi)/2;if(m.spans[mid].end<=at)lo=mid+1;else hi=mid;}
  action=0;if(lo==m.spanCount)return end;
  const auto& span=m.spans[lo];
  if(span.start>at)return std::min(end,span.start);
  action=span.action;return std::min(end,span.end);
}
inline int segmentWidth(const char* text,uint32_t length,ui::DrawTarget& target,ui::TextStyle style) {
  char buffer[LINE_BYTES+1];uint32_t ink=length;
  while(ink && text[ink-1]==' ')--ink;
  memcpy(buffer,text,ink);buffer[ink]=0;
  const int space=target.measureText(style.font," x",style).width-target.measureText(style.font,"x",style).width;
  return target.measureText(style.font,buffer,style).width+int(length-ink)*std::max(1,space);
}
inline int rangeWidth(const Model& m,uint32_t at,uint32_t end,ui::DrawTarget& target,ui::TextStyle style) {
  int width=0;
  while(at<end){int32_t action;uint32_t next=segmentEnd(m,at,end,action);width+=segmentWidth(m.text+at,next-at,target,style);at=next;}
  return width;
}
// Every emitted byte range is contiguous, including whitespace and UTF-8.
// The bounded buffer defines a line break, never a truncation.
inline Line nextLine(const Model& m,uint32_t start,int width,ui::DrawTarget& target,ui::TextStyle style) {
  Line line{start,start,start,0,roleAt(m,start)};
  style=lineStyle(style,line.role,m.titleFont);
  uint32_t at=start,lastSpace=start;int lastWidth=0;
  while(at<m.length && m.text[at]!='\n') {
    uint32_t end=at+1;while(end<m.length && (uint8_t(m.text[end])&0xc0)==0x80)++end;
    if(end-start>LINE_BYTES)break;
    int measured=rangeWidth(m,start,end,target,style);
    if(measured>width && at>start)break;
    at=end;line.width=measured;
    if(m.text[at-1]==' '){lastSpace=at;lastWidth=measured;}
  }
  if(at<m.length && m.text[at]!='\n' && lastSpace>start){at=lastSpace;line.width=lastWidth;}
  line.end=at;line.next=at;
  if(at<m.length && m.text[at]=='\n')++line.next;
  return line;
}
inline int lineWidth(ui::Rect body,uint8_t role) {return body.width-(role==CP_USER?48:0);}
inline void paginate(Model& m,const Geometry& g,ui::DrawTarget& target,ui::TextStyle style) {
  if(!m.dirty && m.width==g.body.width && m.height==g.body.height && m.font==style.font)return;
  const uint32_t anchor=m.pages[std::min(m.page,m.pageCount-1)];
  m.width=g.body.width;m.height=g.body.height;m.font=style.font;m.pageCount=1;m.pages[0]=0;
  uint32_t at=0;int y=0;
  while(at<m.length) {
    const int step=std::max(1,int(target.lineHeight(roleAt(m,at)==2?m.titleFont:style.font))+8);
    if(y+step>g.body.height && y>0){if(m.pageCount>=PAGE_CAP)break;m.pages[m.pageCount++]=at;y=0;}
    auto line=nextLine(m,at,lineWidth(g.body,roleAt(m,at)),target,style);
    if(line.next<=at)break;
    at=line.next;y+=step;
  }
  m.pages[m.pageCount]=m.length;m.page=0;
  while(m.page+1<m.pageCount && m.pages[m.page+1]<=anchor)++m.page;
  if(m.followTail){m.page=m.pageCount-1;m.followTail=false;}
  m.dirty=false;
}
// Link hit-testing uses the same measured fragments as painting. The caller
// publishes one body interaction, avoiding the shared 24-control table limit.
template<class Fragment>
void drawBody(const Model& m,const Geometry& g,ui::DrawTarget& target,ui::TextStyle style,Fragment fragment) {
  int y=g.body.y;uint32_t at=m.pages[m.page],end=m.pages[m.page+1];
  while(at<end) {
    const int step=std::max(1,int(target.lineHeight(roleAt(m,at)==2?m.titleFont:style.font))+8);
    if(y+step>g.body.bottom())break;
    auto line=nextLine(m,at,lineWidth(g.body,roleAt(m,at)),target,style);
    if(line.next<=at)break;
    const int x=g.body.x+(line.role==CP_USER?40:0);
    auto ls=lineStyle(style,line.role,m.titleFont);
    if(line.role==CP_USER){
      bool first=at==m.pages[m.page],last=line.next==end;
      for(uint32_t i=0;i<m.runCount;++i)if(at>=m.runs[i].start && at<m.runs[i].end){first=first||at==m.runs[i].start;last=last||line.next>=m.runs[i].end;break;}
      target.fill(rect(x-8,y-2,g.body.right()-x+8,step),ui::Paint::solid(ui::Color::Black),8,
        (first?ui::CornersTop:0)|(last?ui::CornersBottom:0));
    }
    uint32_t from=line.start;int dx=0;
    while(from<line.end){
      int32_t action;const uint32_t to=segmentEnd(m,from,line.end,action);
      const int w=segmentWidth(m.text+from,to-from,target,ls);
      char buffer[LINE_BYTES+1];memcpy(buffer,m.text+from,to-from);buffer[to-from]=0;
      const auto bounds=rect(x+dx,y,w,step);target.text(bounds,buffer,ls);
      if(action){target.line({bounds.x,int16_t(y+step-5)},{bounds.right(),int16_t(y+step-5)},1,ui::Paint::solid(ui::Color::Black));fragment(bounds,action);}
      dx+=w;from=to;
    }
    at=line.next;y+=step;
  }
}
}
