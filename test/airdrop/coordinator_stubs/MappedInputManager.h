#pragma once
class MappedInputManager {
 public:
 enum class Button {Back,Confirm,Left,Right,Up,Down,PageBack,PageForward};
 enum class SwipeDir {None,Up,Down,Left,Right};
 bool back=false,home=false,tap=false,backPress=false,confirmPress=false;int tapX=0,tapY=0;
 bool isPhysicalPressed(Button)const{return false;}
 bool wasReleased(Button b){return back&&b==Button::Back;}
 bool wasPressed(Button b){return b==Button::Back?backPress:b==Button::Confirm?confirmPress:false;}
 bool wasHomeGesture(){return home;}
 void suppressNextBackRelease(){}void suppressNextConfirmRelease(){}
 SwipeDir wasSwipe(){return SwipeDir::None;}
 bool isScreenContactActive(){return false;}
 bool wasScreenLongPress(int&,int&){return false;}
 bool wasScreenTapped(int&x,int&y){x=tapX;y=tapY;return tap;}
 bool wasScreenTapped(int&,int&,unsigned long&){return false;}
 struct Labels{const char*btn1="",*btn2="",*btn3="",*btn4="";};
 Labels mapLabels(const char*,const char*,const char*,const char*){return {};}
};
