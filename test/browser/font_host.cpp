// Host capture adapter: production EpdFont/Family metrics and builtin bitmaps.
// This is not the device GfxRenderer, panel LUT or physical refresh pipeline.
#include "EpdFontFamily.h"
#include "Utf8.h"
#include "builtinFonts/inter_12_regular.h"
#include "builtinFonts/inter_12_bold.h"
#include "builtinFonts/inter_8_regular.h"
#include "builtinFonts/lexenddeca_14_regular.h"
#include "builtinFonts/lexenddeca_14_bold.h"
#include "builtinFonts/lexenddeca_16_regular.h"
#include "builtinFonts/lexenddeca_16_bold.h"
#ifdef BROWSER_IMAGE_HOST
#include <BrowserImage.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#endif
#include <MinizConfig.h>
#include <vector>
#include <cstdio>
#include <cstring>
extern "C" {
#include "lauxlib.h"
}
static unsigned char canvas[800*800];static int width=480,height=800;
static EpdFont ui(&inter_12_regular),bold(&inter_12_bold),small(&inter_8_regular),l14(&lexenddeca_14_regular),b14(&lexenddeca_14_bold),l16(&lexenddeca_16_regular),b16(&lexenddeca_16_bold);
static EpdFontFamily f12(&ui,&bold),f8(&small),f14(&l14,&b14),f16(&l16,&b16);
static EpdFontFamily& font(lua_State* L,int index){const char* n=luaL_optstring(L,index,"ui12");return !strcmp(n,"small")?f8:!strcmp(n,"lexend14")?f14:!strcmp(n,"lexend16")?f16:f12;}
static EpdFontFamily::Style style(lua_State* L,int index){return !strcmp(luaL_optstring(L,index,"regular"),"bold")?EpdFontFamily::BOLD:EpdFontFamily::REGULAR;}
static int fw(lua_State* L){int w,h;font(L,2).getTextDimensions(luaL_checkstring(L,1),&w,&h,style(L,3));lua_pushinteger(L,w);return 1;}
static int fh(lua_State* L){lua_pushinteger(L,font(L,1).getData()->advanceY);return 1;}
static int advance(lua_State* L){const char* s=luaL_checkstring(L,1);auto& f=font(L,2);auto st=style(L,3);int x=0;uint32_t prev=0;int32_t pending=0;while(uint32_t cp=utf8NextCodepoint((const uint8_t**)&s)){cp=f.applyLigatures(cp,s,st);auto* g=f.getGlyph(cp,st);if(g){if(prev)x+=fp4::toPixel(pending+f.getKerning(prev,cp,st));pending=g->advanceX;prev=cp;}}x+=fp4::toPixel(pending);lua_pushinteger(L,x);return 1;}
static void pixel(int x,int y,bool black){if(x>=0&&y>=0&&x<width&&y<height)canvas[y*width+x]=black?0:255;}
static int clear(lua_State* L){width=luaL_optinteger(L,1,480);height=luaL_optinteger(L,2,800);memset(canvas,255,sizeof(canvas));return 0;}
static int rect(lua_State* L){int x=lua_tointeger(L,1),y=lua_tointeger(L,2),w=lua_tointeger(L,3),h=lua_tointeger(L,4);bool fill=lua_toboolean(L,5);for(int yy=y;yy<y+h;++yy)for(int xx=x;xx<x+w;++xx)if(fill||yy==y||yy==y+h-1||xx==x||xx==x+w-1)pixel(xx,yy,true);return 0;}
static int text(lua_State* L){int x=lua_tointeger(L,1),y=lua_tointeger(L,2);const char* s=luaL_checkstring(L,3);auto& f=font(L,4);auto st=style(L,5);const char* align=luaL_optstring(L,6,"left");int tw,th;f.getTextDimensions(s,&tw,&th,st);if(!strcmp(align,"center"))x-=tw/2;else if(!strcmp(align,"right"))x-=tw;int pending=0;uint32_t prev=0;bool black=lua_gettop(L)<7||lua_toboolean(L,7);
 while(uint32_t cp=utf8NextCodepoint((const uint8_t**)&s)){cp=f.applyLigatures(cp,s,st);auto data=f.getGlyphData(cp,st);if(!data.glyph)continue;auto* g=data.glyph;auto* d=data.fontData;if(prev)x+=fp4::toPixel(pending+f.getKerning(prev,cp,st));const uint8_t* bitmap=d->bitmap+g->dataOffset;std::vector<uint8_t> decoded;bool aligned=false;
  if(d->groupCount){size_t index=g-d->glyph;for(unsigned i=0;i<d->groupCount;++i){const auto& group=d->groups[i];if(index>=group.firstGlyphIndex&&index<group.firstGlyphIndex+group.glyphCount){decoded.resize(group.uncompressedSize);size_t produced=tinfl_decompress_mem_to_mem(decoded.data(),decoded.size(),d->bitmap+group.compressedOffset,group.compressedSize,0);if(produced!=decoded.size())return luaL_error(L,"font inflate");size_t offset=0;for(size_t j=group.firstGlyphIndex;j<index;++j)offset+=((d->glyph[j].width+3)/4)*d->glyph[j].height;bitmap=decoded.data()+offset;aligned=true;break;}}}
  for(int yy=0;yy<g->height;++yy)for(int xx=0;xx<g->width;++xx){int pos=aligned?yy*((g->width+3)/4)*4+xx:yy*g->width+xx;bool ink=d->is2Bit?((bitmap[pos/4]>>((3-pos%4)*2))&3)!=0:((bitmap[pos/8]>>(7-pos%8))&1);if(ink)pixel(x+g->left+xx,y+d->ascender-g->top+yy,black);}pending=g->advanceX;prev=cp;
 }return 0;}
static int capture(lua_State* L){FILE* f=fopen(luaL_checkstring(L,1),"wb");if(!f)return luaL_error(L,"capture open");fprintf(f,"P5\n%d %d\n255\n",width,height);fwrite(canvas,1,width*height,f);fclose(f);return 0;}
#ifdef BROWSER_IMAGE_HOST
static int imageData(lua_State* L){
 size_t count=0;const char* data=luaL_checklstring(L,1,&count);int x=luaL_checkinteger(L,2),y=luaL_checkinteger(L,3),w=luaL_checkinteger(L,4),h=luaL_checkinteger(L,5);
 bool ok=false;
 {Storage.put("/font-preview-image",std::vector<uint8_t>(data,data+count));GfxRenderer renderer(width,height);
  ok=ink::browser::drawImage("/font-preview-image",renderer,x,y,w,h);
  if(ok){auto* bitmap=renderer.getWriteTarget();int stride=(width+7)/8;for(int yy=y;yy<y+h;++yy)for(int xx=x;xx<x+w;++xx)pixel(xx,yy,(bitmap[yy*stride+xx/8]&(0x80>>(xx%8)))==0);}
  Storage.remove("/font-preview-image");}
 lua_pushboolean(L,ok);return 1;
}
#endif
void registerFontHost(lua_State* L){
#ifdef BROWSER_IMAGE_HOST
 lua_register(L,"font_image_data",imageData);
#endif
for(auto pair:{std::pair<const char*,lua_CFunction>{"font_width",fw},{"font_height",fh},{"font_advance",advance},{"font_clear",clear},{"font_text",text},{"font_rect",rect},{"font_capture",capture}}){lua_pushcfunction(L,pair.second);lua_setglobal(L,pair.first);}}
