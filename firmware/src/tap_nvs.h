#pragma once
#include "tap_storage.h"
#include <nvs.h>

// A dedicated namespace keeps pairing, calibration and connection data separate.
struct TapNvsStore {
  TapStoredRead read(uint8_t* bytes,size_t capacity,size_t& size){
    nvs_handle_t handle;const auto opened=nvs_open("thincube_tap",NVS_READONLY,&handle);
    if(opened==ESP_ERR_NVS_NOT_FOUND)return TapStoredRead::Missing;
    if(opened!=ESP_OK)return TapStoredRead::Failed;
    auto result=nvs_get_blob(handle,"config",nullptr,&size);
    if(result==ESP_OK&&size<=capacity)result=nvs_get_blob(handle,"config",bytes,&size);
    nvs_close(handle);
    if(result==ESP_ERR_NVS_NOT_FOUND)return TapStoredRead::Missing;
    return result==ESP_OK?TapStoredRead::Found:TapStoredRead::Failed;
  }
  bool write(const uint8_t* bytes,size_t size){
    nvs_handle_t handle;if(nvs_open("thincube_tap",NVS_READWRITE,&handle)!=ESP_OK)return false;
    const bool ok=nvs_set_blob(handle,"config",bytes,size)==ESP_OK&&nvs_commit(handle)==ESP_OK;
    nvs_close(handle);return ok;
  }
};
