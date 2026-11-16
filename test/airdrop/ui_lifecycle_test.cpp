// Includes exact foreground methods extracted from the production activity.
#include <cassert>
#include "components/AirDropConsentInput.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include "network/AirDropReceiver.h"
#include <browser/BrowserLaunch.h>
#define LOG_ERR(...) ((void)0)
namespace CrossPointSettings {enum class SHORT_PWRBTN{SLEEP};}
static int lockDepth=0,decisionCalls=0,stopCalls=0,navigations=0,resolveCalls=0,disconnectCalls=0,browserCalls=0,startCalls=0;
static bool lastHome=false;
static bool isStopped=false,lastAccept=false,resolvedOpened=false;
static airdrop::Status supplied;
namespace airdrop {
Receiver& Receiver::instance(){static Receiver receiver;return receiver;}
bool Receiver::begin(){++startCalls;return true;}
void Receiver::requestStop(){++stopCalls;}
void Receiver::reap(){}
bool Receiver::stopped()const{return isStopped;}
void Receiver::snapshot(Status& out)const{out=supplied;}
bool Receiver::decide(const OfferIdentity&,bool accept){assert(lockDepth==1);++decisionCalls;lastAccept=accept;return true;}
bool Receiver::resolvePendingUrl(const OfferIdentity&,bool opened){++resolveCalls;resolvedOpened=opened;return true;}
}
struct Activity {void onExit(){}};
namespace fui {struct InputSnapshot {bool touchPressed=false,touchReleased=false;};struct ActionEvent {explicit operator bool()const{return false;}};}
struct MappedInputManager {
 enum class Button {Back,Confirm};bool home=false,back=false,confirm=false,backSuppressed=false;
 bool isPhysicalPressed(Button)const{return false;}bool isScreenContactActive()const{return false;}
 bool wasHomeGesture()const{return home;}
 bool wasPressed(Button b)const{return b==Button::Back?back:confirm;}
 void suppressNextConfirmRelease(){};void suppressNextBackRelease(){backSuppressed=true;}
};
static fui::InputSnapshot touchSnapshotFrom(const MappedInputManager&){return {};}
struct FakeUi {void closeRouting(){};bool routeTouch(const MappedInputManager&,fui::ActionEvent&){return false;}};
namespace TouchHeaderBackButton {bool wasTapped(const MappedInputManager&,int){return false;}}
struct Manager {void returnFromAirDrop(const std::string&,bool home){assert(isStopped);++navigations;lastHome=home;}}activityManager;
constexpr uint64_t PaintIntervalMs=1200;
struct AirDropReceiveActivity:Activity {
 airdrop::Status status_{};
 airdrop::OfferIdentity paintedIdentity_{};
 airdrop::browser::LaunchHandle launch_;
 FakeUi ui_;
 bool paintedPending_=false,decisionSent_=false,leaving_=false,childPending_=false,homeRequested_=false;
 bool powerDeferred_=false,wifiOwned_=false,handoffResolved_=false,radioStopped_=false;
 bool browserAttempted_=false,browserFailed_=false,startFailed_=false;
 uint64_t lastPaintRequestMs_=0;
 MappedInputManager mappedInput;int renderer=0;std::string returnBookPath_;
 int paints=0;
 void requestUpdate(){++paints;}
 static bool sameIdentity(const airdrop::OfferIdentity&,const airdrop::OfferIdentity&);
 airdrop::input::ConsentGate consentInput_;uint32_t routedEpoch_=0;airdrop::OfferIdentity routedIdentity_{};
 bool routeConsentInput();
 void decide(bool);void beginLeaving(bool);bool handleHomeGesture();void loop();void retryReceiver();
 void startBrowserHandoff(){assert(isStopped);++browserCalls;browserAttempted_=true;}
 bool deferGlobalPowerAction(CrossPointSettings::SHORT_PWRBTN);void resolveHandoff(bool);void onExit();
};
struct RenderLock {explicit RenderLock(AirDropReceiveActivity&){++lockDepth;}~RenderLock(){--lockDepth;}};
namespace HalLuaHttp {void disconnectWifi(){++disconnectCalls;}}
#include "ui_lifecycle_extracted.inc"
static AirDropReceiveActivity pending(){
 AirDropReceiveActivity a;a.status_.state=airdrop::State::Pending;a.status_.nowMs=100;a.status_.deadlineMs=200;
 a.status_.identity.generation=1;a.status_.identity.session=2;a.status_.identity.offer=3;
 a.status_.identity.connection=4;a.status_.identity.interfaceIndex=5;a.status_.identity.peer[0]=6;
 a.paintedIdentity_=a.status_.identity;a.paintedPending_=true;
 airdrop::input::capture.beginPaint();a.routedEpoch_=airdrop::input::capture.completePaint();a.consentInput_.painted(a.routedEpoch_);a.routedIdentity_=a.paintedIdentity_;return a;
}
int main(){
 auto a=pending();a.paintedPending_=false;a.decide(true);assert(decisionCalls==0);
 a=pending();a.paintedIdentity_.generation++;a.decide(true);assert(decisionCalls==0);
 a=pending();a.paintedIdentity_.offer++;a.decide(true);assert(decisionCalls==0);
 a=pending();a.paintedIdentity_.peer[0]++;a.decide(true);assert(decisionCalls==0);
 a=pending();a.status_.nowMs=a.status_.deadlineMs;a.decide(true);assert(decisionCalls==0);
 a=pending();a.leaving_=true;a.decide(true);assert(decisionCalls==0);
 a=pending();a.childPending_=true;a.decide(true);assert(decisionCalls==0);
 a=pending();a.decide(false);assert(decisionCalls==1&&!lastAccept);a.decide(true);assert(decisionCalls==1);
 a=pending();a.decide(true);assert(decisionCalls==2&&lastAccept);a.decide(true);assert(decisionCalls==2);
 // Back/Home/cancel revoke before navigation; no route or reset API is called here.
 a=pending();a.beginLeaving(false);assert(a.leaving_&&!a.homeRequested_&&stopCalls==1);a.decide(true);assert(decisionCalls==2);
 a.handleHomeGesture();assert(a.homeRequested_&&stopCalls==2&&navigations==0);
 assert(a.deferGlobalPowerAction(CrossPointSettings::SHORT_PWRBTN::SLEEP));assert(a.powerDeferred_);
 isStopped=true;assert(!a.deferGlobalPowerAction(CrossPointSettings::SHORT_PWRBTN::SLEEP));assert(!a.powerDeferred_);
 // Retained URL resolves once; activity destruction never joins or frees radio ownership.
 a=pending();a.status_.offer.kind=airdrop::OfferKind::Url;a.resolveHandoff(true);a.resolveHandoff(false);
 assert(resolveCalls==1&&resolvedOpened);
 isStopped=false;a.wifiOwned_=true;a.onExit();assert(disconnectCalls==0&&resolveCalls==1);
 a=pending();a.onExit();a.decide(true);assert(decisionCalls==2);
 a=pending();a.status_.offer.kind=airdrop::OfferKind::Url;isStopped=true;a.wifiOwned_=true;a.onExit();
 assert(disconnectCalls==1&&resolveCalls==2&&!resolvedOpened);
 // Every exit state routes only after the persistent receiver reports Stopped.
 for(auto state:{airdrop::State::Listening,airdrop::State::Pending,airdrop::State::Receiving,
     airdrop::State::Publishing,airdrop::State::Success,airdrop::State::UrlReady,
     airdrop::State::Error,airdrop::State::Stopping}){
  a=pending();a.leaving_=true;supplied=a.status_;supplied.state=state;
  isStopped=false;const int before=navigations;a.loop();assert(navigations==before);
  isStopped=true;a.loop();assert(navigations==before+1);
 }
 a=pending();supplied=a.status_;supplied.state=airdrop::State::UrlReady;supplied.offer.kind=airdrop::OfferKind::Url;
 isStopped=false;a.loop();assert(browserCalls==0);isStopped=true;a.loop();assert(browserCalls==1);
 a=pending();supplied=a.status_;isStopped=false;a.mappedInput.home=true;a.loop();
 assert(a.leaving_&&a.homeRequested_);a.mappedInput.home=false;const int beforeHome=navigations;
 a.loop();assert(navigations==beforeHome);isStopped=true;a.loop();assert(navigations==beforeHome+1&&lastHome);
 a=pending();supplied=a.status_;isStopped=false;a.mappedInput.back=true;a.loop();
 assert(a.leaving_&&a.mappedInput.backSuppressed);a.mappedInput.back=false;isStopped=true;a.loop();assert(!lastHome);
 // Explicit recovery cannot start a new receiver over ambiguous publication/live ownership.
 a=pending();a.status_.publicationUncertain=true;isStopped=true;a.retryReceiver();assert(startCalls==0);
 a.status_.publicationUncertain=false;isStopped=false;a.retryReceiver();assert(startCalls==0);
 isStopped=true;a.retryReceiver();assert(startCalls==1);
 std::puts("PASS production UI consent: paint/token/deadline/one-use, Back/Home/power safe revocation, one-shot URL and nonblocking exit");
}
