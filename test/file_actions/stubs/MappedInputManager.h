#pragma once
#include <set>
#include <GfxRenderer.h>
class HalGPIO;
class MappedInputManager {
 public:
 enum class Button {Back,Confirm,Left,Right,Up,Down,Power};
 enum class SwipeDir {None,Left,Right,Up,Down};
 struct Label{const char*text;Label(const char*t):text(t){}};
 struct Labels{const char*btn1;const char*btn2;const char*btn3;const char*btn4;};
 const GfxRenderer& renderer;std::set<Button> press,release,held;
 bool down=false,tap=false,contact=false;int x=0,y=0;
 MappedInputManager(HalGPIO&,const GfxRenderer&r):renderer(r){}
 const GfxRenderer&getRenderer()const{return renderer;}
 bool wasScreenTouchDown(int&a,int&b)const{a=x;b=y;return down;}
 bool wasScreenTapped(int&a,int&b)const{a=x;b=y;return tap;}
 bool isScreenContactActive()const{return contact;}
 bool wasScreenTouchReleased()const{return tap;}
 bool isScreenTouchHeld(int&a,int&b)const{a=x;b=y;return contact;}
 bool wasPressed(Button b)const{return press.count(b);}
 bool wasReleased(Button b)const{return release.count(b);}
 bool isPressed(Button b)const{return held.count(b);}
 bool isPhysicalPressed(Button b)const{return held.count(b);}
 bool wasAnyPressed()const{return !press.empty();}
 bool wasAnyReleased()const{return !release.empty();}
 SwipeDir wasSwipe()const{return SwipeDir::None;}
 void suppressNextTouchTap(){}void suppressCurrentTouchContact(){}
 void suppressNextConfirmRelease(){}void suppressNextBackRelease(){}void suppressNextPowerRelease(){}
 void setReaderTouchscreenOverride(bool){}
 Label withBackArrow(const char*t)const{return Label(t);}
 Label withPreviousPageArrow(const char*t)const{return Label(t);}
 Label withNextPageArrow(const char*t)const{return Label(t);}
 Labels mapLabels(Label a,Label b,Label c,Label d)const{return {a.text,b.text,c.text,d.text};}
 void frame(){press.clear();release.clear();down=tap=false;}
};
