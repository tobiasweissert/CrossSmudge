#include <CrossPointSettings.h>
#include <GfxRenderer.h>
#include <gtest/gtest.h>

#include "util/ButtonNavigator.h"
unsigned long testNowMs = 1000;
namespace {
uint8_t pressed = 0, released = 0, held = 0;
unsigned long heldMs = 0, powerMs = 0;
}  // namespace
namespace {
const char* controllerIdentity = nullptr;
unsigned controllerQueries = 0;
}  // namespace
// The physical HAL boundary is doubled. MappedInputManager and Navigator are
// the whole production translation units, using exact SDK declarations.
InputManager::InputManager() {}
void HalGPIO::update() {}
bool HalGPIO::wasPressed(uint8_t b) const { return pressed & (1u << b); }
bool HalGPIO::wasReleased(uint8_t b) const { return released & (1u << b); }
bool HalGPIO::isPressed(uint8_t b) const { return held & (1u << b); }
bool HalGPIO::wasAnyPressed() const { return pressed; }
bool HalGPIO::wasAnyReleased() const { return released; }
unsigned long HalGPIO::getHeldTime() const { return heldMs; }
unsigned long HalGPIO::getPowerButtonHeldTime() const { return powerMs; }
SemaphoreHandle_t xSemaphoreCreateMutex() {
  static int token;
  return &token;
}
bool HalGPIO::hasTouch() const { return false; }
unsigned long HalGPIO::lastTouchHeldMs() const { return 0; }
bool HalGPIO::wasTouchActivity() const { return false; }
bool HalGPIO::wasTouchTap(float&, float&) const { return false; }
bool HalGPIO::wasTouchDown(float&, float&) const { return false; }
bool HalGPIO::isTouchPressed() const { return false; }
bool HalGPIO::supportsMultiTouch() const { return false; }
HalGPIO::TouchSnapshot HalGPIO::getTouchSnapshot() const { return {}; }
bool HalGPIO::wasSwipe(float&, float&, float&, float&) const { return false; }
namespace freeink {
FreeInkDisplay::FreeInkDisplay(int8_t, int8_t, int8_t, int8_t, int8_t, int8_t) {}
const char* FreeInkDisplay::getControllerName() const {
  ++controllerQueries;
  return controllerIdentity;
}
EpdBus::~EpdBus() = default;
}  // namespace freeink
class Adapters : public testing::Test {
 protected:
  HalGPIO gpio;
  HalDisplay panel;
  GfxRenderer renderer{panel};
  MappedInputManager input{gpio, renderer};
  ButtonNavigator nav;
  int taps = 0, repeats = 0;
  void SetUp() override {
    pressed = released = held = 0;
    heldMs = powerMs = 0;
    testNowMs = 1000;
    ButtonNavigator::setMappedInputManager(input);
  }
  void frame(uint8_t p, uint8_t r, uint8_t h, unsigned long duration) {
    pressed = p;
    released = r;
    held = h;
    heldMs = duration;
    input.update();
  }
  void poll() {
    nav.onNextRelease([&] { ++taps; });
    nav.onPreviousRelease([&] { --taps; });
    nav.onNextContinuous([&] { ++repeats; });
    nav.onPreviousContinuous([&] { --repeats; });
  }
};
TEST_F(Adapters, NextTapIsDeferredUntilRelease) {
  frame(1u << HalGPIO::BTN_RIGHT, 0, 1u << HalGPIO::BTN_RIGHT, 0);
  poll();
  EXPECT_EQ(taps, 0);
  frame(0, 1u << HalGPIO::BTN_RIGHT, 0, 80);
  poll();
  EXPECT_EQ(taps, 1);
  frame(0, 0, 0, 0);
  poll();
  EXPECT_EQ(taps, 1);
}
TEST_F(Adapters, PowerClickRequiresShortUnsuppressedRelease) {
  frame(0, 0, 0, 0);
  EXPECT_FALSE(input.wasPowerSingleClick());
  powerMs = 399;
  frame(0, 1u << HalGPIO::BTN_POWER, 0, 9999);
  EXPECT_TRUE(input.wasPowerSingleClick());
  powerMs = 400;
  EXPECT_FALSE(input.wasPowerSingleClick());
  powerMs = 800;
  EXPECT_FALSE(input.wasPowerSingleClick());
  input.suppressNextPowerRelease();
  powerMs = 10;
  EXPECT_FALSE(input.wasPowerSingleClick());
  EXPECT_FALSE(input.wasPowerSingleClick());
  EXPECT_FALSE(input.wasReleased(MappedInputManager::Button::Power));
  frame(0, 0, 0, 0);
  frame(0, 1u << HalGPIO::BTN_POWER, 0, 10);
  EXPECT_TRUE(input.wasPowerSingleClick());
}
TEST_F(Adapters, ContinuousHoldCancelsDeferredTap) {
  frame(1u << HalGPIO::BTN_RIGHT, 0, 1u << HalGPIO::BTN_RIGHT, 0);
  poll();
  testNowMs += 501;
  frame(0, 0, 1u << HalGPIO::BTN_RIGHT, 501);
  poll();
  EXPECT_EQ(repeats, 1);
  frame(0, 1u << HalGPIO::BTN_RIGHT, 0, 550);
  poll();
  EXPECT_EQ(taps, 0);
  EXPECT_EQ(repeats, 1);
}
TEST_F(Adapters, RemoteClickReleasesOnceAndDoesNotPersist) {
  ASSERT_TRUE(input.injectRemoteClick(RemoteButton::Next));
  poll();
  EXPECT_EQ(taps, 1);
  frame(0, 0, 0, 0);
  poll();
  EXPECT_EQ(taps, 1);
}
TEST_F(Adapters, ControllerIdentityPreservesUnknownAndForwardsSdkResult) {
  const char* identities[] = {nullptr, "external-panel-double"};
  for (const char* identity : identities) {
    controllerIdentity = identity;
    const auto before = controllerQueries;
    EXPECT_EQ(panel.controllerName(), identity);
    EXPECT_EQ(controllerQueries, before + 1);
  }
}
