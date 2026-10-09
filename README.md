# ThinCube

ThinCube 是 StickS3 上的 Clawd 电子伙伴与 Mac Wi-Fi / BLE 麦克风。Mac 软件同名 ThinCube，角色名保留 Clawd。当前 Mac 为 v0.4.0，已烧录固件为 v0.4.3，兼容 v1 通信协议固件。已完成配网、恢复已存配对、Bonjour 发现，以及 Wi-Fi 音频送达 BlackHole 音频桥的实测；设备端连续采样达到每秒 100 帧。屏幕朝上平放后竖向倒立已在真机触发传音并显示悬浮对讲机。A 键快捷键、目标应用录音与无线 OTA 仍待用户在场联调。

v0.4.0 起已实现 Wi-Fi / BLE 互斥模式、上位机及长按 B 切换、60 秒未确认自动回退、BLE 控制/语音和 A+B 重新配对。完整 Flash 备份、烧录与原设置保留已完成。**上位机两个方向各两次切换成功，用户实测 B 短按静音及长按双向切换正常。**三分钟语音暴露的丢帧问题经 v0.4.1 修复后，最终一分钟复测设备队列丢帧为 0、播放补零约 0.016%，全程连接正常。按用户要求未执行长时间压力测试，端到端时延尚未测量。完整协议、结果与验收范围见 [Wi-Fi / BLE 实现](docs/wifi-ble-implementation.md)。

## 已实现

- 设备 v0.4.3 将两个横屏方向拆为不同玩法：A 键在右保留目标探索，A 键在左为 20 秒念力接星；轻倾斜控制托盘，接住增加星数与连击，漏接清空连击后继续。两侧共用方向锁、长按 A 1.2 秒退出、休息和语音隔离规则。13 个固件测试程序通过，已编译并 USB 烧写，原配对/网络/校准区域未写入；用户确认两侧场景、接星和退出正常。操作与现场结果见 [横屏玩法](docs/landscape-exploration.md)。

- 设备 v0.4.2 横屏短按 A 主动进入探索并锁定方向；相对握持基准轻微倾斜瞄准，约 8 px 内吸附、停留 250 ms 显示绿色锁定框并自动扫描。成功或长按 A 1.2 秒退出后恢复原来的 90 ms 方向确认。两个横屏方向完成探索、长按取消和退出旋转已实测，用户确认符合预期。规则与证据见 [横屏探索](docs/landscape-exploration.md)。
- 设备 v0.3.10 修复休息唤醒时的计时下溢：同一轮内更新的时间戳按 0 ms 经过时间处理，防止刚唤醒就再次入睡、吞掉新的倒置动作。新增连续倒置、状态超时、计时回绕及停止后禁止自动恢复的回归测试。
- 设备 v0.3.9 新增 Clawd 短电子音：唤醒、互动、念力扫描、发现、好奇、晕眩和入睡；Mac“语音 → Clawd 音效”调整 0–100% 音量、静音与试听，菜单栏也可静音。默认 35%，设备持久保存；传音、配网和升级优先占用音频通路。

- 设备热点配网，每台设备独立密码；Wi-Fi 验证成功后才保存，新网络失败保留旧凭据。
- Mac 原生 SwiftUI 上位机：Bonjour 发现、屏幕 PIN 配对、亮度、增益、静音、本地声音互动、语音总开关及 Wi-Fi 切换。
- Mac v0.3.12 记住最后连接的设备与连接意愿：应用重启或替换后自动恢复；设备掉线后按 1/2/4/8/15/30 秒间隔重试，此后每 30 秒继续。Bonjour 可更新同一设备的新地址，锁屏/休眠/网络不可用时暂停，恢复后继续。手动“断开”会停止重试并在下次启动保持断开。
- 48 kHz 单声道 PCM 经 Wi-Fi UDP 到 Mac，缓冲处理乱序、重复、丢包与小幅时钟漂移，再通过 BlackHole 2ch 提供系统音频输入。
- 校准后上下倒置进入语音模式。A 按下保持自定义组合键，松开释放；停止、断线、锁屏及退出均释放已注入的按键。
- Mac 快捷键编辑器保留 Folotoy/VibeCard 的侧栏“按键”页、键帽和录制交互，另提供“选择快捷键”窗口。一个“区分左右修饰键”开关统一控制两种设置方式，支持仅修饰键、双侧组合、F1–F20、导航键与小键盘，兼容旧配置。
- 屏幕四方向自动适配，缩小、旋转、展开过渡；横置按 A 键所在侧切换探索/接星，短按 A 开始并通过倾斜游玩；平放保留最近方向。
- Clawd 无嘴、无腮红，用念力举起对讲机；待命红灯、A 按住绿灯。待机探索、好奇、跳跃、晕眩恢复、充能和醒来动作已迁入固件。
- 配对鉴权的局域网 OTA：本机选择固件，设备 A 确认，备用槽写入、SHA-256 与设备镜像校验、启动健康确认及失败回滚路径。

## 构建产物

Mac 应用与菜单栏使用以 StickS3 瘦长机身、屏幕和蓝色按键为主题的配套扁平图标，PNG、ICNS、SVG/PDF 和导出方式见 [图标设计说明](docs/icons.md)。

- `artifacts/ThinCube.app`：本机 Apple Silicon 构建，macOS 13+，固定 Apple Development 签名，未公证。
- `artifacts/ThinCube-v0.4.0-mac-arm64.zip`：本次联调构建的便携压缩包；与已安装应用一起刷新，签名以包内应用为准。
- `artifacts/thincube-v0.4.3.bin`：最新 app 镜像，供兼容 v1 协议的 A/B 分区设备 OTA 使用，不能当作整片 Flash 备份烧录。包含探索/接星双场景、游戏方向锁、目标吸附和自动扫描；保留电池图标内百分比、绿色填充与电量滤波。
- `artifacts/firmware-manifest.json`：大小、SHA-256 与设备标识。
- `artifacts/landscape-games-v0.4.3/`：双场景实现的源码快照、完整 Flash 备份和编译/测试/烧写/真机记录。
- `firmware-backup/`：原有备份及本次修改前源码/固件备份。
- `artifacts/exploration-v0.4.2/`：v0.4.2 源码快照、完整 8 MB Flash 备份、编译/测试/烧写日志及真机验收记录。

本机联调固定使用 `/Applications/ThinCube.app`。`scripts/build-macos.sh` 构建、签名成功后会自动替换此处的系统应用；如果 ThinCube 正在运行，会先退出，替换后从同一位置重新启动。旧版保存在 `artifacts/install-backups/`。后续测试与系统授权都使用此固定位置，不能只更新 `artifacts/` 中的应用包。

## 首次使用

1. 将 v0.3 固件安装到 StickS3。初次安装或分区不兼容时，需要 USB，通过 `cd firmware && pio run -t upload` 安装项目 bootloader、A/B 分区表及应用。已有兼容 A/B 分区与旧 OTA 的设备也可按其旧升级流程迁移 app；不要用新 Mac 客户端连接 v0.2 协议。
2. 无 Wi-Fi 凭据时，设备显示完整的 `ThinCube-Setup-xxxxxx` 热点名称及 **8 位数字密码**，密码以屏幕为准并在重启后保留。v0.3.1 自动替换旧版 16 位密码，手机可能需要先忘记旧热点。连接后打开 `http://192.168.4.1`，选择 2.4 GHz Wi-Fi 并输入密码。若需恢复配网，开机时按住 B。
3. 将 ThinCube.app 复制到 `/Applications` 后打开。让 Mac 与设备处于同一局域网，选择发现的设备或输入 IP / `.local` 名称。
4. 输入设备屏幕的六位 PIN，点击“配对并连接”。配对窗口两分钟；空闲时同时按住 A+B 三秒并松开两键重新开启配对窗口并撤销旧配对及 BLE 绑定。Mac 将密钥保存在系统钥匙串。
5. 在“设备”页依次完成“竖向倒立，A 键在上”和“屏幕朝上平放”校准。竖向校准时屏幕仍面向你、A 键在上、显示屏在下，USB 线从顶部引出。校准完成保持待命；屏幕朝上平放后再倒立才触发语音。上位机等待设备确认本次校准结果，已有勾选表示之前保存的校准。已有方向数据保留原含义，平放时在桌面转动不会触发倒置语音。
6. 从 [BlackHole 官方项目](https://github.com/ExistentialAudio/BlackHole) 安装 **BlackHole 2ch**，按其说明重启音频服务或 Mac。在“语音”点“准备音频”，再按需点“设为系统输入”，或在目标应用内选择 BlackHole 2ch。
7. 从侧栏进入“按键”，点击 A 键行的“录制”，按下所需组合并松开全部按键，完成后立即保存和生效；Esc 或“停止录制”取消。无法录制或需要设置 Esc 时，点击“选择快捷键”，勾选修饰键并选择主键，检查预览后点击“使用快捷键”；主键选“无”可设置仅修饰键。底部只有一个“区分左右修饰键”开关，统一控制录制、选择和执行；关闭时合并左右，保留原侧别以便重新开启恢复。“按键 → 按键测试”实时显示权限、设备姿态/传音/A 键状态及最近 100 条事件，记录设备边沿、拦截原因和系统事件提交；可复制或清空。点击“测试当前快捷键”在本窗口短暂发送并自动松开，可查看实际系统回读；也可直接测试 Mac 键盘，停止或切换窗口后结束。本窗口测试不会改变映射。快捷键需要系统设置 → 隐私与安全性 → 辅助功能允许当前 ThinCube；按键测试区同时显示辅助功能检查和事件发送检查。系统开关已开但检查拒绝时，需刷新原授权并重启应用。目标语音软件需配置同一组合键，并处于能够接收该快捷键的状态。默认 Option+Space 只是示例，不保证任何语音软件默认采用它。
8. 开启“允许语音输入”。屏幕仍朝向你，上下翻转至 A 键在上，稍作停稳即可传音：v0.3.7 屏幕方向确认 90 ms，语音姿态确认 220 ms。确认计时从进入有效姿态开始，尚需加上传感器采样与调度时间。A 按住举起对讲机并保持组合键，松开只释放组合键，音频持续至退出语音模式。

设备不会录制或上传到云端；语音识别由接收音频的 Mac 软件负责。BlackHole 是独立安装的虚拟音频设备，本版不包含自行开发的 HAL 驱动。应用只在用户点击时修改系统输入，不修改系统输出。

## 模式与隐私行为

| 操作或状态 | 实际行为 |
|---|---|
| 关闭语音总开关 | 禁止向 Mac 传音；可以保留本地声音驱动的好奇动作 |
| 音效音量 / 音效静音 | 只控制喇叭，0% 也停止音效；不关闭麦克风或语音输入 |
| 长按 B 3 秒并松开 | 切换 Wi-Fi / BLE，停止当前语音；目标连接 60 秒未确认则恢复原模式 |
| A+B 同时按住 3 秒并松开两键 | 空闲时重新配对；模式不变，撤销旧授权；组合手势消费单键动作 |
| 静音 / 短按 B | 停止物理麦克风采集、网络音频和声音动作；静音状态持久保存 |
| 倒置进入语音 | 需要校准、总开关、Mac 音频就绪、控制连接存活，且未静音/休眠/升级 |
| A 按住 / 松开 | 组合键按下 / 释放；指示灯绿 / 红，LIVE 表示持续的音频会话 |
| 倒置前已按住 A | 必须先松开再按下才触发组合键 |
| 翻回正常竖持、横持或屏幕朝上平放 | 结束姿态触发的会话，释放组合键；下一次倒立可重新开始 |
| 横置玩法 | A 键在右为探索，在左为接星；短按 A 开始并锁定方向，倾斜控制；探索成功、接星 20 秒到时或长按 A 1.2 秒退出后恢复旋转；不触发姿态传音或语音快捷键，静音不影响按键玩法 |
| Mac 手动开始语音 | 唤醒休息中的 Clawd 并可正放持续收音；翻回正常姿态不结束手动会话，快捷键仍仅倒置时触发 |
| 断线、静音、停止、OTA、重新启动 | 停止会话；恢复后不会因为仍倒置而自动重新传音，需重新翻转或手动开始 |
| 屏幕朝下平放 / 视觉睡眠 | 停止麦克风和声音动作；声音不能唤醒。视觉睡眠尚不等于硬件深睡 |

## Wi-Fi OTA

BLE 模式先在“设备连接”切回 Wi-Fi。在 Mac“固件升级”选择 `thincube-v0.4.3.bin`。设备需有近期可信电量读数且至少 50%，上一轮 OTA 启动健康确认须已完成。客户端先停止音频并释放快捷键，再发送版本、精确长度和 SHA-256；设备显示确认页，30 秒内按 A 确认，B 取消。升级门限采用原始、中值及平滑电压估算中的最低值，不使用缓慢变化的显示值。

上传只写备用槽。镜像型号、长度、SHA-256 与 IDF 镜像验证全部通过后才切换启动槽。新固件运行至少 60 秒且屏幕、IMU、控制服务与所需麦克风采集检查通过才确认启动；90 秒仍未确认则请求回滚。崩溃或提前重启走 bootloader 回滚路径。这些路径已实现，**断电、丢包和实际回滚仍需真机验收**。

这里的“在线 OTA”是上位机通过 Wi-Fi 上传本地固件；尚不包含云端版本索引、自动下载或自动更新。固件型号标识和 SHA-256 用于兼容性及传输完整性，不是发行者数字签名。控制与音频目前为局域网明文协议，应在可信局域网使用，尚未实现 TLS、签名固件或防版本降级。

## 开发与验证

固件：PlatformIO `espressif32@6.12.0`，Arduino ESP32 2.0.17，M5Unified 固定提交 `8b63555e7bd9ac71978c55ad868fd1d1d3b2ef3e`，M5GFX `0.2.32`，8 MB Flash / Octal PSRAM，两个 2 MiB app 槽。

```sh
cd firmware
pio run
cd ..
python3 scripts/package-firmware.py
```

Mac：Xcode 命令行工具、Swift 5.9+，无外部 Swift 包依赖。脚本使用项目内构建缓存，`--disable-sandbox` 避免在已受限执行环境中重复启动 SwiftPM 沙盒。

默认构建包含系统应用替换，需要对 `/Applications` 的写权限。仅生成分发产物时使用 `scripts/build-macos.sh --no-install`；已有构建可通过 `bash scripts/install-macos.sh` 单独安装。

Mac 构建固定复用 Apple Development 或 Developer ID Application 身份；不再自动生成自签证书或退回 ad-hoc。首次在 Xcode“Settings → Apple Accounts → 团队 → Manage Certificates”配置开发证书，再运行 `bash scripts/setup-macos-signing.sh`；存在多个身份时用 `--identity <证书 SHA-1 指纹>` 明确选择。公共证书和指纹保存在 `~/Library/Application Support/ThinCube/Signing/`，私钥仍保存在原钥匙串。证书到期或迁移机器时，需要明确更新或恢复签名身份。

`scripts/sign-macos.sh` 校验固定证书、Apple 证书链、开发/Developer ID 类别扩展、TeamIdentifier 和稳定 DR；安装脚本也校验这些条件。普通自签证书虽然能稳定 DR，现代登录钥匙串仍按它的 cdhash 分区，不能独立解决更新后的密码弹窗。切换到 Apple 签名后，首次访问原设备配对项可能仍需输入登录钥匙串密码并选择“始终允许”。原配对项无需删除。原理与验证见 [钥匙串更新调研](docs/keychain-update-research.md)。

`bash scripts/test-macos-signing.sh` 用两个内容不同的程序模拟同路径替换，禁止系统交互，验证新程序能读取旧程序保存的一次性钥匙串测试项，并校验临时签名和缺失身份会被拒绝。测试完成后删除自己创建的测试项，不读取真实设备凭据。

```sh
scripts/build-macos.sh
clang++ -std=c++17 -Wall -Wextra -pedantic firmware/test/policy_test.cpp -o /tmp/thincube-policy-test
/tmp/thincube-policy-test
clang++ -std=c++17 -Wall -Wextra -pedantic firmware/test/pose_voice_test.cpp -o /tmp/thincube-pose-voice-test
/tmp/thincube-pose-voice-test
clang++ -std=c++17 -Wall -Wextra -pedantic firmware/test/display_motion_test.cpp -o /tmp/thincube-display-motion-test
/tmp/thincube-display-motion-test
clang++ -std=c++17 -Wall -Wextra -pedantic firmware/test/exploration_test.cpp -o /tmp/thincube-exploration-test
/tmp/thincube-exploration-test
clang++ -std=c++17 -Wall -Wextra -pedantic firmware/test/peer_address_test.cpp -o /tmp/thincube-peer-test
/tmp/thincube-peer-test
clang++ -std=c++17 -Wall -Wextra -pedantic firmware/test/sound_policy_test.cpp -o /tmp/thincube-sound-test
/tmp/thincube-sound-test
clang++ -std=c++17 -Wall -Wextra -pedantic firmware/test/ui_timing_test.cpp -o /tmp/thincube-ui-timing-test
/tmp/thincube-ui-timing-test
clang++ -std=c++17 -Wall -Wextra -pedantic firmware/test/battery_gauge_test.cpp -o /tmp/thincube-battery-test
/tmp/thincube-battery-test
cd macos
swift test --disable-sandbox
```

测试覆盖及真机验收清单见 `docs/implementation-status.md`，通信协议见 `docs/thincube-protocol-v1.md`，角色动作见 `docs/clawd-character-v2.md`。源码与文档通过 Git 维护；构建产物、设备 Flash 备份及现场日志保留在本机，不纳入版本控制。

v0.3.7 将屏幕方向确认与语音模式确认分离，原 700 ms 竖向确认缩短为语音 220 ms、屏幕 90 ms，并加入姿态保持滞回。旋转使用预分配内部 RAM 缓冲及逐帧时间插值，目标过渡 180 ms；完成后回到 LCD 硬件旋转，持续方向不额外做软件旋转。途中反向转动从当前角度平滑折返。平放保持屏幕方向，FaceUp 仍在 160 ms 确认后结束姿态语音；FaceDown 保留 1500 ms 的休息姿态确认。

Mac v0.3.11 增加按键测试区：设备边沿/姿态/传音状态、每次注入拦截或提交结果、键码/左右 flags、前台应用和本窗口实际回读；使用事件发送权限检查决定是否可注入。本地测试投递至 ThinCube 进程，设备按键仍发送至系统前台应用。修复系统状态快照包含自身合成按键时，松开事件仍残留修饰键 flags 的问题；使用独立事件源，并在释放时排除自身持有的键。合成期间物理再按相同键不支持无缝接管，清理优先配对释放。更新应用使用临时签名，代码签名要求包含二进制哈希，更新后原辅助功能或本地网络开关可能仍显示开启但不匹配当前应用；此时需要刷新授权。按键边沿去重独立于注入条件，受阻边沿仍确认且恢复权限后不会自动重放；序号 0 和回绕同样有效。读取已保存配对凭据移到后台，系统钥匙串确认不再阻塞界面。42 项 Swift 测试通过。

Mac v0.3.10 在 Folotoy 风格的独立“按键”页保留键帽和“录制/停止录制”，补充无需实际按键的选择窗口；录制松键后、手动选择确认后即时保存。Esc 取消录制或选择窗口，Esc 绑定可手动选择。重复的左右开关合并为一个，直接读取并持久保存当前设备的左右设置；暂时关闭不会丢失已保存侧别。设备按键对应 ThinCube 的正面 A 键。仅修饰键和左右同组组合继续受支持，录制及选择窗口打开期间设备边沿只确认不注入。普通主键在物理键盘上已按住时拒绝注入；合成按住期间再物理按相同主键，不支持无缝接管，清理仍会发送配对释放。Fn 与 Caps Lock 未纳入可映射键域。37 项 Swift 测试通过；实际目标软件接收及物理 A 键保持/释放仍待用户在场验收。

v0.3.6 将屏幕朝上平放识别为明确姿态，修复平放校准后的第一次倒置被初始化吞掉、以及从休息唤醒时麦克风尚未准备导致触发丢失的问题。保持倒立时断线、停止、静音或重启仍不会自动恢复；可朝上平放再倒立，或手动开始。上位机显示当前姿态与待命原因，校准显示本次设备确认结果。心跳计时器在 UI 追踪期间继续运行。

v0.3.4 联调修复了控制 JSON 帧格式、双栈连接的 IPv4 对端解析，以及屏幕绘制干扰采样缓冲补充的问题。Mac“音频调试”可查看收到的帧数、写入音频桥的样本数、丢帧补零及累计缓冲等待时间；这些计数用于诊断，不代表端到端延迟或目标应用已收到声音。

若 Mac 应用提示离线，但设备 IP 可以访问，先查看“系统设置 → 隐私与安全性 → 本地网络”中的 ThinCube 权限。本机调试曾出现开关已开启但系统日志仍为 `Local network prohibited` 的情况；刷新该既有开关后连接恢复。

v0.3.12 电量每秒采集一次电压，使用最近 7 次有效读数的中值及指数平滑；至少 5 次有效采样后显示百分比。正常显示每 5 秒最多变化 1 个百分点，并使用 2 个百分点迟滞；持续低电量允许快速下调，连续 5 秒无有效读数显示 `--%`。百分比仍沿用固定版本 SDK 的电压估算（3300–4100 mV），不是经过本电池完整充放电标定的真实荷电量；本次改善稳定性，不宣称提升绝对精度。

USB 监视可使用 `python3 scripts/monitor-usb.py /dev/cu.usbmodem101 --seconds 60`（需要 pyserial）。`[power]` 行包含原始/中值/平滑电压、原始/显示/OTA 百分比、VBUS 电压及去抖后的充电状态，便于区分测量波动与显示变化。macOS 打开原生 USB 串口时可能复位设备，应保持同一监视连接。PlatformIO 已配置上传后 watchdog 完整复位，解决普通 USB core reset 保留下载模式的问题，依据 [Espressif 复位说明](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/esptool/advanced-options.html)。

配网页实际像素可用 `python3 scripts/capture-setup.py /dev/cu.usbmodem101 artifacts/setup.png` 抓取（需要 pyserial、Pillow）。该 USB 调试命令可导出稳定空闲界面的实际画布（含竖屏主页和横屏探索），拒绝配对窗口、OTA、旋转中与持续传音中截图；不是网页模拟图。可给 monitor-usb.py 传入 --capture-request 请求文件，在已有串口连接中抓图，避免再次打开 USB 引起复位。

## 倒置后没有出现对讲机

先检查 Mac 是否显示“在线”，以及“语音”页的待命原因。设备状态栏的 `wifi` 只说明已连上路由器；对讲机传音还需要 Mac 控制连接及音频桥就绪。日志出现 `inverted ... block disconnected` 说明倒置已识别，但 Mac 未就绪，此时继续翻转无法开始传音。

Mac v0.3.12 会自动恢复上次连接，也可点击“连接”手动重试；应用关闭前若手动断开，则下次启动继续保持断开。配对被撤销、钥匙串缺失或系统拒绝读取时暂停重试并显示处理提示，避免反复弹窗。本地开发版替换后若出现读取已存配对的钥匙串确认窗口，请在系统窗口手动允许。系统本地网络权限失效时，应用会显示等待自动重连；需恢复现有 ThinCube 授权，重连代码无法绕过系统限制。先恢复已有凭据访问，避免无故撤销配对或重新校准。连接恢复后，先正常竖持、横持或屏幕朝上平放，再重新倒立即可开始；保持原来的倒置姿态不会在断线恢复后自动传音。

本机完整回归：`bash scripts/test-local.sh`（不访问 USB，不烧录）。
