#include "ProviderKey.h"
#if defined(FREEINK_DEVICE_STICKY)
#include <nvs.h>
#include "ChatProtocol.h"
namespace stickychat {
bool loadProviderKey(std::string& key) {
  key.clear();
  nvs_handle_t handle;
  if (nvs_open("sticky-openai", NVS_READONLY, &handle) != ESP_OK) return false;
  size_t size = 0;
  bool ok = nvs_get_str(handle, "key", nullptr, &size) == ESP_OK && size > 1 && size <= 513;
  if (ok) {
    key.resize(size);
    ok = nvs_get_str(handle, "key", key.data(), &size) == ESP_OK;
    if (ok) { key.resize(size - 1); ok = validProviderKey(key); }
  }
  nvs_close(handle);
  if (!ok) key.clear();
  return ok;
}
bool saveProviderKey(const std::string& key) {
  if (!validProviderKey(key)) return false;
  nvs_handle_t handle;
  if (nvs_open("sticky-openai", NVS_READWRITE, &handle) != ESP_OK) return false;
  bool ok = nvs_set_str(handle, "key", key.c_str()) == ESP_OK && nvs_commit(handle) == ESP_OK;
  nvs_close(handle);
  return ok;
}
bool removeProviderKey() {
  nvs_handle_t handle;
  if (nvs_open("sticky-openai", NVS_READWRITE, &handle) != ESP_OK) return false;
  auto result = nvs_erase_key(handle, "key");
  bool ok = (result == ESP_OK || result == ESP_ERR_NVS_NOT_FOUND) && nvs_commit(handle) == ESP_OK;
  nvs_close(handle);
  return ok;
}
}
#endif
