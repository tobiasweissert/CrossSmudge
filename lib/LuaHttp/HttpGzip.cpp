#include "HttpGzip.h"
#include <MinizConfig.h>
#include <BrowserMemory.h>
#include <Memory.h>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#if defined(ARDUINO_ARCH_ESP32) && !defined(SIMULATOR)
#include <esp_heap_caps.h>
#include <Logging.h>
#endif
namespace ink::http {
namespace {
struct Workspace {tinfl_decompressor state;uint8_t window[32768];char input[2048];};
struct Input {
 BodyStream& stream;Workspace& work;size_t at=0,length=0;
 int byte(){if(at==length){int n=stream.read(stream.context,work.input,sizeof(work.input));if(n<=0)return -1;at=0;length=n;}return static_cast<uint8_t>(work.input[at++]);}
};
bool emit(Result& result,const char* bytes,size_t count){
 if(count>result.capacity-result.size){result.error="body_limit";return false;}
 if(result.sink){if(!result.sink(result.sinkContext,bytes,count)){result.error="storage_write";return false;}}
 else memcpy(result.data()+result.size,bytes,count);
 result.size+=count;return true;
}
}
bool gzip(BodyStream& stream,Result& result){
 // ~46 KiB state/ring/input cannot live on the 6 KiB TLS task stack. Keep its
 // reservation distinct from both the Lua budget and the TLS SRAM budget.
 static_assert(sizeof(Workspace)<=48*1024,"Browser gzip workspace budget");
 HeapByteBuffer storage;
#if defined(ARDUINO_ARCH_ESP32) && !defined(SIMULATOR)
 if(ink::browser::permitsResponse(sizeof(Workspace),heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)))
  storage=makePsramByteBufferNoThrow(sizeof(Workspace));
#else
 storage=makeHeapByteBufferNoThrow(sizeof(Workspace));
#endif
 if(!storage){
#if defined(ARDUINO_ARCH_ESP32) && !defined(SIMULATOR)
  LOG_ERR("LHTTP","Browser gzip workspace allocation/reserve failed");
#endif
  result.error="out_of_memory";return false;
 }
 auto* work=::new(storage.get()) Workspace{}; // Placement only; owned byte buffer frees the POD workspace.
 Input input{stream,*work};uint32_t headerCrc=0;size_t headerBytes=0;
 auto header=[&](){int b=input.byte();if(b<0 || ++headerBytes>1024)return -1;uint8_t c=b;headerCrc=mz_crc32(headerCrc,&c,1);return b;};
 int magic0=header(),magic1=header(),method=header(),flags=header();
 if(magic0!=31 || magic1!=139 || method!=8 || flags<0 || (flags&0xe0)){if(!result.error)result.error="gzip_header";return false;}
 for(int i=0;i<6;++i)if(header()<0){if(!result.error)result.error="gzip_header";return false;}
 if(flags&4){int a=header(),b=header();if(a<0 || b<0){if(!result.error)result.error="gzip_header";return false;}unsigned n=a+(b<<8);while(n--)if(header()<0){if(!result.error)result.error="gzip_header";return false;}}
 for(int bit:{8,16})if(flags&bit){int b;do{b=header();if(b<0){if(!result.error)result.error="gzip_header";return false;}}while(b);}
 if(flags&2){int a=input.byte(),b=input.byte();if(a<0 || b<0 || unsigned(a+(b<<8))!=(headerCrc&65535)){if(!result.error)result.error="gzip_header_crc";return false;}}
 tinfl_init(&work->state);size_t position=0;uint32_t crc=0;
 for(;;){
  if(!stream.ok(stream.context)){if(!result.error)result.error="cancelled";return false;}
  if(input.at==input.length){int n=stream.read(stream.context,work->input,sizeof(work->input));if(n<=0){if(!result.error)result.error="gzip_truncated";return false;}input.at=0;input.length=n;}
  size_t consumed=input.length-input.at,produced=sizeof(work->window)-position;
  auto status=tinfl_decompress(&work->state,reinterpret_cast<const uint8_t*>(work->input+input.at),&consumed,
      work->window,work->window+position,&produced,TINFL_FLAG_HAS_MORE_INPUT);
  input.at+=consumed;
  if(produced){crc=mz_crc32(crc,work->window+position,produced);if(!emit(result,reinterpret_cast<char*>(work->window+position),produced))return false;position=(position+produced)%sizeof(work->window);}
  if(status==TINFL_STATUS_DONE)break;
  if(status<0 || (!consumed && !produced)){if(!result.error)result.error="gzip_deflate";return false;}
 }
 uint32_t trailer[2]{};
 for(unsigned i=0;i<8;++i){int b=input.byte();if(b<0){if(!result.error)result.error="gzip_truncated";return false;}trailer[i/4]|=uint32_t(b)<<((i%4)*8);}
 if(trailer[0]!=crc || trailer[1]!=uint32_t(result.size)){if(!result.error)result.error="gzip_checksum";return false;}
 if(input.byte()!=-1){if(!result.error)result.error="gzip_trailing";return false;}
 return !result.error && stream.ok(stream.context);
}
}
