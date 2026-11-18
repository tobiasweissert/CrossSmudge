#include <cassert>
#include <cstdio>
#include <climits>
#include <initializer_list>
#include "HalStorageSink.h"
using namespace airdrop::storage;
bool ioLocked=false;
unsigned writeCalls=0,readCalls=0,yields=0;
size_t shortWrite=SIZE_MAX;
int readResult=0;
int64_t clockUs=0,ioCostUs=0;
int64_t esp_timer_get_time(){return clockUs;}
void vTaskDelay(int n){assert(n==1&&!ioLocked);++yields;clockUs+=1000;}
namespace airdrop::storage {
// Test lifetime only. Production I/O, cancellation, fields and cadence are extracted.
HalFiles::HalFiles()=default;
HalFiles::~HalFiles()=default;
// PRODUCTION METHODS
}
int main(){
 uint8_t data[1025]{};
 for(bool reading:{false,true})for(size_t fragment:{size_t(1),size_t(64),size_t(1024)}){
  clockUs=0;yields=0;writeCalls=readCalls=0;HalFiles f;
  for(size_t n=0;n<8192;n+=fragment){if(reading)assert(f.read(data,fragment)==int(fragment));else assert(f.writeStage(data,fragment)==fragment);}
  if(yields!=2){fprintf(stderr,"FAIL bounded 4KiB yield quantum reading=%d fragment=%zu yields=%u expected=2\n",reading,fragment,yields);return 1;}
 }
 for(bool reading:{false,true}){
  clockUs=0;yields=0;HalFiles f;ioCostUs=1000;
  for(int i=0;i<4;++i){if(reading)assert(f.read(data,1)==1);else assert(f.writeStage(data,1)==1);}
  assert(yields==4);ioCostUs=0; // Slow I/O must yield after at most 1ms, even tiny spans.
 }
 {clockUs=0;yields=0;HalFiles f;auto writes=writeCalls,reads=readCalls;
  assert(f.writeStage(data,1025)==0&&f.read(data,1025)==-1);assert(writeCalls==writes&&readCalls==reads);
  f.requestCancel();assert(f.writeStage(data,1)==0&&f.read(data,1)==-1);assert(writeCalls==writes&&readCalls==reads);
 }
 {clockUs=1000;HalFiles f;f.setDeadlineMs(1);auto calls=writeCalls+readCalls;assert(f.writeStage(data,1)==0&&f.read(data,1)==-1);assert(writeCalls+readCalls==calls);}
 {clockUs=0;HalFiles f;shortWrite=3;assert(f.writeStage(data,9)==3);shortWrite=SIZE_MAX;readResult=-1;assert(f.read(data,9)==-1);readResult=0;}
 puts("PASS production HAL byte/time bounded yielding + cancellation/deadline + short/error I/O; synthetic SPI lock");
}
