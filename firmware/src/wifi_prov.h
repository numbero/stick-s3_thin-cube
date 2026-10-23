#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "connection_setup_policy.h"

/* 热点配网：设备自开热点 → 手机连上后浏览器弹出门户 → 提交 Wi-Fi 账号密码
   → APSTA 并行验密 → 成功才写 NVS 并重启。设计见 docs/wifi-prov-ota.md §2。
   流程与约束照搬 folotoy-vibecard 的 prov.c（真机验证过），实现按 Arduino 改写。 */

#define PROV_MAX_TRIES 3

void      provBegin();                 /* 幂等：已在配网则 no-op */
void      provBeginWithCredentials(const char* ssid, const char* pass);
const char* provApSsid();
const char* provApPass();
void      provTick();                  /* 主循环里调：DNS 应答 + 验密状态机 */
void      provRetry();                 /* FAIL 后重来：清计数回 WAIT（BtnA） */
bool      provActive();
bool      provServiceHealthy();        /* local portal or credential-verification resources */
ProvStage provStage();
const char* provStageName(ProvStage s);
const char* provTargetSsid();          /* 手机提交的目标 SSID（空 = 还没提交） */
const char* provLastErr();             /* 最近一次失败原因 */
int       provTries();

/* NVS（命名空间 clawd）*/
bool provLoadCreds(char* ssid, size_t n, char* pass, size_t m);
bool provSaveCreds(const char* ssid, const char* pass);
bool provForgetCreds();
void provLeave();
bool provResumeChange();
void provPrepareChange();
const char* provRequestNetwork(const char* ssid,const char* pass,const char* requestId);
WiFiChange provChangeRecord();
bool provManagedChange();
