#pragma once
#include <nvs.h>
namespace ink::http {
// Missing namespace/key is factory-default enabled. Other storage errors must
// not silently bypass an existing opt-out. Old per-domain grants are ignored.
inline bool networkEnabled(bool& readOk) {
  nvs_handle_t handle;
  esp_err_t result = nvs_open("lua_https", NVS_READONLY, &handle);
  readOk = result == ESP_OK || result == ESP_ERR_NVS_NOT_FOUND;
  if (result != ESP_OK) return result == ESP_ERR_NVS_NOT_FOUND;
  uint8_t blocked = 0;
  result = nvs_get_u8(handle, "blocked", &blocked);
  nvs_close(handle);
  readOk = result == ESP_OK || result == ESP_ERR_NVS_NOT_FOUND;
  return result == ESP_ERR_NVS_NOT_FOUND || (result == ESP_OK && blocked == 0);
}
inline bool setNetworkEnabled(bool enabled) {
  nvs_handle_t handle;
  if (nvs_open("lua_https", NVS_READWRITE, &handle) != ESP_OK) return false;
  esp_err_t result = nvs_set_u8(handle, "blocked", enabled ? 0 : 1);
  if (result == ESP_OK) result = nvs_commit(handle);
  nvs_close(handle);
  return result == ESP_OK;
}
}
