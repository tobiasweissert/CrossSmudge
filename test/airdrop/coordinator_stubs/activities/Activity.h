#pragma once
#include <GfxRenderer.h>
#include <MappedInputManager.h>
#include <functional>
#include <memory>
#define LOG_ERR(...) ((void)0)
#define tr(...) ""
#define STR_BACK 1
#define STR_SELECT 2
struct ActivityResult {bool isCancelled=false;};
struct RenderLock {RenderLock()=default;template<class T>explicit RenderLock(T&) {}};
class Activity {
 protected:GfxRenderer& renderer;MappedInputManager& mappedInput;
 public:
 bool finished=false;int updates=0;
 std::unique_ptr<Activity> child;
 std::function<void(const ActivityResult&)> resultCallback;
 Activity(const char*,GfxRenderer&r,MappedInputManager&i):renderer(r),mappedInput(i){}
 virtual ~Activity()=default;
 virtual void onEnter(){}virtual void onExit(){}virtual void loop(){}virtual void render(RenderLock&&){}
 virtual bool handleHomeGesture(){return false;}virtual bool preventAutoSleep(){return false;}
 void finish(){finished=true;}void requestUpdate(){++updates;}
 template<class T,class F>void startActivityForResult(std::unique_ptr<T> next,F callback){child=std::move(next);resultCallback=std::move(callback);}
};
