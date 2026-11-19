#pragma once
// Synchronous popup host has no render task or held render lock.
class RenderLock {
 public:
  inline static bool held = false;
  static bool peek() { return held; }
};
