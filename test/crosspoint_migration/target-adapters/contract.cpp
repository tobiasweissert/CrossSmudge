#include <HalDisplay.h>
#include <HalStorage.h>

#include <type_traits>

#include "util/ButtonNavigator.h"
static_assert(std::is_same_v<decltype(&MappedInputManager::wasPowerSingleClick), bool (MappedInputManager::*)() const>);
static_assert(std::is_same_v<decltype(&HalDisplay::controllerName), const char* (HalDisplay::*)() const>);
static_assert(std::is_same_v<decltype(&HalFile::creationTime), uint32_t (HalFile::*)()>);
void release(ButtonNavigator& n) {
  n.onNextRelease([] {});
}
