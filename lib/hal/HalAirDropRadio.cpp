#include "HalAirDropRadio.h"
#include "findmy/Firmware.h"

#include <AppCapabilities.h>
#if CROSSINK_APP_CAP_AIRDROP
#include <Logging.h>
#include <LuaHttp.h>
#include <WiFi.h>
#include <esp_wifi.h>
namespace HalAirDropRadio {
bool releaseArduino() {
  // Explicit BLE stop + controller idle confirmation, never inferred from WiFi.
  // Handoff is latched; only a reboot and new owner consent can re-enable.
  if (!crosscub::findmy::firmware::beforeAirDrop()) return false;
  if (ink::http::Session::busy()) {
    LOG_ERR("AIRDROP", "HTTPS owner is still active");
    return false;
  }
  if (!WiFi.mode(WIFI_OFF)) {
    LOG_ERR("AIRDROP", "Arduino WiFi release failed");
    return false;
  }
  wifi_mode_t mode;
  const auto result = esp_wifi_get_mode(&mode);
  if (result != ESP_ERR_WIFI_NOT_INIT) {
    LOG_ERR("AIRDROP", "Radio still initialized after Arduino release: %d", result);
    return false;
  }
  return true;
}
}  // namespace HalAirDropRadio
#endif
