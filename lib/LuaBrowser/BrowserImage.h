#pragma once
#include <string>
#include <cstdint>
class GfxRenderer;
namespace ink::browser {
// Caller resolves the session-confined path. Decoder/factory access must be
// serialized by LuaAppActivity's render mutex. No retained file/decoder handle.
bool drawImage(const std::string& path,GfxRenderer& renderer,int x,int y,int width,int height);
}

#include <atomic>
#include <memory>
#include <mutex>
namespace ink::browser {
// One global bounded codec worker. It owns files/cache bands and never touches
// Lua, an activity, fonts, or the live framebuffer. Close cancels and detaches;
// the worker's reference keeps its job alive until safe cleanup finishes.
class ImageSession {
 struct Job;
 Job* job_=nullptr;
 std::mutex cancelMutex_;
 std::unique_ptr<uint8_t[]> scratch_;
 public:
 ImageSession()=default;
 ~ImageSession();
 ImageSession(const ImageSession&)=delete;
 ImageSession& operator=(const ImageSession&)=delete;
 const char* start(const std::string& source,const std::string& destination,int width,int height);
 int poll(); // 0 running, 1 adopted complete cache, -1 failed; drains ownership.
 void close();
 void requestCancel(); // Signals only; never waits for I/O or destroys the job.
 static bool busy();
 bool draw(const std::string& path,GfxRenderer& renderer,int x,int y,int width,int height,const std::atomic<bool>& cancel);
};
}
