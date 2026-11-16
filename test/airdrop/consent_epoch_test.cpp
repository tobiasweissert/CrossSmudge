
#include <FreeInkUI.h>
#include <FreeInkApp.h>
#include <algorithm>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include "network/AirDropReceiver.h"
#include "ui_render_strings.inc"
#if __has_include("components/AirDropConsentInput.h")
#include "components/AirDropConsentInput.h"
#define HAS_CONSENT_EPOCH 1
#endif
namespace fui=freeink::ui;
constexpr fui::ActionId Accept=1,Decline=2,Cancel=3,Retry=4;
constexpr uint64_t PaintIntervalMs=1200;
#define LOG_ERR(...) ((void)0)
#define LOG_INF(...) ((void)0)
namespace CrossPointSettings {enum class SHORT_PWRBTN {SLEEP};}
static std::mutex renderMutex,paintMutex;
static std::condition_variable paintCv;
static bool blockPaint=false,displayStarted=false,finishDisplay=false;
static std::atomic<bool> waitingForRender{false},delayHandler{false},handlerQueued{false},deliverHandler{false};
struct RenderLock {template<class T>RenderLock(T&){waitingForRender.store(true);renderMutex.lock();waitingForRender.store(false);}~RenderLock(){renderMutex.unlock();}};
struct GfxRenderer {
 int width=480,height=800;void clearScreen(){}
 void displayBuffer(int){std::unique_lock<std::mutex> l(paintMutex);if(blockPaint){displayStarted=true;paintCv.notify_all();paintCv.wait(l,[]{return finishDisplay;});}}
};
static void (*duringPoll)() = nullptr;
struct MappedInputManager {
 enum class Button {Back,Confirm};enum class SwipeDir {None};
 mutable bool down=false,release=false,contact=false,confirm=false,confirmEdge=false,back=false,home=false;
 mutable int x=0,y=0;
 struct GPIO{void update()const{if(duringPoll)duringPoll();}}gpio;
 void update()const;void expireReleaseSuppressions()const{}
 bool wasScreenLongPress(int&,int&)const{return false;}
 bool isScreenTouchHeld(int&tx,int&ty)const{tx=x;ty=y;return contact;}
 bool wasScreenTouchDown(int&tx,int&ty)const{tx=x;ty=y;return down;}
 bool wasScreenTapped(int&tx,int&ty)const{tx=x;ty=y;return release;}
 bool wasScreenTouchReleased()const{return release;}
 bool isScreenContactActive()const{return contact;}
 SwipeDir wasSwipe()const{return SwipeDir::None;}
 bool wasHomeGesture()const{return home;}
 bool wasPressed(Button b)const{return b==Button::Back?back:confirmEdge;}
 bool isPhysicalPressed(Button b)const{return b==Button::Confirm&&confirm;}
 void suppressNextConfirmRelease(){}void suppressNextBackRelease(){}
 struct Labels{const char*btn1="",*btn2="",*btn3="",*btn4="";};
 Labels mapLabels(const char*,const char*,const char*,const char*)const{return {};}
};
struct HeaderRect {int x,y,width,height;};struct Metrics {int contentSidePadding=24,buttonHintsHeight=40;};
struct UITheme {Metrics metrics;const Metrics&getMetrics()const{return metrics;}static UITheme&getInstance(){static UITheme t;return t;}};
namespace TouchHeaderBackButton {
 HeaderRect headerRect(const GfxRenderer&r,const MappedInputManager&){return {0,0,r.width,60};}
 template<class T>void draw(GfxRenderer&,T&,HeaderRect,const char*,bool){}
 bool wasTapped(const MappedInputManager&,const GfxRenderer&){return false;}
}
struct Gui{void drawButtonHints(GfxRenderer&,const char*,const char*,const char*,const char*){}}GUI;
struct Refresh{int modeFor(uint8_t){return 0;}};
static int decisions=0,stops=0;static bool accepted=false,stopped=false;static airdrop::OfferIdentity delivered{};static airdrop::Status supplied;
namespace airdrop {
 Receiver&Receiver::instance(){static Receiver v;return v;}
 bool Receiver::decide(const OfferIdentity&id,bool accept){++decisions;accepted=accept;delivered=id;return true;}
 void Receiver::snapshot(Status&out)const{out=supplied;}bool Receiver::stopped()const{return ::stopped;}void Receiver::reap(){}
 void Receiver::requestStop(){++stops;}
}
struct Manager{void returnFromAirDrop(const std::string&,bool){}}activityManager;
static std::string escape(const char*s){std::string out;for(;*s;++s){switch(*s){case '&':out+="&amp;";break;case '<':out+="&lt;";break;case '>':out+="&gt;";break;case '"':out+="&quot;";break;default:out+=*s;}}return out;}
struct SvgTarget:fui::DrawTarget {
 int small=24,body=24;std::ostringstream svg;
 std::string requiredHost,requiredFilename;bool hostComplete=false,filenameComplete=false,deadlineComplete=false;int textCount=0;
 int16_t lineHeight(fui::FontId f)const override{return static_cast<int16_t>(f==0?small:body);}
 fui::Size measureText(fui::FontId f,const char*s,fui::TextStyle)const override{
  // Count UTF-8 glyph starts. Stable declared host metric, no claim of firmware kerning.
  int n=0;for(const unsigned char*p=reinterpret_cast<const unsigned char*>(s);*p;++p)if((*p&0xc0)!=0x80)++n;
  return {static_cast<int16_t>(n*lineHeight(f)/2),lineHeight(f)};
 }
 void fill(fui::Rect r,fui::Paint p,uint8_t radius,uint8_t)override{
  if(p.kind==fui::PaintKind::None)return;
  svg<<"<rect x='"<<r.x<<"' y='"<<r.y<<"' width='"<<r.width<<"' height='"<<r.height<<"' rx='"<<int(radius)<<"' fill='"<<(p.color==fui::Color::White?"white":"black")<<"'/>\n";
 }
 void stroke(fui::Rect r,fui::Paint p,uint8_t width,uint8_t radius,uint8_t)override{
  if(p.kind==fui::PaintKind::None)return;
  svg<<"<rect x='"<<r.x<<"' y='"<<r.y<<"' width='"<<r.width<<"' height='"<<r.height<<"' rx='"<<int(radius)<<"' fill='none' stroke='black' stroke-width='"<<int(width)<<"'/>\n";
 }
 void line(fui::Point a,fui::Point b,uint8_t width,fui::Paint)override{svg<<"<line x1='"<<a.x<<"' y1='"<<a.y<<"' x2='"<<b.x<<"' y2='"<<b.y<<"' stroke='black' stroke-width='"<<int(width)<<"'/>\n";}
 void triangle(fui::Point,fui::Point,fui::Point,fui::Paint)override{}
 void bitmap(fui::Rect,fui::BitmapRef,fui::BitmapMode,fui::Paint,fui::Rotation)override{}
 void text(fui::Rect r,const char*value,fui::TextStyle style)override{
  ++textCount;std::string shown;
  fui::layoutText(*this,r,value,style,[&](const char*line,fui::Rect at){
   shown+=line;
   svg<<"<text x='"<<at.x<<"' y='"<<(at.y+lineHeight(style.font)*4/5)<<"' font-family='monospace' font-size='"<<lineHeight(style.font)<<"' font-weight='"<<(style.bold?"bold":"normal")<<"' fill='"<<(style.inverted?"white":"black")<<"'>"<<escape(line)<<"</text>\n";
  });
  if(requiredFilename==value)filenameComplete=shown==requiredFilename;
  if(std::strncmp(value,"Decide within",13)==0)deadlineComplete=shown==value;
  if(requiredHost==value){hostComplete=shown==requiredHost;if(!hostComplete)std::fprintf(stderr,"host clipped h=%d maxLines=%u shown=%zu required=%zu\n",r.height,style.maxLines,shown.size(),requiredHost.size());}
 }
};

struct Ui {
 SvgTarget target;fui::FreeInkApp<4,4>app;bool ready=false;
 static fui::DeviceContext device(){fui::DeviceContext d;d.width=480;d.height=800;d.hasTouch=true;return d;}
 Ui():app(target,device()){}
 void closeRouting(){ready=false;}void render(){app.render();ready=true;}
 bool routeTouch(const MappedInputManager&input,fui::ActionEvent&event);
};
struct AirDropReceiveActivity {
 using UiApp=fui::FreeInkApp<4,4>;
 GfxRenderer renderer;MappedInputManager mappedInput;Ui ui_;Refresh refresh_;
 airdrop::Status status_{};char textBuffer_[256]{};
 bool leaving_=false,decisionSent_=false,browserFailed_=false,startFailed_=false,radioStopped_=false;
 bool childPending_=false,paintedPending_=false,powerDeferred_=false,homeRequested_=false,browserAttempted_=false;
 airdrop::OfferIdentity paintedIdentity_{},routedIdentity_{};
 uint32_t routedEpoch_=0;
#ifdef HAS_CONSENT_EPOCH
 airdrop::input::ConsentGate consentInput_;
 bool routeConsentInput();
#endif
 uint64_t lastPaintRequestMs_=0;std::string returnBookPath_;
 struct Launch{void cancel(){}};Launch* launch_=nullptr;
 unsigned updateRequests=0;
 void requestUpdate(){++updateRequests;}void resolveHandoff(bool){}void retryReceiver(){}void startBrowserHandoff(){}
 static void action(const fui::ActionEvent&,void*);
 void decide(bool);void buildScreen(UiApp::ScreenType&);void render(RenderLock&&);void loop();
 void beginLeaving(bool);bool handleHomeGesture();bool deferGlobalPowerAction(CrossPointSettings::SHORT_PWRBTN);
 static bool sameIdentity(const airdrop::OfferIdentity&,const airdrop::OfferIdentity&);
};
// beginLeaving only needs the existing optional launch cancellation seam.
#include "epoch_extracted.inc"
bool Ui::routeTouch(const MappedInputManager&input,fui::ActionEvent&event){
 if(!ready)return false;const auto snapshot=touchSnapshotFrom(input);
 if(!snapshot.touchPressed&&!snapshot.touchReleased&&!snapshot.touchHeld)return false;
 event=app.route(snapshot);return true;
}
static void configure(AirDropReceiveActivity&a){
 a.status_.state=airdrop::State::Pending;a.status_.nowMs=100;a.status_.deadlineMs=10000;a.status_.identity.offer=1;
 a.status_.offer.kind=airdrop::OfferKind::Url;std::strcpy(a.status_.offer.host,"old.example.com");
 a.ui_.app.setScreen([](AirDropReceiveActivity::UiApp::ScreenType&s,void*p){static_cast<AirDropReceiveActivity*>(p)->buildScreen(s);},&a);
 for(auto id:{Accept,Decline})a.ui_.app.on(id,[](const fui::ActionEvent&e,void*p){const auto queued=e;
  if(delayHandler.load()){handlerQueued.store(true);while(!deliverHandler.load())std::this_thread::yield();}
  AirDropReceiveActivity::action(queued,p);},&a);
}
static void paint(AirDropReceiveActivity&a){a.render(RenderLock(a));}
static fui::Rect button(AirDropReceiveActivity&a,bool accept=true){
 fui::InteractionBuffer<4> interactions;fui::InputSnapshot empty;auto device=Ui::device();
 fui::Frame<4>frame(a.ui_.target,device,empty,interactions);fui::Screen<4>screen(frame,a.ui_.app.theme());a.buildScreen(screen);
 return interactions.data()[accept?1:0].rect;
}
static void capture(AirDropReceiveActivity&a,bool down=false,bool release=false,bool contact=false,bool confirm=false,bool edge=false,bool accept=true){
 auto rect=button(a,accept);auto&i=a.mappedInput;i.x=rect.x+rect.width/2;i.y=rect.y+rect.height/2;
 i.down=down;i.release=release;i.contact=contact;i.confirm=confirm;i.confirmEdge=edge;i.update();
}
static void route(AirDropReceiveActivity&a){supplied=a.status_;a.loop();}
static void neutral(AirDropReceiveActivity&a){capture(a);route(a);}
static void tap(AirDropReceiveActivity&a,bool accept=true){capture(a,true,false,true,false,false,accept);route(a);capture(a,false,true,false,false,false,accept);route(a);}
int main(){
 // Same old-view release as retained external reviewer probe; through actual loop.
 {AirDropReceiveActivity a;configure(a);paint(a);capture(a,false,true);a.status_.identity.offer=2;paint(a);route(a);
 if(decisions){std::fputs("FAIL ADR-LIFECYCLE-001: old-view captured release accepted new painted offer via production loop/SDK/decide\n",stderr);return 1;}
 neutral(a);tap(a);assert(decisions==1&&accepted&&delivered.offer==2);}
 // Pre-paint held touch and Confirm cannot arm; release then a new sequence succeeds.
 for(bool physical:{false,true}){
  AirDropReceiveActivity a;configure(a);capture(a,!physical,false,!physical,physical,physical);paint(a);route(a);int n=decisions;
  capture(a,false,!physical,false,false,false);route(a);assert(decisions==n);
  neutral(a);
  if(physical){capture(a,false,false,false,true,true);route(a);}else tap(a,false);
  assert(decisions==n+1&&accepted==physical);
 }
 // A neutral frame captured before paint must not arm a held/new-looking edge.
 {AirDropReceiveActivity a;configure(a);capture(a);paint(a);route(a);int n=decisions;
  capture(a,true,false,true);route(a);capture(a,false,true);route(a);assert(decisions==n);
  neutral(a);tap(a);assert(decisions==n+1);}
 // Actual render holds RenderLock while physical paint is blocked. An old action
 // waiting for that lock must remain old after the new paint completes.
 {AirDropReceiveActivity a;configure(a);paint(a);neutral(a);capture(a,true,false,true);route(a);capture(a,false,true);int n=decisions;
  {std::lock_guard<std::mutex>l(paintMutex);blockPaint=true;displayStarted=false;finishDisplay=false;}
  std::thread painter([&]{paint(a);});
  {std::unique_lock<std::mutex>l(paintMutex);paintCv.wait(l,[]{return displayStarted;});}
  supplied=a.status_;std::thread router([&]{a.loop();});
  while(!waitingForRender.load())std::this_thread::yield();
  {std::lock_guard<std::mutex>l(paintMutex);finishDisplay=true;}paintCv.notify_all();
  painter.join();router.join();blockPaint=false;assert(decisions==n);
  neutral(a);tap(a);assert(decisions==n+1);}
 // SDK already routed an eligible release; its handler waits while the
 // renderer completes another paint. Final delivery must reject the old epoch.
 {AirDropReceiveActivity a;configure(a);paint(a);neutral(a);capture(a,true,false,true);route(a);capture(a,false,true);int n=decisions;
  delayHandler=true;handlerQueued=false;deliverHandler=false;supplied=a.status_;
  std::thread router([&]{a.loop();});while(!handlerQueued.load())std::this_thread::yield();
  paint(a);deliverHandler=true;router.join();delayHandler=false;assert(decisions==n);
  neutral(a);tap(a);assert(decisions==n+1);}
 // Capture begins before a paint completes and ends afterward: never relabel it.
#ifdef HAS_CONSENT_EPOCH
 {AirDropReceiveActivity a;configure(a);paint(a);auto token=airdrop::input::capture.begin();paint(a);airdrop::input::capture.end(token);
  int n=decisions;route(a);capture(a,false,false,false,true,true);route(a);assert(decisions==n);neutral(a);capture(a,false,false,false,true,true);route(a);assert(decisions==n+1);}
#endif
 // A paint that finishes INSIDE the actual GPIO update also invalidates
 // the capture. The epoch must be taken before polling, never at route time.
#ifdef HAS_CONSENT_EPOCH
 {AirDropReceiveActivity a;configure(a);paint(a);static AirDropReceiveActivity* polling=nullptr;polling=&a;
  duringPoll=[](){paint(*polling);};capture(a);duringPoll=nullptr;route(a);int n=decisions;
  capture(a,false,false,false,true,true);route(a);assert(decisions==n);
  neutral(a);capture(a,false,false,false,true,true);route(a);assert(decisions==n+1);}
#endif
 // Offer replacement, deadline, Back/Home and explicit power cancellation revoke.
 for(int reason=0;reason<5;++reason){AirDropReceiveActivity a;configure(a);paint(a);neutral(a);capture(a,true,false,true);route(a);int n=decisions;
  if(reason==0)++a.status_.identity.offer;
  if(reason==1)a.status_.nowMs=a.status_.deadlineMs;
  if(reason==2)a.mappedInput.back=true;
  if(reason==3)a.mappedInput.home=true;
  if(reason==4)a.deferGlobalPowerAction(CrossPointSettings::SHORT_PWRBTN::SLEEP);
  capture(a,false,true);route(a);assert(decisions==n);
 }
 // The timer must not repaint an unchanged offer and discard a touch in progress.
 {AirDropReceiveActivity a;configure(a);paint(a);neutral(a);
  capture(a,true,false,true);route(a);const auto updates=a.updateRequests;const int n=decisions;
  a.status_.nowMs=2000;capture(a,false,false,true);route(a);assert(a.updateRequests==updates);
  capture(a,false,true);route(a);assert(decisions==n+1&&accepted);
 }
 // Short input sequences may complete between main-loop polls.
 for(int edge=0;edge<3;++edge){AirDropReceiveActivity a;configure(a);paint(a);neutral(a);int n=decisions;
  if(edge==0)capture(a,false,true,false);
  if(edge==1)capture(a,true,true,false);
  if(edge==2)capture(a,false,false,false,false,true);
  route(a);assert(decisions==n+1&&accepted);
 }
 std::puts("PASS production capture/build/paint/SDK/loop/decision: queued and held inputs, concurrent paint lock, offer/epoch/deadline revocation, Back/Home/power, fresh Accept/Decline/Confirm and short taps");
}
