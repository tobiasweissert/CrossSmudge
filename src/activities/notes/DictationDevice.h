#pragma once
#include <atomic>
#include <string>

#include "DictationProtocol.h"
namespace dictation {
// Access credential only on the activity loop. No disk, Lua or HTTP settings field.
bool configure(const std::string& key);
void forget();
bool configured();
bool provisioningAvailable();
class Job {
 public:
  enum State { Idle, Recording, Ready, Sending, Review, Failed };
  std::atomic<State> state{Idle};
  std::atomic<bool> cancelled{false}, stop{false}, send{false}, running{false};
  std::atomic<const char*> error{nullptr};
  bool authorized() const;
  std::string transcript;
  bool start();
  void drain();
  ~Job() { drain(); }

 private:
  uint32_t generation = 0;
  char key[maxKey + 1]{};
  void work();
};
}  // namespace dictation
