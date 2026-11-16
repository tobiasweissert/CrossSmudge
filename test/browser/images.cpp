#include "BrowserImage.h"
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <fstream>
#include <cassert>
#include <algorithm>
#include <cstdio>
#include <cmath>
#include "Epub/converters/ImageDecoderFactory.h"
#include "Epub/converters/ImageToFramebufferDecoder.h"
static void load(const char* name,const char* fixture){std::ifstream input(fixture,std::ios::binary);assert(input);Storage.put(name,std::vector<uint8_t>((std::istreambuf_iterator<char>(input)),{}));}
int main(){
 load("/image-no-extension","test/browser/fixtures/gradient.png");load("/image-query","test/browser/fixtures/gradient.jpg");
 for(const char* path:{"/image-no-extension","/image-query"}){GfxRenderer renderer;assert(ink::browser::drawImage(path,renderer,30,40,192,128));auto* bytes=renderer.getWriteTarget();size_t black=0;for(size_t i=0;i<800*480/8;++i)black+=bytes[i]!=255;assert(black>100);assert(bytes[0]==255);assert(Storage.openHandles(path)==0);}
 // Count actual one-bit output pixels: accepted decode alone cannot detect contrast loss.
 for(const char* kind:{"midtone","ramp"})for(const char* suffix:{"png","jpg","progressive.jpg"}){
  const std::string fixture=std::string("test/browser/fixtures/")+kind+"."+suffix;
  load("/density",fixture.c_str());
  for(int maxWidth:{256,512,180,64}){
   const int width=std::min(maxWidth,256),height=width/4;
   GfxRenderer renderer;assert(ink::browser::drawImage("/density",renderer,32,40,maxWidth,maxWidth/4));
   auto* bytes=renderer.getWriteTarget();size_t black=0;
   for(int y=40;y<40+height;++y)for(int x=32;x<32+width;++x)black+=(bytes[y*100+x/8]&(0x80>>(x%8)))==0;
   const double density=double(black)/(width*height);
   printf("%s %s width %d black density %.4f\n",kind,suffix,width,density);fflush(stdout);
   assert(density>0.43 && density<0.57);
   if(std::string(kind)=="ramp"){
    for(int band=0;band<4;++band){size_t ink=0;for(int y=40;y<40+height;++y)for(int x=32+band*width/4;x<32+(band+1)*width/4;++x)ink+=(bytes[y*100+x/8]&(0x80>>(x%8)))==0;
     const double expected=1.0-(band*64+31.5)/255.0;assert(std::abs(double(ink)/(width/4*height)-expected)<0.09);}
   }
   assert(Storage.openHandles("/density")==0);
  }
 }
 // Default converter callers still produce their prior four-level output.
 load("/default-mid","test/browser/fixtures/midtone.png");
 RenderConfig legacy{32,40,256,64};assert(!legacy.useMonochromeDither);
 GfxRenderer legacyRenderer;
 assert(ImageDecoderFactory::getDecoder("test.png")->decodeToFramebuffer("/default-mid",legacyRenderer,legacy));
 for(int y=40;y<104;++y)for(int x=32;x<288;++x)assert((legacyRenderer.getWriteTarget()[y*100+x/8]&(0x80>>(x%8)))==0);
 GfxRenderer renderer;assert(!ink::browser::drawImage("/image-no-extension",renderer,790,0,192,128));
 auto oversized=Storage.bytes("/image-no-extension");oversized[16]=0x7f;Storage.put("/oversized",oversized);assert(!ink::browser::drawImage("/oversized",renderer,0,0,192,128));
 Storage.put("/truncated",{0xff,0xd8,0xff,0,0,0,0,0});assert(!ink::browser::drawImage("/truncated",renderer,0,0,192,128));
 puts("PASS real pinned JPEGDEC/PNGdec + production browser helper/converters; sniff/scale/dither/bounds/cleanup");
}
