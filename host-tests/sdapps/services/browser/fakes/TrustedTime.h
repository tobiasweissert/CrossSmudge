#pragma once
#include "Arduino.h"
#include <ctime>
namespace trustedtime {inline int64_t trustedNow(){return fixture.validTime?static_cast<int64_t>(std::time(nullptr))+(fixture.rollback?3600:0):0;}}
