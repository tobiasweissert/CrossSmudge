#include "ModuleActivity.h"
#include "PresentationScreen.h"
#include "PresentationValidation.h"
#if defined(FREEINK_DEVICE_STICKY)
#include <HalMemory.h>
#include <HalStorage.h>
#include <Logging.h>
#include <I18n.h>
#include <Memory.h>
#include <TrustedTime.h>
#include <WiFi.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include "DevMode.h"
#include "KeySettingsActivity.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "components/HeaderBackTapTarget.h"
#include "components/controls/button.h"
#include "../Shelf.h"
#include "../ui/ToyboxWrappedText.h"
#include "services/BrowserCore.h"
#include "services/BrowserPolicy.h"
#include "services/BrowserFetch.h"
#include "services/BrowserCache.h"
#include "services/ChatLayout.h"
#include "services/ChatPost.h"
#include "services/ChatCapture.h"
#include "services/PdmMicrophone.h"
#include "services/ProviderKey.h"
namespace sdapps {
namespace fui = freeink::ui;
namespace {
constexpr size_t MODULE_HEAP_CAP = 2 * 1024 * 1024;
constexpr size_t DOCUMENT_CAP = 512 * 1024;
size_t boundedLength(const char* s,size_t cap){return s?strnlen(s,cap):0;}
bool copyText(char* out,size_t cap,const char* text) {
  if(!text){out[0]=0;return true;}
  const size_t n=boundedLength(text,cap);if(n==cap)return false;
  memcpy(out,text,n+1);return true;
}
void preview(char* out,size_t cap,const char* text) {
  if(!text){out[0]=0;return;}
  size_t n=boundedLength(text,cap-1);
  while(n && (static_cast<unsigned char>(text[n])&0xc0)==0x80)--n;
  memcpy(out,text,n);out[n]=0;
  for(size_t i=0;i<n;++i)if(out[i]=='\n'||out[i]=='\r'||out[i]=='\t')out[i]=' ';
}
size_t documentBytes(const stickyweb::Document& d) {
  size_t bytes=d.title.capacity()+d.text.capacity()+d.links.capacity()*sizeof(stickyweb::Link)+d.spans.capacity()*sizeof(stickyweb::LinkSpan);
  for(const auto& link:d.links)bytes+=link.label.capacity()+link.url.capacity();
  return bytes;
}
}
struct ModuleActivity::Runtime {
  enum class Child { None, Edit, Wifi, Key };
  enum class Job { Request, Capture };
  struct Model {
    CpViewKind kind=CP_LIST;
    char title[64]{},status[192]{},text[CP_APP_MAX_TEXT+1]{},assistant[16385]{},draft[8193]{};
    char labels[CP_APP_MAX_ROWS][160]{};
    int32_t ids[CP_APP_MAX_ROWS]{};
    uint32_t flags[CP_APP_MAX_ROWS]{};
    fui::ListItem items[CP_APP_MAX_ROWS]{};
    uint32_t count=0;
    char previewYou[516]{},previewAssistant[516]{},previewDraft[516]{};
  };
  HalMemory::PsramBuffer modelBytes;
  HalMemory::PsramBuffer designBytes;
  presentation::Model* design=nullptr;
  Model* model=nullptr;
  struct Allocation { uint8_t* pointer=nullptr;size_t size=0; } allocations[8];
  size_t allocated=0;
  struct Document { std::unique_ptr<stickyweb::Document> value;size_t bytes=0;uint32_t id=0; } docs[17];
  size_t documentBytes=0;uint32_t nextDocument=0;
  fui::ListNav nav;
  toybox::WrappedText wrapped;
  uint32_t topLine=0,lineCount=0,visibleLines=1;
  bool hold=false,touchHold=false,wifiOwned=false,bodyGesture=false;
  int32_t holdId=0;
  Child child=Child::None;
  char editTitle[64]{},editText[8193]{};
  size_t editLimit=0;int32_t editId=0;
  Job job=Job::Request;
  CpRequestKind requestKind=CP_GET;
  bool busy=false,pending=false;
  std::atomic<bool> cancelled{false},stopRecording{false},done{false};
  TaskHandle_t task=nullptr;
  int32_t jobId=0,lastGetId=0,resultStatus=-1;
  uint32_t getStarted=0;
  char url[2049]{};
  HalMemory::PsramBuffer requestBytes,wav;
  size_t requestLength=0;
  std::string key,result,location;
  stickychat::PdmMicrophone microphone;
  ~Runtime() {
    for(auto& allocation:allocations)if(allocation.pointer)HalMemory::PsramDeleter{}(allocation.pointer);
    if(model)std::destroy_at(model);
  }
};
ModuleActivity::ModuleActivity(GfxRenderer& r,MappedInputManager& i,const char* folder,const Package& p)
 :Activity("SdExecutable",r,i),UiAppHost(r),package(p),root(folder){}
ModuleActivity::~ModuleActivity()=default;
std::unique_ptr<Activity> ModuleActivity::create(GfxRenderer& r,MappedInputManager& i,const char* folder,const Package& p) {
  return makeUniqueNoThrow<ModuleActivity>(r,i,folder,p);
}
void ModuleActivity::onEnter() {
  Activity::onEnter();
  // Runtime owns fixed job/edit buffers; model and module working sets are
  // PSRAM-only so their bounded hundreds of KiB never consume task stacks.
  runtime=makeUniqueNoThrow<Runtime>();
  if(!runtime){LOG_ERR("SDAPP","OOM: runtime");leaving=true;return;}
  runtime->modelBytes=HalMemory::allocatePsram(sizeof(Runtime::Model));
  runtime->requestBytes=HalMemory::allocatePsram(32769);
  if(!runtime->modelBytes || !runtime->requestBytes){LOG_ERR("SDAPP","OOM: UI/request buffers");leaving=true;return;}
  runtime->model=std::construct_at(reinterpret_cast<Runtime::Model*>(runtime->modelBytes.get()));
  resetUi();app.on(1,[](const fui::ActionEvent& e,void* p){static_cast<ModuleActivity*>(p)->activate(e.value);},this);
  app.setScreen([](UiScreen& screen,void* p){static_cast<ModuleActivity*>(p)->buildScreen(screen);},this);
  if(package.abi==CP_APP_ABI_V2){
    // One bounded presentation arena (~0.96 MiB), PSRAM only. Too large for
    // stack/internal heap; reused for the entire activity, no render allocation.
    runtime->designBytes=HalMemory::allocatePsram(sizeof(presentation::Model));
    if(!runtime->designBytes){LOG_ERR("SDAPP","OOM: presentation arena");leaving=true;return;}
    runtime->design=std::construct_at(reinterpret_cast<presentation::Model*>(runtime->designBytes.get()));
  }
  bindServices();
  if(!image.load(root,package)){notice("Package could not be loaded. Check its ABI, target and hash. Back returns to the shelf.");return;}
  if(!image.module().start(&host,&moduleState)){
    drain();
    if(moduleState){image.module().stop(moduleState);moduleState=nullptr;}
    image.unload();
    notice("Module initialization failed. Back returns to the shelf.");return;
  }
  started=true;
}
void ModuleActivity::notice(const char* text) {
  CpView view{sizeof(CpView),CP_LIST,package.title,text,"","","",nullptr,0};present(&view);
}
int ModuleActivity::present(const CpView* view) {
  if(!runtime || !runtime->model || !view || view->size!=sizeof(*view) || view->row_count>CP_APP_MAX_ROWS ||
     (view->row_count && !view->rows) || view->kind<CP_LIST || view->kind>CP_CHAT)return 0;
  if(boundedLength(view->status,192)>=192 ||
     boundedLength(view->text,CP_APP_MAX_TEXT+1)>CP_APP_MAX_TEXT || boundedLength(view->assistant_text,16385)>16384 ||
     boundedLength(view->draft,8193)>8192)return 0;
  RenderLock lock;
  auto& m=*runtime->model;
  if(runtime->design)runtime->design->valid=false;
  runtime->bodyGesture=false;
  closeRouting();m.kind=view->kind;m.count=view->row_count;
  preview(m.title,sizeof(m.title),view->title);copyText(m.status,sizeof(m.status),view->status);
  copyText(m.text,sizeof(m.text),view->text);copyText(m.assistant,sizeof(m.assistant),view->assistant_text);copyText(m.draft,sizeof(m.draft),view->draft);
  preview(m.previewYou,sizeof(m.previewYou),m.text);preview(m.previewAssistant,sizeof(m.previewAssistant),m.assistant);
  preview(m.previewDraft,sizeof(m.previewDraft),m.draft[0]?m.draft:"Type a message");
  for(uint32_t i=0;i<m.count;++i){preview(m.labels[i],sizeof(m.labels[i]),view->rows[i].label);m.ids[i]=view->rows[i].id;
    m.flags[i]=view->rows[i].flags;m.items[i].label=m.labels[i];m.items[i].actionValue=i;}
  runtime->nav.reset();runtime->topLine=0;
  resetUi();requestUpdate();return 1;
}
void ModuleActivity::emit(CpEventKind kind,int32_t id,int32_t status,const char* text,size_t length,const char* location) {
  if(!started)return;
  CpEvent event{sizeof(CpEvent),kind,id,status,text,length,location};image.module().event(moduleState,&event);
}
void ModuleActivity::activate(int index) {
  if(!runtime || !runtime->model || index<0 || static_cast<uint32_t>(index)>=runtime->model->count)return;
  const int32_t id=runtime->model->ids[index];
  app.clearTapFlash();emit(CP_SELECT,id);
}
void ModuleActivity::cancel(){if(runtime){runtime->cancelled=true;runtime->stopRecording=true;runtime->child=Runtime::Child::None;}}
void ModuleActivity::drain(){
  if(!runtime)return;cancel();runtime->pending=false;
  while(runtime->task && !runtime->done.load(std::memory_order_acquire))vTaskDelay(pdMS_TO_TICKS(5));
  runtime->task=nullptr;runtime->busy=false;runtime->microphone.end();runtime->wav.reset();
}
void ModuleActivity::shutdown(){
  drain();
  if(started){started=false;image.module().stop(moduleState);moduleState=nullptr;}
  image.unload();
}
void ModuleActivity::onExit(){
  shutdown();
  if(runtime && runtime->wifiOwned && !devmode::holdsRadio()){WiFi.disconnect(false);WiFi.mode(WIFI_OFF);}
  runtime.reset();Activity::onExit();
}
void ModuleActivity::prepareForSleep(){shutdown();}
bool ModuleActivity::preventAutoSleep(){return runtime && runtime->busy;}
bool ModuleActivity::handleHomeGesture(){cancel();leaving=false;return false;}
bool ModuleActivity::storagePath(const char* name,char* out,size_t capacity) const {
  const size_t n=boundedLength(name,33);
  if(!name || !n || n>32 || name[0]=='.' || strstr(name,".."))return false;
  for(size_t i=0;i<n;++i)if(!((name[i]>='a'&&name[i]<='z')||(name[i]>='0'&&name[i]<='9')||name[i]=='.'||name[i]=='-'||name[i]=='_'))return false;
  const int used=snprintf(out,capacity,"%s/%s/data/%s",root,package.id,name);
  return used>0 && static_cast<size_t>(used)<capacity;
}
uint32_t ModuleActivity::parseHtml(const char* url,const char* bytes,size_t length) {
  if(!url || !bytes || length>32768 || runtime->nextDocument==UINT32_MAX)return 0;
  Runtime::Document* slot=nullptr;
  for(auto& d:runtime->docs)if(!d.value){slot=&d;break;}
  if(!slot)return 0;
  auto result=stickyweb::parseHtml(std::string_view(bytes,length),url);
  if(!result.ok)return 0;
  const size_t bytesUsed=documentBytes(result.document);
  if(bytesUsed>DOCUMENT_CAP-runtime->documentBytes)return 0;
  auto document=makeUniqueNoThrow<stickyweb::Document>(std::move(result.document));
  if(!document){LOG_ERR("SDAPP","OOM: parsed document");return 0;}
  slot->value=std::move(document);slot->bytes=bytesUsed;slot->id=++runtime->nextDocument;
  runtime->documentBytes+=bytesUsed;return slot->id;
}
void ModuleActivity::releaseDocument(uint32_t id) {
  for(auto& d:runtime->docs)if(d.value && d.id==id){runtime->documentBytes-=d.bytes;d.value.reset();d.bytes=0;d.id=0;return;}
}
void ModuleActivity::bindServices() {
  host.size=package.abi==2?sizeof(host2):sizeof(host);host.version=package.abi;host.context=this;
  host2.present_v2=[](void* p,const CpPresentation* v){return static_cast<ModuleActivity*>(p)->presentV2(v);};
  host.allocate=[](void* p,size_t n)->void* {
    auto& r=*static_cast<ModuleActivity*>(p)->runtime;
    if(!n || n>MODULE_HEAP_CAP-r.allocated)return nullptr;
    for(auto& a:r.allocations)if(!a.pointer){
      auto memory=HalMemory::allocatePsram(n);
      if(!memory){LOG_ERR("SDAPP","OOM: module %u bytes",unsigned(n));return nullptr;}
      a.pointer=memory.release();a.size=n;r.allocated+=n;return a.pointer;
    }
    LOG_ERR("SDAPP","Module allocation slots exhausted");return nullptr;
  };
  host.release=[](void* p,void* memory){
    auto& r=*static_cast<ModuleActivity*>(p)->runtime;
    for(auto& a:r.allocations)if(a.pointer==memory && memory){r.allocated-=a.size;HalMemory::PsramDeleter{}(a.pointer);a={};return;}
  };
  host.present=[](void* p,const CpView* v){return static_cast<ModuleActivity*>(p)->present(v);};
  host.edit=[](void* p,int32_t id,const char* title,const char* text,size_t max){return static_cast<ModuleActivity*>(p)->queueEdit(id,title,text,max);};
  host.request=[](void* p,int32_t id,CpRequestKind kind,const char* url,const uint8_t* bytes,size_t n){return static_cast<ModuleActivity*>(p)->queueRequest(id,kind,url,bytes,n);};
  host.cancel=[](void* p){static_cast<ModuleActivity*>(p)->cancel();};
  host.record=[](void* p,int32_t id,int start){return static_cast<ModuleActivity*>(p)->queueRecord(id,start);};
  host.wifi=[](void* p){auto& r=*static_cast<ModuleActivity*>(p)->runtime;if(r.busy||r.child!=Runtime::Child::None)return 0;r.child=Runtime::Child::Wifi;return 1;};
  host.key_settings=[](void* p){auto& r=*static_cast<ModuleActivity*>(p)->runtime;if(r.busy||r.child!=Runtime::Child::None)return 0;r.child=Runtime::Child::Key;return 1;};
  host.milliseconds=[](void*){return static_cast<uint32_t>(millis());};
  host.leave=[](void* p){auto* self=static_cast<ModuleActivity*>(p);self->cancel();self->leaving=true;};
  host.read_file=[](void* p,const char* name,void* bytes,size_t cap,size_t* length){
    if(!bytes||!length||cap>128*1024)return 0;*length=0;
    char path[96];if(!static_cast<ModuleActivity*>(p)->storagePath(name,path,sizeof(path)))return 0;
    HalFile file;if(!Storage.openFileForRead("SDAPP",path,file)||file.isDirectory())return 0;
    const auto n=file.fileSize64();if(n>cap || file.read(bytes,n)!=static_cast<int>(n))return 0;
    *length=n;return 1;
  };
  host.write_file=[](void* p,const char* name,const void* bytes,size_t n){
    if(!bytes||n>128*1024)return 0;auto* self=static_cast<ModuleActivity*>(p);
    char path[96],temporary[104];if(!self->storagePath(name,path,sizeof(path)))return 0;
    snprintf(temporary,sizeof(temporary),"%s/%s/data",self->root,self->package.id);
    if(!Storage.ensureDirectoryExists(temporary))return 0;
    snprintf(temporary,sizeof(temporary),"%s.new",path);
    {HalFile file;if(!Storage.openFileForWrite("SDAPP",temporary,file)||file.write(static_cast<const uint8_t*>(bytes),n)!=n)return 0;}
    return Storage.replaceFile(temporary,path)?1:0;
  };
  host.parse_html=[](void* p,const char* url,const char* html,size_t n){return static_cast<ModuleActivity*>(p)->parseHtml(url,html,n);};
  host.document_field=[](void* p,uint32_t id,uint32_t field,uint32_t index)->const char* {
    for(const auto& d:static_cast<ModuleActivity*>(p)->runtime->docs)if(d.value && d.id==id){
      switch(field){case 0:return d.value->title.c_str();case 1:return d.value->text.c_str();
        case 2:return index<d.value->links.size()?d.value->links[index].label.c_str():"";
        case 3:return index<d.value->links.size()?d.value->links[index].url.c_str():"";default:return "";}
    }return "";
  };
  host.document_links=[](void* p,uint32_t id)->uint32_t{for(const auto& d:static_cast<ModuleActivity*>(p)->runtime->docs)if(d.value&&d.id==id)return d.value->links.size();return 0;};
  host.release_document=[](void* p,uint32_t id){static_cast<ModuleActivity*>(p)->releaseDocument(id);};
  host.resolve_url=[](void*,const char* base,const char* reference,char* out,size_t cap){
    if(!base||!reference||!out||!cap||cap>MODULE_HEAP_CAP || boundedLength(base,2049)>2048 || boundedLength(reference,2049)>2048)return 0;
    const auto url=stickyweb::resolveUrl(base,reference);stickyweb::Destination destination;
    return stickyweb::parseDestination(url,destination)&&copyText(out,cap,url.c_str())?1:0;
  };
  host.cache_list=[](void*,char* out,size_t cap){
    if(!out||!cap||cap>256*2049+1)return 0;size_t used=0;out[0]=0;
    const auto urls=stickyweb::deviceCache().list();
    for(const auto& url:urls){if(url.size()+1>=cap-used)return 0;memcpy(out+used,url.data(),url.size());used+=url.size();out[used++]='\n';}
    out[used]=0;return 1;
  };
  host.cache_read=[](void*,const char* url,char* out,size_t cap,size_t* length){
    if(!url||!out||!length)return 0;stickyweb::CachePage page;
    if(!stickyweb::deviceCache().load(url,page)||page.html.size()>=cap)return 0;
    memcpy(out,page.html.data(),page.html.size());out[page.html.size()]=0;*length=page.html.size();return 1;
  };
  host.cache_write=[](void* p,const char* url,const char* html,size_t length,const char* protectedUrl){
    if(!url||!html||!protectedUrl||length>32768)return 0;
    auto* self=static_cast<ModuleActivity*>(p);
    return stickyweb::deviceCache().save(url,std::string(html,length),trustedtime::trustedNow(),protectedUrl,
      [self]{return self->runtime->cancelled.load();})?1:0;
  };
  host.cache_clear=[](void*){return stickyweb::deviceCache().clear()?1:0;};
}
int ModuleActivity::queueEdit(int32_t id,const char* title,const char* initial,size_t max) {
  auto& r=*runtime;if(r.busy||r.child!=Runtime::Child::None||max>8192)return 0;
  if(!copyText(r.editTitle,sizeof(r.editTitle),title)||!copyText(r.editText,sizeof(r.editText),initial)||strlen(r.editText)>max)return 0;
  r.editId=id;r.editLimit=max;r.child=Runtime::Child::Edit;return 1;
}
int ModuleActivity::queueRequest(int32_t id,CpRequestKind kind,const char* url,const uint8_t* bytes,size_t length) {
  auto& r=*runtime;if(r.busy||r.child!=Runtime::Child::None||WiFi.status()!=WL_CONNECTED||id<=0)return 0;
  if(kind==CP_GET){
    stickyweb::Destination destination;
    if(!url||!copyText(r.url,sizeof(r.url),url)||!stickyweb::parseDestination(url,destination))return 0;
    if(r.lastGetId!=id){r.lastGetId=id;r.getStarted=millis();}
  }else if(kind==CP_PROVIDER_CHAT){
    if(!bytes||!length||length>32768||!stickychat::loadProviderKey(r.key))return 0;
    memcpy(r.requestBytes.get(),bytes,length);r.requestBytes.get()[length]=0;
  }else return 0;
  r.requestLength=length;r.requestKind=kind;r.jobId=id;r.job=Runtime::Job::Request;
  r.cancelled=false;r.stopRecording=false;r.done=false;r.busy=true;r.pending=true;return 1;
}
int ModuleActivity::queueRecord(int32_t id,int begin) {
  auto& r=*runtime;
  if(!begin){if(r.busy && r.job==Runtime::Job::Capture && r.jobId==id){r.stopRecording=true;return 1;}return 0;}
  if(r.busy||r.child!=Runtime::Child::None||WiFi.status()!=WL_CONNECTED||!stickychat::loadProviderKey(r.key))return 0;
  r.wav=HalMemory::allocatePsram(stickychat::kWavMax);
  if(!r.wav){LOG_ERR("SDAPP","OOM: %u-byte PSRAM-only WAV",unsigned(stickychat::kWavMax));return 0;}
  r.job=Runtime::Job::Capture;r.jobId=id;r.cancelled=false;r.stopRecording=false;r.done=false;r.busy=true;r.pending=true;return 1;
}
void ModuleActivity::startJob() {
  auto& r=*runtime;r.pending=false;r.result.clear();r.location.clear();r.resultStatus=-1;
  if(r.cancelled){r.busy=false;r.wav.reset();return;}
  if(xTaskCreate([](void* p){auto* self=static_cast<ModuleActivity*>(p);self->work();self->runtime->done.store(true,std::memory_order_release);vTaskDelete(nullptr);},
     "SdAppService",8192,this,1,&r.task)!=pdPASS){
    LOG_ERR("SDAPP","Could not create service task");r.task=nullptr;r.result="Could not start network task";r.done=true;
  }
}
void ModuleActivity::work() {
  auto& r=*runtime;if(r.cancelled)return;
  if(r.job==Runtime::Job::Capture){
    if(r.stopRecording){r.result="Recording released before capture started";return;}
    stickychat::Capture capture(r.microphone);
    if(!capture.start(r.wav.get(),stickychat::kWavMax,millis())){r.result="Microphone unavailable";return;}
    while(capture.recording()&&!r.cancelled&&!r.stopRecording){capture.tick(millis());vTaskDelay(pdMS_TO_TICKS(1));}
    if(r.cancelled){capture.cancel();return;}capture.stop();
    if(!capture.size()){r.result="No audio captured";return;}
    auto response=stickychat::postProvider(r.key,stickychat::Route::Transcribe,r.wav.get(),capture.size(),r.cancelled);
    if(!response.ok){r.result=response.error;return;}
    auto envelope=stickychat::parseProviderEnvelope(response.body,true);
    if(!envelope.ok){r.result="Invalid transcription response";return;}
    r.result=std::move(envelope.text);r.resultStatus=200;return;
  }
  if(r.requestKind==CP_GET){
    const uint32_t elapsed=millis()-r.getStarted;
    if(elapsed>=15000){r.result="Navigation timed out";return;}
    auto response=stickyweb::fetchOnce(r.url,[&r]{return r.cancelled.load();},15000-elapsed);
    r.resultStatus=response.ok?response.status:-1;r.location=std::move(response.location);
    r.result=response.ok?std::move(response.body):std::move(response.error);return;
  }
  auto response=stickychat::postProvider(r.key,stickychat::Route::Chat,r.requestBytes.get(),r.requestLength,r.cancelled);
  if(!response.ok){r.result=response.error;return;}
  auto envelope=stickychat::parseProviderEnvelope(response.body,false);
  if(!envelope.ok){r.result="Invalid provider response";return;}
  r.result=std::move(envelope.text);r.resultStatus=200;
}
void ModuleActivity::openChild() {
  auto& r=*runtime;const auto child=r.child;r.child=Runtime::Child::None;
  switch(child){
    case Runtime::Child::None:return;
    case Runtime::Child::Edit:{
      auto keyboard=makeUniqueNoThrow<KeyboardEntryActivity>(renderer,mappedInput,r.editTitle,r.editText,r.editLimit,InputType::Text);
      const int32_t id=r.editId;
      if(!keyboard){LOG_ERR("SDAPP","OOM: module keyboard");emit(CP_EDITED,id,-1);return;}
      startActivityForResult(std::move(keyboard),[this,id](const ActivityResult& result){
        const auto* value=std::get_if<KeyboardResult>(&result.data);
        emit(CP_EDITED,id,result.isCancelled||!value?-1:0,value?value->text.c_str():nullptr,value?value->text.size():0);
      });return;
    }
    case Runtime::Child::Wifi:{
      auto wifi=makeUniqueNoThrow<WifiSelectionActivity>(renderer,mappedInput);
      if(!wifi){LOG_ERR("SDAPP","OOM: module Wi-Fi picker");return;}
      r.wifiOwned=r.wifiOwned||(WiFi.getMode()==WIFI_MODE_NULL);WiFi.mode(WIFI_STA);
      startActivityForResult(std::move(wifi),[this](const ActivityResult&){emit(CP_WIFI_DONE,0,WiFi.status()==WL_CONNECTED?1:0);});return;
    }
    case Runtime::Child::Key:{
      auto settings=makeUniqueNoThrow<KeySettingsActivity>(renderer,mappedInput);
      if(!settings){LOG_ERR("SDAPP","OOM: module key settings");return;}
      startActivityForResult(std::move(settings),nullptr);return;
    }
  }
}
void ModuleActivity::loop() {
  if(leaving){shelf::leave(renderer,mappedInput);return;}
  if(!runtime||!runtime->model){leaving=true;return;}
  auto& r=*runtime;
  int x=0,y=0;
  // Release is consumed before scheduling capture, including a press/release
  // pair whose worker has not started yet.
  if(r.hold){
    const bool held=r.touchHold?mappedInput.isScreenTouchHeld(x,y):mappedInput.isPressed(MappedInputManager::Button::Confirm);
    if(!held){r.hold=false;emit(CP_HOLD_END,r.holdId);return;}
    if(r.touchHold && r.design && r.design->valid){
      fui::Interaction hit;bool inside;
      {RenderLock lock;inside=app.hitPublished(x,y,2,hit)&&hit.value==static_cast<int16_t>(presentation::Control::ComposeAction);}
      if(!inside){r.hold=false;mappedInput.swallowCurrentTouch();
        if(r.busy && r.job==Runtime::Job::Capture && r.jobId==r.holdId)emit(CP_BACK);return;}
    }
  }
  if(mappedInput.wasReleased(MappedInputManager::Button::Back)){
    r.hold=false;if(started)emit(CP_BACK);else leaving=true;return;
  }
  if(r.busy&&r.done.load(std::memory_order_acquire)){
    r.task=nullptr;r.busy=false;r.wav.reset();
    std::fill(r.key.begin(),r.key.end(),'\0');r.key.clear();
    if(!r.cancelled)emit(CP_HTTP_DONE,r.jobId,r.resultStatus,r.result.c_str(),r.result.size(),r.location.c_str());
    return;
  }
  if(r.pending){startJob();return;}
  if(r.child!=Runtime::Child::None){openChild();return;}
  if(!started||r.hold)return;
  if(r.design && r.design->valid){
    auto& d=*r.design;
    if(mappedInput.wasScreenTouchPressed(x,y)){
      fui::Interaction hit;RenderLock lock;
      r.bodyGesture=routingReady() && app.hitPublished(x,y,2,hit) && hit.value==static_cast<int16_t>(presentation::Control::Body);
    }
    if(!r.busy && !d.draft[0] && d.binding.hold &&
       (mappedInput.wasPressed(MappedInputManager::Button::Confirm) || mappedInput.wasScreenTouchPressed(x,y))){
      fui::Interaction hit;
      const bool physical=mappedInput.wasPressed(MappedInputManager::Button::Confirm);
      bool voice=false;
      {RenderLock lock;voice=routingReady() && (physical || (app.hitPublished(x,y,2,hit)&&hit.value==static_cast<int16_t>(presentation::Control::ComposeAction)));}
      if(voice){r.hold=true;r.touchHold=!physical;r.holdId=d.binding.hold;emit(CP_HOLD_START,r.holdId);return;}
    }
    if(!r.busy && d.draft[0] && mappedInput.wasReleased(MappedInputManager::Button::Confirm)){
      emit(CP_SELECT,d.binding.send);return;
    }
    const auto routed=routeTouch(mappedInput);
    if(routed && routed.event.action==2){
      int32_t id=0;const auto value=static_cast<presentation::Control>(routed.event.value);
      {RenderLock lock;
        switch(value){
          case presentation::Control::Back:id=d.binding.back;break;
          case presentation::Control::Address:id=d.binding.address_action;break;
          case presentation::Control::Menu:id=d.binding.menu;break;
          case presentation::Control::NewChat:id=d.binding.new_chat;break;
          case presentation::Control::Edit:id=d.binding.edit;break;
          case presentation::Control::ComposeAction:id=d.draft[0]?d.binding.send:0;break;
          case presentation::Control::Body:for(uint32_t i=0;i<d.hitCount;++i)if(d.hits[i].bounds.contains(routed.snap.touchX,routed.snap.touchY)){id=d.hits[i].action;break;}break;
        }
      }
      if(r.busy){if(value==presentation::Control::Back || value==presentation::Control::NewChat)emit(CP_BACK);}
      else if(id)emit(CP_SELECT,id);
      return;
    }
    const auto swipe=mappedInput.wasSwipe();
    const bool next=(r.bodyGesture && swipe==MappedInputManager::SwipeDir::Up)||mappedInput.wasReleased(MappedInputManager::Button::Down)||mappedInput.wasReleased(MappedInputManager::Button::PageForward);
    const bool prev=(r.bodyGesture && swipe==MappedInputManager::SwipeDir::Down)||mappedInput.wasReleased(MappedInputManager::Button::Up)||mappedInput.wasReleased(MappedInputManager::Button::PageBack);
    if(swipe!=MappedInputManager::SwipeDir::None || mappedInput.wasScreenTouchReleased())r.bodyGesture=false;
    if(!r.busy && (next||prev)){RenderLock lock;if(next && d.page+1<d.pageCount)++d.page;else if(prev && d.page)--d.page;resetUi();requestUpdate();}
    return;
  }
  if(!r.busy && mappedInput.wasScreenTouchPressed(x,y)){
    fui::Interaction hit;bool record=false;
    {RenderLock lock;record=routingReady()&&app.hitPublished(x,y,1,hit)&&hit.value>=0&&
       static_cast<uint32_t>(hit.value)<r.model->count&&(r.model->flags[hit.value]&1);}
    if(record){r.hold=true;r.touchHold=true;r.holdId=r.model->ids[hit.value];emit(CP_HOLD_START,r.holdId);return;}
  }
  if(!r.busy && mappedInput.wasPressed(MappedInputManager::Button::Confirm) && r.nav.selected>=0 &&
     static_cast<uint32_t>(r.nav.selected)<r.model->count && (r.model->flags[r.nav.selected]&1)){
    r.hold=true;r.touchHold=false;r.holdId=r.model->ids[r.nav.selected];emit(CP_HOLD_START,r.holdId);return;
  }
  if(r.model->kind!=CP_READING && mappedInput.wasReleased(MappedInputManager::Button::Confirm)){activate(r.nav.selected);return;}
  if(r.model->kind!=CP_READING && routeTouch(mappedInput))return;
  const auto swipe=mappedInput.wasSwipe();
  const bool next=swipe==MappedInputManager::SwipeDir::Up||mappedInput.wasReleased(MappedInputManager::Button::Down)||mappedInput.wasReleased(MappedInputManager::Button::PageForward);
  const bool previous=swipe==MappedInputManager::SwipeDir::Down||mappedInput.wasReleased(MappedInputManager::Button::Up)||mappedInput.wasReleased(MappedInputManager::Button::PageBack);
  if(next||previous){
    RenderLock lock;
    if(r.model->kind==CP_READING){
      const uint32_t step=std::max<uint32_t>(1,r.visibleLines);
      if(next && r.topLine+step<r.lineCount)r.topLine+=step;
      else if(previous)r.topLine=r.topLine>step?r.topLine-step:0;
    }else{
      const bool page=swipe==MappedInputManager::SwipeDir::Up||swipe==MappedInputManager::SwipeDir::Down;
      const int step=page?std::max(1,r.nav.inputPageRows()):1;
      r.nav.requestSelection(std::clamp(r.nav.selected+(next?step:-step),0,std::max(0,int(r.model->count)-1)));
    }
    requestUpdate();
  }
}
int ModuleActivity::presentV2(const CpPresentation* v) {
  if(!runtime || !runtime->design || !presentation::validPresentation(v))return 0;
  const stickyweb::Document* document=nullptr;
  if(v->kind==CP_BROWSER_V2 && v->document){
    for(const auto& d:runtime->docs)if(d.id==v->document && d.value)document=d.value.get();
    if(!document || document->title.size()+document->text.size()+2>presentation::TEXT_CAP || document->spans.size()>CP_APP_MAX_SPANS)return 0;
  }
  RenderLock lock;auto& d=*runtime->design;closeRouting();runtime->bodyGesture=false;
  const bool changed=d.identity!=v->identity || d.binding.kind!=v->kind;
  d.binding=*v;d.binding.messages=nullptr;d.binding.title=d.binding.address=d.binding.status=d.binding.draft=nullptr;
  d.identity=v->identity;copyText(d.title,sizeof(d.title),v->title);copyText(d.address,sizeof(d.address),v->address);
  copyText(d.status,sizeof(d.status),v->status);copyText(d.draft,sizeof(d.draft),v->draft);
  d.length=d.runCount=d.spanCount=0;
  if(document){
    const uint32_t title=document->title.size();
    memcpy(d.text,document->title.data(),title);d.text[title]='\n';d.text[title+1]='\n';
    const uint32_t body=title+2;
    memcpy(d.text+body,document->text.data(),document->text.size());d.length=body+document->text.size();
    d.runs[d.runCount++]={0,body,2};
    for(const auto& span:document->spans)d.spans[d.spanCount++]={body+span.start,body+span.end,v->link_base+int32_t(span.link)};
  }else if(v->kind==CP_CHAT_V2){
    for(uint32_t i=0;i<v->message_count;++i){const auto& message=v->messages[i];const uint32_t start=d.length;
      memcpy(d.text+d.length,message.text,message.length);d.length+=message.length;d.text[d.length++]='\n';
      d.runs[d.runCount++]={start,d.length,uint8_t(message.role)};d.text[d.length++]='\n';
    }
  }
  d.text[d.length]=0;
  uint32_t hash=2166136261u;
  for(uint32_t i=0;i<d.length;++i)hash=(hash^uint8_t(d.text[i]))*16777619u;
  d.dirty=d.dirty || changed || hash!=d.contentHash;d.contentHash=hash;d.valid=true;
  d.followTail=changed && v->kind==CP_CHAT_V2;
  if(changed){d.page=0;d.pages[0]=0;}
  resetUi();requestUpdate();return 1;
}
void ModuleActivity::buildPresentation(UiScreen& screen) {
  HeaderBackTapTarget::clear();
  auto& target=screen.target();const auto safe=UITheme::getContentArea(renderer);
  presentation::drawScreen(*runtime->design,target,screen.theme(),presentation::rect(safe.x,safe.y,safe.width,safe.height),
    tr(STR_SD_ASK_ANYTHING),tr(STR_SD_SEND),[&](fui::Rect bounds,presentation::Control value){screen.frame().hit(bounds,2,static_cast<int16_t>(value),fui::InputTouch,fui::StateNormal);});
}

void ModuleActivity::buildScreen(UiScreen& screen) {
  if(!runtime||!runtime->model)return;
  auto& r=*runtime;auto& m=*r.model;
  if(r.design && r.design->valid){buildPresentation(screen);return;}
  const auto& metrics=UITheme::getInstance().getMetrics();const auto safe=UITheme::getInstance().getScreenSafeArea(renderer,true,false);
  screen.setContentMarginFromScreen({static_cast<int16_t>(safe.y+metrics.topPadding+metrics.headerHeight),
    static_cast<int16_t>(renderer.getScreenWidth()-safe.x-safe.width),
    static_cast<int16_t>(renderer.getScreenHeight()-safe.y-safe.height+metrics.buttonHintsHeight),static_cast<int16_t>(safe.x)});
  auto rect=screen.contentRect();
  if(m.kind==CP_READING){
    auto style=screen.theme().smallText;style.maxLines=0;
    r.lineCount=r.wrapped.lineCount(screen.target(),rect.width,m.text,style);
    const int height=std::max<int>(1,screen.target().lineHeight(style.font));
    r.visibleLines=std::max(1,int(rect.height)/height);
    r.wrapped.draw(screen.target(),rect,m.text,style,r.topLine);return;
  }
  if(m.kind==CP_CHAT && m.count>=9){
    const auto layout=stickychat::chatLayout(rect.height);const int gap=layout.gap;
    fui::ButtonProps props{};props.action=1;props.inputMask=fui::InputTouch;props.radius=12;
    props.text=screen.theme().smallText;props.text.maxLines=3;props.minTouchSize=0;
    const auto button=[&](int index,const char* label,int x,int y,int w,int h){
      props.value=index;props.label=label;props.state=r.nav.selected==index?fui::StateSelected:fui::StateNormal;
      fui::button(screen.frame(),{static_cast<int16_t>(x),static_cast<int16_t>(y),static_cast<int16_t>(w),static_cast<int16_t>(h)},props);
    };
    int y=rect.y;
    button(3,m.previewYou,rect.x+24,y,rect.width-24,layout.bubble);y+=layout.bubble+gap;
    button(3,m.previewAssistant,rect.x,y,rect.width-24,layout.bubble);y+=layout.bubble+gap;
    button(0,m.previewDraft,rect.x,y,rect.width*2/3-gap,layout.button);
    button(1,m.labels[1],rect.x+rect.width*2/3,y,rect.width/3,layout.button);y+=layout.button+gap;
    button(2,m.labels[2],rect.x,y,rect.width,layout.button);y+=layout.button+gap;
    for(int i=0;i<5;++i)button(i+4,m.labels[i+4],rect.x+i*rect.width/5,y,rect.width/5-gap,layout.button);
    y+=layout.button+gap;
    auto style=screen.theme().smallText;style.maxLines=1;
    screen.target().text({rect.x,static_cast<int16_t>(y),rect.width,static_cast<int16_t>(layout.status)},m.status,style);return;
  }
  auto style=screen.theme().smallText;style.maxLines=2;
  const int statusHeight=std::min<int>(48,rect.height/5);
  screen.target().text({rect.x,rect.y,rect.width,static_cast<int16_t>(statusHeight)},m.status,style);
  screen.setContentMarginFromScreen({static_cast<int16_t>(safe.y+metrics.topPadding+metrics.headerHeight+statusHeight),
    static_cast<int16_t>(renderer.getScreenWidth()-safe.x-safe.width),
    static_cast<int16_t>(renderer.getScreenHeight()-safe.y-safe.height+metrics.buttonHintsHeight),static_cast<int16_t>(safe.x)});
  fui::ListProps props{};props.items=m.items;props.count=m.count;props.action=1;props.inputMask=fui::InputTouch;
  screen.syncListViewport(r.nav,props,m.count);screen.list(props);
}
void ModuleActivity::render(RenderLock&&) {
  renderer.clearScreen();
  if(runtime&&runtime->model){
    const auto& m=UITheme::getInstance().getMetrics();if(!runtime->design || !runtime->design->valid)GUI.drawHeader(renderer,Rect{0,m.topPadding,renderer.getScreenWidth(),m.headerHeight},runtime->model->title);
    renderUi();
    for(int pass=0;runtime->nav.consumeRebuildNeeded()&&pass<8;++pass){renderer.clearScreen();if(!runtime->design || !runtime->design->valid)GUI.drawHeader(renderer,Rect{0,m.topPadding,renderer.getScreenWidth(),m.headerHeight},runtime->model->title);renderUi();}
  }
  if(!runtime || !runtime->design || !runtime->design->valid){const auto labels=mappedInput.mapLabels("Back","Select","Up","Down");GUI.drawButtonHints(renderer,labels.btn1,labels.btn2,labels.btn3,labels.btn4);}
  renderer.displayBuffer();
}
}
#endif
