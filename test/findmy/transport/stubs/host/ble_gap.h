#pragma once
#include <cstdint>
#define BLE_GAP_EVENT_ADV_COMPLETE 9
#define BLE_GAP_CONN_MODE_NON 0
#define BLE_GAP_DISC_MODE_NON 0
struct ble_addr_t { uint8_t type; uint8_t val[6]; };
struct ble_gap_adv_params { uint8_t conn_mode, disc_mode; uint16_t itvl_min, itvl_max; uint8_t channel_map, filter_policy, high_duty_cycle; };
struct ble_gap_event { uint8_t type; };
typedef int ble_gap_event_fn(struct ble_gap_event*, void*);
extern "C" {
int ble_gap_adv_start(uint8_t, const ble_addr_t*, int32_t, const ble_gap_adv_params*, ble_gap_event_fn*, void*);
int ble_gap_adv_stop(); int ble_gap_adv_active();
int ble_gap_adv_set_data(const uint8_t*, int);
int ble_gap_adv_rsp_set_data(const uint8_t*, int);
}
