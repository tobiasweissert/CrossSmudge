#include <GfxRenderer.h>
#include <FontCacheManager.h>
#include <FontDecompressor.h>
#include <SdCardFontManager.h>
#include <SdCardFont.h>
#include <builtinFonts/inter_12_regular.h>
#include <cassert>
#include <cstring>
template<class T> concept LegacyAdvanceAccepts = requires(const GfxRenderer& r,T value) {r.getTextAdvanceX(1,"A",EpdFontFamily::REGULAR,value);};
static_assert(LegacyAdvanceAccepts<uint32_t>);
static_assert(!LegacyAdvanceAccepts<int8_t>);
static_assert(!LegacyAdvanceAccepts<uint8_t>);
static int8_t kern(void*,uint32_t left,uint32_t right){return left=='A' && right==0x3A9 ? -8 : 0;}
int main(){
 HalDisplay display(64,48);GfxRenderer r(display);r.begin();r.setOrientation(GfxRenderer::LandscapeCounterClockwise);
 const uint8_t primaryBitmap[]={0,0};const uint8_t fallbackBitmap[]={0xFF,0xFF,0xFF,0xFF};
 const EpdGlyph primaryGlyph[]={{4,4,72,0,4,2,0}};
 const EpdGlyph fallbackGlyph[]={{4,4,88,0,4,4,0}};
 const EpdUnicodeInterval primaryInterval[]={{'A','A',0}}, fallbackInterval[]={{0x3A9,0x3A9,0},{0x4E00,0x4E00,0}};
 EpdFontData pd{},fd{};pd.kernHandler=kern;pd.bitmap=primaryBitmap;pd.glyph=primaryGlyph;pd.intervals=primaryInterval;pd.intervalCount=1;pd.ascender=8;pd.advanceY=10;
 fd.bitmap=fallbackBitmap;fd.glyph=fallbackGlyph;fd.intervals=fallbackInterval;fd.intervalCount=2;fd.ascender=8;fd.advanceY=10;fd.is2Bit=true;
 EpdFont p(&pd),f(&fd);r.insertFont(1,EpdFontFamily(12,&p,nullptr,nullptr,nullptr,&f));
 r.clearScreen();r.drawText(1,2,0,"\xE4\xB8\x80");assert(r.isPixelBlack(2,4));
 r.clearScreen();r.drawScaledCodepoint(1,0x4E00,EpdFontFamily::REGULAR,2,8,256);assert(r.isPixelBlack(2,4));
 r.clearScreen();r.beginTextClip(3,4,1,2);r.drawText(1,2,0,"\xE4\xB8\x80");r.endTextClip();assert(!r.isPixelBlack(2,4));assert(r.isPixelBlack(3,4));assert(!r.isPixelBlack(4,4));
 assert(r.getTextAdvanceX(1,"A",EpdFontFamily::REGULAR,0x1F600)==5);
 assert(r.getTextAdvanceX(1,"A",EpdFontFamily::REGULAR,0x3A9)==4);
 assert(r.getTextAdvanceX(1,"A\xCE\xA9",EpdFontFamily::REGULAR)==10);
 assert(r.getTextAdvanceXTracked(1,"AA",EpdFontFamily::REGULAR,2)==12);
 auto* bm=r.getFrameBuffer();memset(bm,0xFF,r.getBufferSize());r.drawTextRotated90CW(1,1,12,"\xE4\xB8\x80");bool ink=false;for(size_t i=0;i<r.getBufferSize();++i)ink|=bm[i]!=0xFF;assert(ink);
 r.clearScreen();r.drawText(1,2,0,"\xCE\x93");bool syntheticInk=false;for(size_t i=0;i<r.getBufferSize();++i)syntheticInk|=r.getFrameBuffer()[i]!=0xFF;assert(syntheticInk);
 // Small caps must also fetch pixels/bit depth from the supplying shared fallback.
 EpdFontData blank=pd, capitals=fd;blank.intervalCount=0;capitals.intervals=primaryInterval;capitals.intervalCount=1;
 EpdFont empty(&blank),caps(&capitals);r.insertFont(2,EpdFontFamily(12,&empty,nullptr,nullptr,nullptr,&caps));
 r.clearScreen();r.drawText(2,2,0,"a",true,EpdFontFamily::SMALL_CAPS);assert(r.isPixelBlack(2,5));
 r.removeFont(2);
 r.removeFont(1);assert(r.getFontMap().empty());
 SdCardFont unloaded;r.registerSdCardFont(9,&unloaded);
 FontCacheManager cache(r.getFontMap(),r.getSdCardFonts(),r.getTtfFonts());
 assert(!cache.prewarmCache(9,"A",1,FontCacheManager::PreparationPolicy::DictionaryLean));
 r.removeFont(9);
 // Real owner Inter compressed asset through actual FontDecompressor + renderer.
 FontDecompressor decompressor;cache.setFontDecompressor(&decompressor);r.setFontCacheManager(&cache);
 EpdFont inter(&inter_12_regular);r.insertFont(20,EpdFontFamily(12,&inter));r.clearScreen();
 r.drawText(20,1,0,"AV");bool realInk=false;for(size_t i=0;i<r.getBufferSize();++i)realInk|=r.getFrameBuffer()[i]!=0xFF;assert(realInk);
 assert(r.getTextAdvanceX(20,"A",EpdFontFamily::REGULAR,'V')+r.getTextAdvanceX(20,"V",EpdFontFamily::REGULAR)==r.getTextAdvanceX(20,"AV",EpdFontFamily::REGULAR));
 for(auto orientation:{GfxRenderer::Portrait,GfxRenderer::LandscapeClockwise,GfxRenderer::PortraitInverted,GfxRenderer::LandscapeCounterClockwise}) {
   r.setOrientation(orientation);r.clearScreen();r.drawText(20,1,0,"A");assert(r.getTextAdvanceX(20,"A",EpdFontFamily::REGULAR)>0);
 }
 r.setFontCacheManager(nullptr);
 r.removeFont(20);
}
