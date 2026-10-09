#pragma once
#include <stdbool.h>
#include <stdint.h>

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

void      otaTick(uint32_t ms);        /* 主循环里调：起 httpd / 确认超时 / 重启 */
bool      otaActive();                 /* stage != IDLE：占用屏幕 */
bool      otaConnected();              /* STA 是否已连上（决定上传页可达） */
OtaStage  otaStage();
const char* otaStageName(OtaStage s);
const char* otaTargetVer();            /* 客户端声明的目标版本（空=未定） */
int       otaPct();                    /* 0..100 */
const char* otaLastErr();
void      otaConfirm();                /* BtnA：确认 */
void      otaCancel();                 /* BtnB：取消 */
void      otaDismiss();                /* FAIL 后回主界面 */
int       otaConfirmLeftSec();         /* CONFIRM 页剩余秒数（倒计时显示用） */
bool      otaMarkValid();              /* 仅健康检查后确认；失败则继续重试 */
void      otaRollbackUnhealthy();      /* 90 秒仍未确认的 OTA 试启动回滚 */

/* 电池电平缓存：httpd 在独立 task，不能直接读 I2C，由主循环刷新 */
void      otaSetBattery(int level);
void      otaStopServer();             // main loop only, before AP provisioning
