#include "BrowserImage.h"
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <fstream>
#include <cassert>
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include "Epub/converters/ImageDecoderFactory.h"
#include "Epub/converters/ImageToFramebufferDecoder.h"
static void load(const char* name,const char* fixture){std::ifstream input(fixture,std::ios::binary);assert(input);Storage.put(name,std::vector<uint8_t>((std::istreambuf_iterator<char>(input)),{}));}
struct Gate {
 std::mutex mutex;std::condition_variable condition;int calls=0,at=1;bool held=false,released=false;
 static void read(void* p){auto& gate=*static_cast<Gate*>(p);std::unique_lock lock(gate.mutex);if(++gate.calls==gate.at){gate.held=true;gate.condition.notify_all();gate.condition.wait(lock,[&]{return gate.released;});}}
};
static void waitDrained(){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(2);while(ink::browser::ImageSession::busy() && std::chrono::steady_clock::now()<end)std::this_thread::yield();assert(!ink::browser::ImageSession::busy());}
int main(){
 double maxDetach=0;
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
 // The real worker decodes once to a disk band cache and never sees renderer.
 for(const char* suffix:{"png","jpg","progressive.jpg"}){
  load("/async-source",(std::string("test/browser/fixtures/ramp.")+suffix).c_str());
  ink::browser::ImageSession session;assert(!session.start("/async-source","/pixels",256,64));
  int state=0;auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
  while(state==0 && std::chrono::steady_clock::now()<deadline){state=session.poll();std::this_thread::yield();}if(state!=1)fprintf(stderr,"async failed suffix=%s state=%d busy=%d\n",suffix,state,ink::browser::ImageSession::busy());assert(state==1);waitDrained();
  GfxRenderer output;std::atomic<bool> cancel{false};assert(session.draw("/pixels",output,32,40,256,64,cancel));
  size_t black=0;for(int y=40;y<104;++y)for(int x=32;x<288;++x)black+=(output.getWriteTarget()[y*100+x/8]&(0x80>>(x%8)))==0;
  assert(double(black)/(256*64)>0.43 && double(black)/(256*64)<0.57);Storage.remove("/pixels");
  for(int at:{1,2,3}){
   Gate gate;gate.at=at;auto& source=Storage.data("/async-source");source.readGate=Gate::read;source.gateContext=&gate;
   assert(!session.start("/async-source","/cancelled-pixels",256,64));
   {std::unique_lock lock(gate.mutex);assert(gate.condition.wait_for(lock,std::chrono::seconds(2),[&]{return gate.held;}));}
   auto start=std::chrono::steady_clock::now();if(at==2)session.requestCancel();else session.close();
   auto elapsed=std::chrono::steady_clock::now()-start;maxDetach=std::max(maxDetach,std::chrono::duration<double,std::milli>(elapsed).count());
   assert(elapsed<std::chrono::milliseconds(50));
   ink::browser::ImageSession replacement;assert(std::string(replacement.start("/async-source","/other",256,64))=="busy_draining");
   assert(ink::browser::ImageSession::busy());
   {std::lock_guard lock(gate.mutex);gate.released=true;gate.condition.notify_all();}
   if(at==2){int state=0;auto end=std::chrono::steady_clock::now()+std::chrono::seconds(2);while(state==0 && std::chrono::steady_clock::now()<end){state=session.poll();std::this_thread::yield();}assert(state==-1);}
   waitDrained();
   source.readGate=nullptr;source.gateContext=nullptr;
   assert(Storage.openHandles("/async-source")==0 && !Storage.exists("/cancelled-pixels"));
  }
 }
 if(const char* directory=std::getenv("BROWSER_REAL_PHOTO_ROOT")){
  for(const char* name:{"parent-image-0.jpg","parent-image-1.png","parent-image-5.jpg"}){
   const std::string path=std::string(directory)+"/"+name;load("/real-photo",path.c_str());
   ink::browser::ImageSession session;const char* error=session.start("/real-photo","/real-cache",448,400);assert(!error);
   int state=0;auto until=std::chrono::steady_clock::now()+std::chrono::seconds(10);
   while(state==0 && std::chrono::steady_clock::now()<until){state=session.poll();std::this_thread::yield();}
   waitDrained();printf("Real private photo %s prepared=%d bytes=%zu\n",name,state,Storage.bytes("/real-photo").size());assert(state==1);
   GfxRenderer page;std::atomic<bool> cancel{false};assert(session.draw("/real-cache",page,16,40,448,400,cancel));Storage.remove("/real-cache");
  }
 }
 printf("Controlled held native read max cancel detach %.3fms\n",maxDetach);
 puts("PASS owned async pinned PNG/JPEG/progressive: cache-only decode/blit density, held header/read/decode cancel detach, no stale cache/slot reuse, handles drained");
 puts("PASS real pinned JPEGDEC/PNGdec + production browser helper/converters; sniff/scale/dither/bounds/cleanup");
}
