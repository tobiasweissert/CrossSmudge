#pragma once
#include <utility>
#include <GfxRenderer.h>
#include <MappedInputManager.h>
struct ActivityResult {bool isCancelled=false;};
class Activity {
 public:
  struct RenderLock{explicit RenderLock(Activity&) {}};
  GfxRenderer& renderer;MappedInputManager& mappedInput;
  int finished=0,updates=0;ActivityResult result;
  Activity(const char*,GfxRenderer&r,MappedInputManager&i):renderer(r),mappedInput(i){}
  virtual ~Activity()=default;
  virtual void onEnter(){}virtual void onExit(){}virtual void loop(){}virtual void render(RenderLock&&){}
  virtual bool allowPowerAsConfirmInReaderMode()const{return false;}
  void setResult(ActivityResult r){result=r;}void finish(){++finished;}
  void requestUpdate(bool=false){++updates;}
};
