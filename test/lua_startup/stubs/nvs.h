#pragma once
#include <cstdint>
using esp_err_t=int; using nvs_handle_t=int;
constexpr int ESP_OK=0, ESP_ERR_NVS_NOT_FOUND=1, NVS_READONLY=1, NVS_READWRITE=2;
extern int openResult,readResult,writeResult,commitResult,closes; extern uint8_t savedBlocked;
inline int nvs_open(const char*,int,nvs_handle_t* h){*h=1;return openResult;}
inline int nvs_get_u8(nvs_handle_t,const char*,uint8_t* value){*value=savedBlocked;return readResult;}
inline void nvs_close(nvs_handle_t){++closes;}
inline int nvs_set_u8(nvs_handle_t,const char*,uint8_t value){if(!writeResult)savedBlocked=value;return writeResult;}
inline int nvs_commit(nvs_handle_t){return commitResult;}
