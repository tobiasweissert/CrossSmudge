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
