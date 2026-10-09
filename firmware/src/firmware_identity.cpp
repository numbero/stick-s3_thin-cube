// ESP-IDF's linker places custom descriptors immediately after esp_app_desc_t.
// Keep the SDK descriptor intact; this second descriptor identifies our device.
struct ThinCubeIdentity { char device[32]; char version[32]; };
extern "C" const ThinCubeIdentity thincubeFirmwareIdentity
  __attribute__((section(".rodata_custom_desc"), used)) = {"ThinCube-StickS3-v1", CLAWD_FW_VER};
const char* firmwareVersion(){return thincubeFirmwareIdentity.version;}
