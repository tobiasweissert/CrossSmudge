#pragma once
#include <cstdint>
#define BLE_HS_EALREADY 2
#define BLE_OWN_ADDR_RANDOM 1
struct ble_hs_cfg_t { void (*reset_cb)(int); void (*sync_cb)(); };
extern "C" { extern ble_hs_cfg_t ble_hs_cfg; }
