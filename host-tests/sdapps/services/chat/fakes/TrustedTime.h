#pragma once
#include <ctime>

#include "PlatformFakes.h"
namespace trustedtime {
inline long long trustedNow() {
  return fake::trusted ? static_cast<long long>(std::time(nullptr)) + fake::clockOffset : 0;
}
}  // namespace trustedtime
