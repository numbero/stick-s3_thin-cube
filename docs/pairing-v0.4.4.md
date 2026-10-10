# 一次配对与持久连接模式

2026-10-10，设备 v0.4.4 / Mac v0.4.3。

## 配对流程

在 ThinCube 中选择设备并连接，首次使用只输入设备 `PAIR PIN` 页上的六位码。Wi-Fi 和蓝牙共用设备应用密钥，密钥保存在 Mac 钥匙串；普通重连、切换模式和升级保留此授权。空闲时同时按住 A+B 三秒并松开会撤销应用授权，显示新的两分钟配对窗口。

蓝牙保持 LE Secure Connections 链路加密，使用 NoInputNoOutput / Just Works，不配置系统静态 PIN，也不显示 `BLUETOOTH PIN` 页。双方自动保存协商的加密密钥，用于断线、设备重启和跨模式重连；此过程无需输入系统验证码。已有有效绑定保留。业务特征和通知描述符仍要求加密；加密就绪不代表 ThinCube 已授权。应用使用随机 nonce 与 HMAC-SHA256 证明持有已保存密钥；未授权连接只能访问 `info`、`pair`、`challenge`、`auth`。状态、设置、音频启动和模式切换继续受应用授权控制，周期状态和音频只在授权后发送。

底层参数依据 [Espressif GAP API](https://docs.espressif.com/projects/esp-idf/en/v4.4.8/esp32s3/api-reference/bluetooth/esp_gap_ble.html) 和 [GATT 安全示例](https://github.com/espressif/esp-idf/blob/master/examples/bluetooth/bluedroid/ble/gatt_security_server/tutorial/Gatt_Security_Server_Example_Walkthrough.md)。Just Works 不提供原系统数字码的 MITM 认证；这里保留链路加密，宿主机控制权限由应用配对码承担。Wi-Fi v1 仍为既有局域网明文协议。

若旧版本 A+B 已删除设备系统绑定，但 Mac 保留旧记录，macOS 会报 `peerRemovedPairingInformation`。需要在 Mac 蓝牙设置中忽略一次旧 ThinCube 记录；此操作不删除 ThinCube 应用授权。上位机明确显示迁移提示，不要求重新输入系统验证码。

## 模式切换

B 单独长按三秒并松开、以及上位机切换，共用一个受理入口：保存目标模式与请求 ID，停止旧会话，700 ms 后重启。目标启动直接保存 `committed`，无需 Mac 发送 `transport_commit`。缺少 Wi-Fi 凭据时进入配网；路由器或 Mac 未连接时保留选择，持续等待。再次长按 B 可再次切换，重启也不改变选择。

NVS 记录保留原布局与枚举值。旧 `pending` / `trial` 在新版启动时变为 `committed`；已经发生的旧 `rolled_back` 保留当前实际模式。保存失败不会改回先前模式，但后续切换会报告存储失败。OTA 镜像健康检查与失败回滚独立保留。

上位机移除 65 秒后查找旧模式的逻辑，等待所选目标。自动模式可以跟随离线期间再次发生的实体 B 切换；手动模式按所选传输查找。重连后的已认证状态作为设备实际选择。旧固件的 trial / commit 协议兼容处理仍在上位机保留。

## 本轮验证

- 13 个固件 CPU 测试程序及 64 项 Swift 测试通过，包含最终 v0.4.4 真实镜像验证，无跳过。模式测试覆盖双向切换、重复重启、旧 trial 迁移、幂等提交与按键组合边界。
- 固件构建成功，最终镜像 1,672,736 字节，SHA-256 见 `artifacts/firmware-manifest.json`。
- 已读取完整 8 MB Flash 备份，只写 app0（0x10000），写入哈希校验及最终 watchdog 复位成功，真实启动日志确认 v0.4.4 从 app0 运行；未写 NVS、OTA 数据、分区表或备用应用槽。
- Mac 已构建、固定签名并安装至 `/Applications/ThinCube.app`，安装脚本逐文件比对通过。旧应用留在 `artifacts/install-backups/`。
- Wi-Fi 首次只输入一次屏幕码后连接成功，原亮度 134、两项校准与静音状态保留。无授权的 `/api/status` 和 `/api/settings` 均返回 401。
- 用户忽略失效的旧系统绑定后，最终版本完成 BLE → Wi-Fi → BLE 真机切换，分别约 2.63 秒和 3.05 秒。BLE 自动加密成功（auth mode 0x09），MTU 517，未再次输入屏幕码或系统码；亮度、校准与静音均保留。
- 用已加密但未证明应用密钥的真实 BLE 连接验证：状态、设置、传音启动、模式切换、无效 auth proof 及其后的状态请求全部返回 401 / `pairing_required`，随后原应用授权自动恢复连接。记录见 `hardware-check-bonded.jsonl`。
- 请求 BLE → Wi-Fi 后在重启前主动断开宿主机、停止重连；USB 日志持续显示 Wi-Fi / committed，约 97 秒后公开 info 仍返回 Wi-Fi。超过原 60 秒回退时限后再恢复自动连接，直接复用原授权成功。记录见 `offline-switch.jsonl`、`usb-final.log`、`offline-info.json` 和 `validation.json`。
- 收尾恢复 Mac 自动模式，再从 Wi-Fi 切回 BLE，约 10.01 秒连接成功，未输入新码；最终从系统安装位置正常启动，保留蓝牙待命。记录见 `restore-ble.jsonl`。

本轮物理长按 B 的手势边界由 CPU 测试验证，现场切换使用与 B 键共用的模式受理入口；未将其声称为本轮物理按键实测。没有新增音频采集或压力测试。

源码修改前快照、完整 Flash、构建/测试/烧录/USB 日志存放在 `artifacts/pairing-v0.4.4/`。日志不记录配对码、密钥、proof 或音频内容。

显式开发验收可用正式应用的 `--hardware-check <绝对日志路径> --switch-count 2 --ble-access-check`；最后保持 BLE 待命，验证加密但未授权的客户端无法读取状态、修改设置、启动传音、切换模式或用无效 proof 通过认证，然后自动恢复原应用授权连接。

`--offline-switch` 在请求切换后、设备重启前主动断开宿主机并停止重连；通过同一 USB 日志观察目标模式是否持续保留。它是明确启用的开发验收选项，普通应用启动不执行这些检查。
