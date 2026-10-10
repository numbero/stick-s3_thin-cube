# 配网与 OTA 设计（Wireless provisioning & firmware update）

> 历史文档：以下为 v0.2 设计记录。v0.3 已改用每台设备独立热点密码、Mac 配对鉴权、SHA-256 镜像校验和延迟健康确认。当前操作以根目录 README.md 与 docs/thincube-protocol-v1.md 为准；固定热点密码、旧 token 和浏览器上传流程不适用于 v0.3。

状态：待确认（界面 mockup 见 `demo/index.html` 新增的 PROV / OTA 两页）
日期：2026-09-23
参考实现：`folotoy-vibecard/firmware/src/prov.c`（热点配网）、`ota.c` +
`folotoy-vibecard/docs/development/ota-design.md`（A/B OTA）

> 借用的是参考实现里经过真机验证的**约束与流程**，不是照抄代码：我们的目标栈是
> Arduino + M5Unified（见 §8 待定项）。

---

## 1. 一句话

设备自己开热点，手机连上后浏览器自动弹出配网页，填完 Wi-Fi 密码点保存；配好后，
在同一局域网里把新固件 POST 给设备就能升级，升级失败能回滚，断网断电不变砖。

## 2. 热点配网（Provisioning）

### 2.1 什么时候进入配网

| 触发 | 说明 |
|---|---|
| 开机时 NVS 里没有可用凭据 | 主流程直接进配网 |
| 开机时按住 BtnB | 强制进配网（凭据错了、换网络时用） |
| 菜单 → `wifi setup` | 运行时手动重新配网 |

进入配网时**不启动**小家伙的日常循环：屏幕切到配网页，clawd 保持一个「等待」姿态。

### 2.2 热点参数

| 项 | 值 |
|---|---|
| SSID | `CLAWD-Setup` |
| 密码 | `clawd1234`（≥8 位满足 WPA2；固定密码便于主人重复配网） |
| 设备 IP | `192.168.4.1` |
| 模式 | APSTA —— 热点和「验证目标网络」同时存在，这是能当场验密的前提 |

### 2.3 门户流程

```
手机                                设备
 │ 连上 CLAWD-Setup
 │ GET /generate_204（或 hotspot-detect / connecttest）─▶ 302 到 http://192.168.4.1/
 │ GET /                                              ─▶ 配网页（琥珀终端风）
 │ GET /scan                                          ─▶ 扫到的 SSID 列表（JSON）
 │ POST /save  {ssid, pass}                           ─▶ 记下目标，进 VERIFY
 │                                                      APSTA 连目标网络，最多 3 次
 │ ◀─ 页面上转圈                                           成功 → 写 NVS → 2 秒后重启
 │                                                     失败 → 回 WAIT，页面报原因
```

要点（都来自参考实现踩过的坑）：

- **通配 DNS**：自己开一个 UDP 53 的 socket，任何 A 查询都回 `192.168.4.1`。
  这是让手机自动弹门户的关键，比只做 302 可靠得多。
- 额外接管三个探测 URL（`/generate_204`、`/hotspot-detect.html`、`/connecttest.txt`）防止系统弹「无互联网」提示。
- **先验证再落盘**：凭据只在 APSTA 并行连上目标网络之后才写 NVS，然后**重启**（不在原地续跑运行态，
  避免半初始化状态）。失败不写、不重启，等着重填。
- 最多 3 次尝试；每次都把失败原因回显到网页（`not found` / `auth failed` / `timeout`）。
- **手机提交的目标网络密码不落屏、不进日志**（只显示前 2 位 + `***`）。热点自身的密码是另一回事：
  它印在设备屏幕上给主人照着输入，所以完整显示。
- 配网会话可重入：已在配网时再次触发是 no-op。

### 2.4 存什么

NVS 命名空间 `clawd`：`ssid` / `pass` / `prov_done`（bool）。
另外存一份 `last_err`（最近一次失败原因，纯诊断用，不参与逻辑）。

## 3. OTA

### 3.1 分区表（8MB flash，A/B 双槽）

```csv
# Name,     Type, SubType, Offset,   Size,     Flags
nvs,        data, nvs,     0x9000,   0x5000,
otadata,    data, ota,     0xE000,   0x2000,
app0,       app,  ota_0,   0x10000,  0x200000,
app1,       app,  ota_1,   0x210000, 0x200000,
```

- 两槽各 2 MB；当前 app 预计 0.8~1.0 MB（M5Unified + WiFi + httpd），余量充足
- 不设 factory 分区：otadata 空白时 bootloader 引导第一个 app 槽
- NVS 仍在 `0x9000`（与出厂布局一致，我们自己的凭据键不受影响）
- **只 OTA app**：bootloader / 分区表不进 OTA 范围（简化 + 安全），要改它们走 USB

> 刷这个分区表会替换掉出厂分区表。出厂整片镜像已备份在 `firmware-backup/`，
> 需要还原时用 `restore.sh` 整片写回（含 NVS、出厂资源）。

### 3.2 会话流程

```
上位机（浏览器/脚本）                          设备
 │ POST /ota/begin {ver, size}               ─▶ 检查：电量≥50%？无进行中会话？尺寸≤槽？
 │                                              esp_ota_begin(下一个槽)
 │ ◀─ {"ok":true,"token":"a1b2c3d4"}            随机 8 位 hex token
 │                                              ── 屏幕弹确认页（A=确认 B=取消，30 秒超时）
 │ POST /ota  (body = app.bin, X-Clawd-Token) ─▶ 4KB/块 → esp_ota_write
 │ ◀─ {"pct":42}                                 每 2% 或 250ms 上报一次
 │ ◀─ {"ok":true}                                esp_ota_end + 镜像校验 → 切槽
 │                                              屏幕「ok, reboot」→ 2 秒后重启
```

### 3.3 保护与边界（逐条对齐参考实现）

| 场景 | 行为 |
|---|---|
| 电量 < 50% | 拒绝，`err="low_bat"`；电量读不到时放行（避免锁死） |
| 已有会话 | `err="busy"` |
| 无 `Content-Length` 或 chunked | 拒绝（必须有确定的尺寸才能预判槽容量） |
| 尺寸 > 槽容量 | 拒绝 `too_large` |
| token 不匹配 | 401，设备无影响 |
| 确认页 30 秒无按键 | 中止，`err="timeout"`，旧固件不受影响 |
| 上传中途断开 / bin 损坏 | `esp_ota_abort`；`esp_ota_end` 自带校验，坏 bin 必被拒 |
| OTA 期间断电 | 旧槽完好，重启跑旧版，重试即可 |
| OTA 期间按键 | 只认 A / B（确认或取消），不响应其它交互 |

### 3.4 回滚

- 构建期打开 `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`
- 新固件启动后处于 `PENDING_VERIFY`：**稳定运行 60 秒**才 `mark_app_valid_cancel_rollback()`
- 这 60 秒内如果用户主动重启（重新配网、切模式），先 mark_valid 再 restart，避免误回滚刚升上来的版本
- 60 秒内崩溃 → 自动回滚到旧槽

### 3.5 上位机

第一版不做 Mac 客户端：**设备自己开一个上传页** `http://<设备IP>/`（运行时页面，和配网页同一个 httpd），
拖一个 `.bin` 进去就上传，带进度条。这样不需要装任何东西——和配网页同一套 UI 语汇。

## 4. 屏幕（要确认的就是这两页）

### 4.1 PROV 配网页

| 阶段 | 屏幕内容 |
|---|---|
| `AP` | `AP  CLAWD-Setup` / `pass clawd1234` / `open 192.168.4.1` |
| `WAIT` | clawd 摆出「广播」姿态 + 天线波纹；`join hotspot` / `then open` / `192.168.4.1` / 动态省略号 |
| `SCAN` | `scanning...` |
| `VERIFY` | `ap: <ssid>` / `try 1/3` / 转圈 |
| `OK` | `connected` / `saved` / `reboot in 2s`；clawd 兴奋 |
| `FAIL` | 失败原因 + `retry`；clawd 不高兴 |

### 4.2 OTA 页

v0.4.6 页面精简为固定 135×240 布局，使用 Font0 固定字格，不依赖主界面字体标定或日志行数。标题与版本在顶部，阶段与大号数值居中，底部操作固定在 y=222，文字下方保留 10 px；角色和屏幕日志从 OTA 页移除，串口日志继续保留。此布局在 v0.4.6 启动后的升级使用。

| 阶段 | 屏幕内容 |
|---|---|
| `CONFIRM` | 目标版本 / `CONFIRM` / 大号 30 秒倒计时 / `A: YES   B: NO` |
| `READY` | `WAITING` / 0% 与进度条 / `Waiting for Mac` / `B: CANCEL` |
| `RECV` | `RECEIVING` / 大号百分比与连续进度条 / `Keep power on` / `B: CANCEL` |
| `VERIFY` | `VERIFYING` / 100% / `Checking image` / `PLEASE WAIT` |
| `DONE` | 绿色 `COMPLETE` / 100% / `Restarting...` / `UPDATE COMPLETE` |
| `FAIL` | 红色 `FAILED` / 完整错误原因换行 / `Try again on Mac` / `B: BACK` |

两页都保持琥珀终端 + terracotta clawd 的既有语汇，不引入新的界面元素。

## 5. 与功耗的关系

- 配网会话期间**不睡**：热点必须持续开着（关屏也不关热点会显著耗电 → 屏幕保持常亮，
  并显示「配网中」，避免主人以为死机）
- OTA 期间不睡、不切页、不做任何可能重启的事
- 日常运行：联网后仍按主设计进深睡；唤醒后按需重连

## 6. 待实机验证（不当结论写死）

1. `WiFi.softAP()` + `WiFi.begin()` 在 APSTA 下并行验密的实际行为（参考实现证实可行，但那是 IDF 路径）
2. 通配 DNS 劫持在 iOS / Android / macOS 上各自能否弹出门户
3. 上传 2MB 固件时的实际耗时、是否需要调大 httpd 的 recv 缓冲
4. 回滚窗口 60 秒是否够（我们启动比参考项目慢：M5GFX 初始化 + PSRAM）
5. 配网/OTA 期间的实际电流（热点 + 屏幕常亮）
6. S3 上 `esp_ota_get_next_update_partition` 的选择是否与分区表预期一致

## 7. 实施顺序（每步等确认）

| 阶段 | 内容 | 状态 |
|---|---|---|
| 0 | 本设计文档 + 两页 mockup（`demo/index.html` 的 PROV / OTA 页） | 已交付 |
| 1 | 固件 bring-up：分区表 + 屏幕/字体/小爪/按键/电池 + 分区与回滚状态自检 | 进行中（`firmware/`） |
| 2 | 配网模块：AP / 通配 DNS / 门户 / 验证 / NVS | 待做 |
| 3 | OTA 模块：会话 / token / 上传 / 回滚 + 运行时上传页 | 待做 |
| 4 | 把 PROV / OTA 两页接进正式 UI（状态机、日志、声音） | 待做 |

### 刷机前的安全网（2026-09-23 验证）

- `firmware-backup/sticks3-original-8MB-20260923.bin`（整片 8MB，含 bootloader / 分区表 / 应用 / NVS）
- 该目录原本缺少 README 里写的 `.sha256` 校验文件，已补：`shasum -a 256` =
  `f607fc650704f821caf6fec5b4a9651f542113ac0081458b62ba72b1ec8dfd3e`
- 不止比对自己：从设备读回前 1MB（`esptool read_flash 0x0 0x100000`）与备份前 1MB 比对，
  **md5 完全一致**（`115e296b319cf6c177b15b4d602136a7`）→ 备份确实是设备出厂状态的真实副本
- 还原：`firmware-backup/restore.sh <串口>`（脚本默认端口 `/dev/cu.usbmodem1401` 是当时的值，
  实际端口每次插拔会变，用 `ls /dev/cu.usb*` 确认后传参）

## 8. 目标栈（已定）

**Arduino + M5Unified**（2026-09-23 定）。界面与生物用 M5GFX 现成能力；网络部分不引入第三方库，
直接用 Arduino-ESP32 里就能 include 的 IDF 组件：

| 能力 | 用什么 |
|---|---|
| 热点 / APSTA | `WiFi.softAP()` / `WiFi.mode(WIFI_AP_STA)` / `WiFi.begin()` |
| 门户 httpd | IDF `esp_http_server`（独立 task，上传时界面不卡） |
| 通配 DNS | 裸 UDP socket（`lwip` 的 `socket/sendto/recvfrom`，照参考实现的做法） |
| 凭据存储 | `Preferences`（命名空间 `clawd`） |
| OTA 写入 | `esp_ota_begin/write/end/set_boot_partition` |
| 回滚 | `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` + `esp_ota_mark_app_valid_cancel_rollback` |
| 构建 | PlatformIO 单 env，board = `m5stack-sticks3`（不可用则 `esp32-s3-devkitc-1` + 8MB/PSRAM 参数） |

代价要说清楚：参考实现的 IDF 代码不能直接搬，`prov.c` / `ota.c` 的 API 调用要按上表改写，
但**流程、约束、边界条件照搬**（§2、§3 那些行就是照搬清单）。
