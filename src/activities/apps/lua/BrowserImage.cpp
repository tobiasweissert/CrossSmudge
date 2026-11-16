#include "BrowserImage.h"
#include <EpdFontFamily.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <HttpCore.h>
#include <Logging.h>
#include <cstring>
#include "Epub/converters/ImageDecoderFactory.h"
#include "Epub/converters/ImageToFramebufferDecoder.h"
namespace ink::browser {
bool drawImage(const std::string& path,GfxRenderer& renderer,int x,int y,int width,int height){
 if(x<0 || y<0 || width<=0 || height<=0 || width>renderer.getScreenWidth() || height>renderer.getScreenHeight() ||
    x>renderer.getScreenWidth()-width || y>renderer.getScreenHeight()-height)return false;
 HalFile file=Storage.open(path.c_str(),O_RDONLY);uint8_t magic[8]{};
 bool bounded=file && file.size()<=768*1024 && file.read(magic,sizeof(magic))==sizeof(magic);file.close();
 const bool png=memcmp(magic,"\x89PNG\r\n\x1a\n",8)==0;
 const bool jpg=magic[0]==0xff && magic[1]==0xd8 && magic[2]==0xff;
 // URLs often have no suffix. Factory owns the licensed, pinned streaming decoder.
 auto* decoder=bounded && (png || jpg) ? ImageDecoderFactory::getDecoder(png?"browser.png":"browser.jpg") : nullptr;
 ImageDimensions dims{};
 if(decoder && decoder->getDimensions(path,dims) && dims.width>0 && dims.height>0 && dims.width<=2048 && dims.height<=2048 && int64_t(dims.width)*dims.height<=2*1024*1024){
  RenderConfig config{x,y,width,height};config.useGrayscale=false;config.useDithering=true;config.useMonochromeDither=true;
  if(decoder->decodeToFramebuffer(path,renderer,config))return true;
 }
 LOG_ERR("LUA","Browser image rejected or decode failed");return false;
}
}

#include <Memory.h>
#include "Epub/converters/DirectPixelWriter.h"
#include "Epub/converters/JpegToFramebufferConverter.h"
#include "Epub/converters/PngToFramebufferConverter.h"
#if defined(ARDUINO_ARCH_ESP32) && !defined(SIMULATOR) && !defined(CROSSINK_MEMORY_TEST)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#else
#include <thread>
#endif
namespace ink::browser {
namespace {std::atomic<bool> imageOccupied{false};}
struct ImageSession::Job {
 std::atomic<unsigned> references{2};
 std::atomic<bool> cancel{false},done{false};
 bool success=false,adopted=false;
 char source[384]{},destination[384]{};
 int width=0,height=0;
 void release(){if(references.fetch_sub(1,std::memory_order_acq_rel)==1){
   if(destination[0] && !adopted && Storage.exists(destination))Storage.remove(destination);
   delete this;imageOccupied.store(false,std::memory_order_release);
 }}
 static void run(void* pointer){
  auto* job=static_cast<Job*>(pointer);
  ImageDecodeControl control{&job->cancel,millis(),30000};ImageDecodeScope scope(&control);
  HalFile file=Storage.open(job->source,O_RDONLY);uint8_t magic[8]{};
  bool valid=file && file.size()<=768*1024 && file.read(magic,sizeof(magic))==sizeof(magic);file.close();
  const bool png=memcmp(magic,"\x89PNG\r\n\x1a\n",8)==0;
  const bool jpg=magic[0]==0xff && magic[1]==0xd8 && magic[2]==0xff;
  // Stateless converter instances stay local: the background worker must not
  // race the EPUB factory's lazily initialized global unique_ptrs after Home.
  JpegToFramebufferConverter jpegDecoder;PngToFramebufferConverter pngDecoder;
  ImageToFramebufferDecoder* decoder=valid && (png || jpg)?(png?static_cast<ImageToFramebufferDecoder*>(&pngDecoder):static_cast<ImageToFramebufferDecoder*>(&jpegDecoder)):nullptr;
  ImageDimensions dims{};
  if(decoder && !control.stopped() && decoder->getDimensions(job->source,dims) && dims.width>0 && dims.height>0 && dims.width<=2048 && dims.height<=2048 && int64_t(dims.width)*dims.height<=2*1024*1024){
   RenderConfig config{0,0,job->width,job->height};config.cacheOnly=true;config.cachePath=job->destination;
   config.useGrayscale=false;config.useDithering=true;config.useMonochromeDither=true;
   job->success=decoder->decodeToCache(job->source,config) && !control.stopped();
  }
  if(!job->success && Storage.exists(job->destination))Storage.remove(job->destination);
  job->done.store(true,std::memory_order_release);job->release();
#if defined(ARDUINO_ARCH_ESP32) && !defined(SIMULATOR) && !defined(CROSSINK_MEMORY_TEST)
  vTaskDelete(nullptr);
#endif
 }
};
ImageSession::~ImageSession(){close();}
bool ImageSession::busy(){return imageOccupied.load(std::memory_order_acquire);}
void ImageSession::requestCancel(){std::lock_guard<std::mutex> lock(cancelMutex_);if(job_)job_->cancel.store(true);}
void ImageSession::close(){
 Job* previous;{std::lock_guard<std::mutex> lock(cancelMutex_);previous=job_;job_=nullptr;if(previous)previous->cancel.store(true);}
 if(previous)previous->release(); // Cleanup cannot hold the input signal lane.
}
const char* ImageSession::start(const std::string& source,const std::string& destination,int width,int height){
 if(job_ || imageOccupied.exchange(true))return "busy_draining";
 if(source.size()>=384 || destination.size()>=384 || source==destination || width<=0 || height<=0 || width>2048 || height>2048 || Storage.exists(destination.c_str())){
  imageOccupied.store(false);return "image_path_or_limit";
 }
 // One <1 KiB owned job, held by session and worker. It must survive Home.
 auto job=makeUniqueNoThrow<Job>();if(!job){imageOccupied.store(false);LOG_ERR("LUA","Image job allocation failed");return "out_of_memory";}
 memcpy(job->source,source.c_str(),source.size()+1);memcpy(job->destination,destination.c_str(),destination.size()+1);
 job->width=width;job->height=height;{std::lock_guard<std::mutex> lock(cancelMutex_);job_=job.release();}
#if defined(ARDUINO_ARCH_ESP32) && !defined(SIMULATOR) && !defined(CROSSINK_MEMORY_TEST)
 if(xTaskCreate(Job::run,"browser_image",6144,job_,1,nullptr)!=pdPASS){job_->references.store(1);close();LOG_ERR("LUA","Image worker allocation failed");return "worker_failed";}
#else
 std::thread(Job::run,job_).detach();
#endif
 return nullptr;
}
int ImageSession::poll(){
 Job* previous;bool success;
 {
  std::lock_guard<std::mutex> lock(cancelMutex_);
  if(!job_)return -1;
  if(!job_->done.load(std::memory_order_acquire))return 0;
  previous=job_;job_=nullptr;success=previous->success && !previous->cancel.load();
  previous->adopted=success;previous->cancel.store(true);
 }
 previous->release();return success?1:-1;
}
bool ImageSession::draw(const std::string& path,GfxRenderer& renderer,int x,int y,int width,int height,const std::atomic<bool>& cancel){
 if(x<0 || y<0 || width<=0 || height<=0 || x>renderer.getScreenWidth()-width || y>renderer.getScreenHeight()-height)return false;
 // Reused 2 KiB SD row scratch; no additional framebuffer or image-sized buffer.
 if(!scratch_){scratch_=makeUniqueNoThrow<uint8_t[]>(2048);if(!scratch_){LOG_ERR("LUA","Image row scratch allocation failed");return false;}}
 HalFile file=Storage.open(path.c_str(),O_RDONLY);uint16_t dims[2]{};
 bool ok=file && file.read(dims,sizeof(dims))==sizeof(dims);
 int w=dims[0],h=dims[1];size_t stride=(w+3)/4;
 ok=ok && w>0 && h>0 && w<=width && h<=height && stride<=2048 && file.size()==4+stride*h;
 DirectPixelWriter writer{};if(ok)writer.init(renderer);
 for(int row=0;ok && row<h;++row){
  if(cancel.load(std::memory_order_relaxed) || file.read(scratch_.get(),stride)!=stride){ok=false;break;}
  writer.beginRow(y+row);
  for(int col=0;col<w;++col)writer.writePixel(x+col,(scratch_[col/4]>>(6-2*(col%4)))&3);
 }
 file.close();return ok;
}
}
