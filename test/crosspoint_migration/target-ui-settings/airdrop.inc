#include "components/AirDropConsentInput.h"
struct MappedInputManager {
  enum class SwipeDir {None, Up};
  enum class Button {Confirm};
  bool confirmHeld=false,confirmPressed=false,suppressed=false;
  bool isScreenContactActive() const {return held;}
  bool isPhysicalPressed(Button) const {return confirmHeld;}
  bool wasPressed(Button) const {return confirmPressed;}
  void suppressNextConfirmRelease() {suppressed=true;}
  bool down=false, tap=false, held=false, released=false, swipe=false, longPress=false;
  bool wasScreenLongPress(int& x,int& y)const{x=12;y=34;return longPress;}
  bool wasScreenTouchDown(int& x,int& y)const{x=10;y=20;return down;}
  bool isScreenTouchHeld(int& x,int& y)const{x=11;y=22;return held;}
  bool wasScreenTapped(int& x,int& y)const{x=13;y=24;return tap;}
  bool wasScreenTouchReleased()const{return released;}
  SwipeDir wasSwipe()const{return swipe?SwipeDir::Up:SwipeDir::None;}
};
// SNAPSHOT_BODY
namespace fui=freeink::ui;
struct RenderLock {template<typename T> explicit RenderLock(T&) {}};
class AirDropReceiveActivity {
 public:
  MappedInputManager mappedInput;
  airdrop::input::ConsentGate consentInput_;
  bool paintedPending_=false;
  uint32_t routedEpoch_=0,paintedIdentity_=77,routedIdentity_=0;
  int decisions=0;
  struct UiBoundary {
    bool enabled=true;int routes=0;
    bool routeTouch(const MappedInputManager&,fui::ActionEvent& event) {
      ++routes;event.action=enabled?1:0;return enabled;
    }
  } ui_;
  bool routeConsentInput();
  void decide(bool) {EXPECT_EQ(routedEpoch_,consentInput_.epoch());EXPECT_EQ(routedIdentity_,paintedIdentity_);++decisions;}
};
// AIRDROP_ROUTE
TEST(AirDropInput, ActualRouteDoesNotAdmitUnpaintedOrStaleConfirm) {
  AirDropReceiveActivity a;
  a.mappedInput.confirmHeld=a.mappedInput.confirmPressed=true;
  EXPECT_FALSE(a.routeConsentInput());EXPECT_EQ(a.decisions,0);
  auto& capture=airdrop::input::capture;
  capture.beginPaint();a.consentInput_.painted(capture.completePaint());a.paintedPending_=true;
  capture.end(capture.begin());EXPECT_FALSE(a.routeConsentInput());EXPECT_EQ(a.decisions,0);
  a.mappedInput.confirmHeld=a.mappedInput.confirmPressed=false;
  capture.end(capture.begin());EXPECT_FALSE(a.routeConsentInput());
  a.mappedInput.confirmHeld=a.mappedInput.confirmPressed=true;
  capture.end(capture.begin());EXPECT_TRUE(a.routeConsentInput());
  EXPECT_EQ(a.decisions,1);EXPECT_TRUE(a.mappedInput.suppressed);EXPECT_EQ(a.routedEpoch_,0u);
  EXPECT_FALSE(a.routeConsentInput());EXPECT_EQ(a.decisions,1);
}
TEST(AirDropInput, RequiresNeutralFrameAfterCompletePaint) {
  airdrop::input::Capture capture;
  airdrop::input::ConsentGate gate;
  MappedInputManager input;
  bool touch=false,confirm=false;
  auto poll = [&] {
    capture.end(capture.begin());
    auto snap=touchSnapshotFrom(input);
    return gate.consume(capture.frame(),input.held,snap.touchPressed,snap.touchReleased,false,false,touch,confirm);
  };
  capture.beginPaint();gate.painted(capture.completePaint());
  input.tap=true; EXPECT_TRUE(poll()); EXPECT_FALSE(touch);
  input.tap=false; EXPECT_TRUE(poll()); EXPECT_FALSE(touch);
  input.tap=true; EXPECT_TRUE(poll()); EXPECT_TRUE(touch);
  capture.beginPaint();gate.painted(capture.completePaint());
  EXPECT_TRUE(poll()); EXPECT_FALSE(touch);
}
TEST(AirDropInput, PollSpanningPaintAndRepeatedFrameCannotAdmit) {
  airdrop::input::Capture capture;airdrop::input::ConsentGate gate;
  bool touch=false,confirm=false;
  auto epoch=capture.begin();capture.beginPaint();gate.painted(capture.completePaint());capture.end(epoch);
  EXPECT_FALSE(gate.consume(capture.frame(),false,false,true,false,true,touch,confirm));
  capture.end(capture.begin());EXPECT_TRUE(gate.consume(capture.frame(),false,false,false,false,false,touch,confirm));
  EXPECT_FALSE(gate.consume(capture.frame(),false,false,true,false,true,touch,confirm));
  capture.end(capture.begin());EXPECT_TRUE(gate.consume(capture.frame(),false,false,true,true,true,touch,confirm));
  EXPECT_TRUE(touch);EXPECT_TRUE(confirm);
}
