#pragma once
class MappedInputManager {
 public:
  enum class Button { Back, Confirm, Up, Down };
  bool held = false, released = false, gesture = false;
  bool touchPressed=false,touchHeld=false,confirm=false,confirmPressed=false,confirmReleased=false;
  bool wasScreenTouchPressed(int&,int&) const { return touchPressed; }
  bool isScreenTouchHeld(int&,int&) const { return touchHeld; }
  bool wasPressed(Button b) const { return b==Button::Confirm && confirmPressed; }
  bool isPressed(Button b) const { return b==Button::Back ? held : b==Button::Confirm && confirm; }
  bool wasReleased(Button b) const { return b==Button::Back ? released : b==Button::Confirm && confirmReleased; }
  bool wasBackGesture() const { return gesture; }
};
