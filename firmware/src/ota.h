#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <esp_http_server.h>
#include "ota_boot_policy.h"

/* ThinCube v1 OTA，详见 docs/thincube-protocol-v1.md。
   配对鉴权 → {ver,size,sha256} → 设备 A 确认 → 校验设备镜像标识
   → 写入备用槽 → SHA-256 / IDF 校验 → 切换启动槽。
   HTTP worker 独占写入句柄；B 取消仅发信号。健康检查通过后才确认启动。 */

enum OtaStage {
  OTA_IDLE = 0,   /* 无会话 */
  OTA_CONFIRM,    /* 已 begin，等按键确认 */
  OTA_READY,      /* 已确认，等客户端开始上传 */
  OTA_RECV,       /* 正在接收 */
  OTA_VERIFY,     /* esp_ota_end 校验中 */
  OTA_DONE,       /* 写成功，待重启 */
  OTA_FAIL,       /* 失败/取消/超时 */
};

/* 本次启动的试运行提示；普通 VALID 开机不显示“升级完成”。
   仅主循环调用，Complete 显示 5 秒或按键提前关闭，Checking 不允许关闭。 */
enum class OtaBootNotice { None, Checking, Complete };
void      otaBootBegin();              /* uiBegin 前缓存本次 PENDING_VERIFY */
OtaBootNotice otaBootNotice();
uint32_t  otaBootStableSec();          /* 已实际观察到的连续健康时长，至多 15 秒 */
unsigned  otaBootCompleteLeftSec();    /* 从真实 IDF 确认成功后计时 5 秒 */
void      otaBootDismiss();
void      otaBootTick();               /* 主循环总是调用；完成页超时不依赖无线服务 */

void      otaTick(uint32_t ms);        /* 主循环里调：起 httpd / 确认超时 / 重启 */
bool      otaRegisterRoutes(httpd_handle_t server); // maintenance server reuses the same OTA engine
bool      otaActive();                 /* stage != IDLE：占用屏幕 */
bool      otaConnected();              /* STA 是否已连上（决定上传页可达） */
OtaStage  otaStage();
const char* otaStageName(OtaStage s);
const char* otaTargetVer();            /* 客户端声明的目标版本（空=未定） */
int       otaPct();                    /* 0..100 */
const char* otaLastErr();
uint32_t  otaSessionGeneration();
void      otaConfirm(uint32_t expectedGeneration); /* fresh BtnA click for this manifest */
bool      otaCancel();                 /* BtnB：取消；VERIFY/DONE 后返回 false */
void      otaDismiss();                /* FAIL 后回主界面 */
int       otaConfirmLeftSec();         /* CONFIRM 页剩余秒数（倒计时显示用） */
bool      otaMarkValid();              /* 仅健康检查后确认；失败则继续重试 */
void      otaRollbackUnhealthy();      /* 90 秒仍未确认的 OTA 试启动回滚 */

/* httpd 不能读 I2C；主循环以同一快照刷新电量及实际输入电源。 */
struct OtaPowerStatus { int level; bool externalPower; };
void      otaSetPower(int level,int source); // source: 1 external, 0 battery, -1 unknown
OtaPowerStatus otaPowerStatus();
void      otaStopServer();             // main loop only, before AP provisioning
void      otaCloseControlConnection(int fd); // main loop: revoke an existing Wi-Fi session
