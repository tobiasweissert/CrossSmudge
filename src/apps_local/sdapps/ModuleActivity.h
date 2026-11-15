#pragma once
#if defined(FREEINK_DEVICE_STICKY)
#include "activities/Activity.h"
#include "components/UiAppHost.h"
#include "ModuleImage.h"
namespace sdapps {
class ModuleActivity final : public Activity, protected UiAppHost {
 public:
  ModuleActivity(GfxRenderer&, MappedInputManager&, const char* root, const Package&);
  ~ModuleActivity() override;
  static std::unique_ptr<Activity> create(GfxRenderer&, MappedInputManager&, const char*, const Package&);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  void prepareForSleep() override;
  bool preventAutoSleep() override;
  bool handleHomeGesture() override;
 private:
  struct Runtime;
  std::unique_ptr<Runtime> runtime;
  Package package;
  const char* root;
  ModuleImage image;
  CpHostV2 host2{};
  CpHostV1& host=host2.v1;
  void* moduleState = nullptr;
  bool started = false, leaving = false;
  void bindServices();
  void emit(CpEventKind, int32_t = 0, int32_t = 0, const char* = nullptr, size_t = 0, const char* = nullptr);
  void buildScreen(UiScreen&);
  void activate(int);
  void notice(const char*);
  int present(const CpView*);
  int presentV2(const CpPresentation*);
  void buildPresentation(UiScreen&);
  int queueEdit(int32_t, const char*, const char*, size_t);
  int queueRequest(int32_t, CpRequestKind, const char*, const uint8_t*, size_t);
  int queueRecord(int32_t, int);
  void startJob();
  void work();
  void cancel();
  void drain();
  void shutdown();
  void openChild();
  bool storagePath(const char*, char*, size_t) const;
  uint32_t parseHtml(const char*, const char*, size_t);
  void releaseDocument(uint32_t);
};
}
#endif
