# ThinCube 建联效率调研 · 2026-10-20

研究基线：`a4f7442f39b03186b6a5f48c6d5df35707d23d9e`（刚安装的远程主干）。按 `research` 与 `m5stack-sticks3` 技能核对项目源码及 Apple/Espressif 对应版本的官方资料。本次通过固定提交快照读取，避免把本地其他改动混入结论；仅新增本报告，没有改源码、构建、安装、操作连接或刷机。以下收益都是待验证的优化机会，未做当前版本端到端时延测量。

## 综合判断

刚才的提交主要让新发现事件绕过离线重试退避。剩余效率空间应沿着“设备启动 → 无线连接/加密 → 应用认证 → 新会话基线 → 控制租约/音频就绪”继续拆解。当前不能根据静态代码判断哪一段占用最多，也不能给出可靠的“整体快几倍”。

| 顺序 | 优化 | 位置与适用场景 | 已能确认的价值 / 不确定性 |
| --- | --- | --- | --- |
| 先测量 | 两端阶段埋点 | 所有建联与唤醒 | 把扫描、认证、音频准备和用户输入分开；本身不直接提速 |
| 第一批 | 复用 CONNECT 中的实际 BLE interval | 固件，已连接后的性能就绪 | 不再把已知间隔当未知，免冗余参数更新依赖；节省时间待测 |
| 第一批 | 复用有效状态，建立明确 ready 响应 | 上位机先做，随后跨端协议 | 减少重复完整状态请求/分片；必须保留新 epoch 确认 |
| 第一批 | 音频准备与网络握手重叠 | 上位机，重启应用/断线重连 | 去掉控制建联前的串行 Core Audio 工作；控制和实际语音就绪分别测 |
| 第二批 | 最小安全启动后先启无线 | 固件，冷启动/PM1 唤醒/重启式模式切换 | 无线可与部分外围工作重叠；现有 200ms delay 单独审计 |
| 第二批 | 进展感知的超时与端点新鲜度 | 上位机，旧地址/设备改变模式/异常链路 | 主要减少长尾等待和无效尝试；不能让重复广告反复打断健康握手 |
| 按测量选做 | 最近成功 Wi-Fi 信道提示、短建联窗口禁 modem sleep | 固件，Wi-Fi 重新入网及后续握手 | 作用阶段不同，AP/DTIM/耗电影响需 A/B；已有 FAST_SCAN |
| 独立项目 | 配网成功免二次重启、运行时免重启切换 | 固件，首次配网/模式切换 | 前者有确定的固定等待可省；后者工程量较大，不影响普通重连全部场景 |

已经具备的能力包括：按指定服务发现、已知 BLE UUID 直接恢复、进程内凭据缓存与后台钥匙串读取、BLE 自动加密绑定、30–60ms 快广播、DLE251/2M PHY 请求、Wi-Fi FAST_SCAN。它们不是本次新增收益。设备当前按 Wi-Fi/BLE 互斥模式工作，优化应保留这一现有行为。

## 上位机侧：已核实的等待与改法

本节代码均来自 `a4f7442f39b03186b6a5f48c6d5df35707d23d9e`，不是当前有其他改动的工作目录。下列耗时上限是代码配置，不能当作每次都会付出的延迟。

### M1. 合并重复状态读取，建立明确的会话就绪响应

**确定的现状：** BLE 已保存授权的路径为 `open → info → 读取凭据 → challenge → auth → status → 本机音频初始化 → status`，末尾 `status` 最多循环十次，每次未拿到有效 epoch 后另等 100 ms。`authenticate()` 在第 481 行已经取过完整状态；第 427 行又重置 epoch/按键流，再于第 431 行重新请求。不能将它理解成每次固定等一秒，但确有重复请求和快照传输。[握手顺序][M-flow]、[认证函数][M-auth]

**建议：** 第一阶段让认证返回的状态进入统一的“首个有效快照”处理，避免刚收到的状态又被连接初始化清掉。第二阶段新增可协商的 `hello/auth_ready`：在设备主循环真正完成 owner/epoch 更新后，返回或主动推送新会话的完整基线；首个基线之后才接受同 epoch 的增量帧。Wi-Fi 可另研究将状态校验合并进已鉴权的 WebSocket 握手/首帧，保留旧固件路径。

**必须处理的代码语义：** 固件 `auth` 当前只把 `connect` 命令入队，`authenticated=true` 不代表新 owner、epoch 和 published 快照已经就绪。不能直接删掉最后的确认，也不能拿旧 epoch 的快照报告成功。[固件认证][M-fw-auth]、[快照基线规则][M-baseline]

**可确定的收益单位：** 正常快路径至少有一个重复完整状态请求可以研究消除；具体节省毫秒数取决于 MTU、分片和连接间隔，尚未测量。

### M2. 将控制就绪与音频就绪分开，移走串行的 Core Audio 初始化

**确定的现状：** 第 425 行同步调用 `prepareAudioNow()`，然后才启动 Wi-Fi WebSocket 或确认 BLE 新状态。它枚举音频设备、设置采样率、创建/初始化/启动 HAL AudioUnit；断线时 `teardown()` 又销毁音频桥。因此一次普通重连会重新做这些本地工作，且发生在 MainActor 的连接关键路径。[调用顺序][M-flow]、[音频准备][M-audio-call]、[AudioBridge][M-audio]、[清理逻辑][M-teardown]

**建议：** 在专用串行执行环境中准备音频，与网络发现/握手重叠；独立发布“控制已鉴权”和“音频可用”。经测量确实有价值时，再短时保留已初始化的本地音频对象，清空 session/缓冲，遇设备变化或休眠时重建，避免长期无意义占用音频资源。BLE 的控制订阅准备好后可开始控制握手，音频订阅独立跟进；当前 `open()` 等两个订阅都成功才返回。[订阅门槛][M-subscribe]

**收益边界：** 仅提前显示“控制已连接”不等于语音已经能用。分别统计控制 ready、音频桥 ready、传输性能 ready 和有效音频租约，才能判断是否真正缩短拿起即用的等待。

### M3. 改善正在进行的连接尝试，降低异常长尾

**确定的现状：** 新发现事件只有在 `!connecting` 且已有重试任务时才能触发当前快速恢复；旧尝试正在运行时会忽略它。BLE 建链设有 12 秒应用超时、服务/订阅阶段统一为 90 秒、每个业务请求为 10 秒，HTTP 请求配置为 5 秒。90 秒包含可能需要用户确认的链路配对，不是每次正常连接耗时。自动模式失败后会改变首选方式，但缓存/发现结果仍可能把请求指向旧端点。[发现及恢复][M-recovery]、[BLE 阶段][M-ble-open]、[BLE 请求][M-ble-request]、[HTTP 配置][M-http]

**建议：** 给阶段记录进展与截止时间；已有绑定重连和首次交互配对分别处理。对同一已知 BLE 外设维持一个受管理的 pending connect，而非不断取消再建立；地址、设备当前模式或可达性证据发生变化时，允许旧尝试让位。普通重复广告不能反复打断正在加密/订阅的健康连接；用户断开、锁屏、OTA 和重新配对仍应阻止自动恢复。

Apple 明确说明 `connect` 请求本身不会自动超时，应用可主动取消；所以本项目的 12 秒是应用策略，并非系统硬性要求。取消异步执行，保留/切换尝试时要继续通过 generation 排除旧回调。[Apple connect][H-connect]、[Apple 取消说明][H-best]

### M4. 缩短已保存授权的慢路径，但不重复已有缓存工作

**已实现：** `cachedCredential` 已在进程内复用凭据；没有缓存时 `Credentials.load` 已移至后台。已保存外设 UUID 也已通过 `retrievePeripherals(withIdentifiers:)` 直接取回，已经不必每次先等一次扫描。这些不能重列为新增优化。[凭据快路径][M-flow]、[已知外设][M-ble-open]

**可新增：** 冷启动时，对已有明确设备身份的目标，将凭据读取与传输连接并行；在收到并验证实际设备身份之后才使用。可补充按 service 获取系统已连接外设的分支，但必须匹配目标身份，且仍需本应用调用 connect。[Apple 系统已连接外设][H-connected]

若把 `info + challenge` 合为一个 `hello`，要注意固件 nonce 当前仅有效 10 秒；钥匙串确认或首次 PIN 交互可能超过它。只对已准备好凭据的快路径使用，nonce 过期时重新取得，首次配对保持原兼容流程，不能省掉随机挑战。[nonce 生命周期][M-fw-auth]

### M5. 发现结果加入新鲜度，减少缓存端点占用等待窗口

**确定的现状：** `FoundDevice` 只有 id/host；BLE 另以本地 `lastSeen` 做八秒过期检查。Wi-Fi 依赖 Bonjour didRemove，重复解析同 id/host 不会产生新的 `onAvailable`，代码也明确说明缓存解析不能证明在线。自动选择优先当前 preferred transport，没有显式记录最近已验证可达时间。[Discovery][M-discovery]、[端点选择][M-mode]

**建议：** 区分“历史地址”“发现提示”“最近成功鉴权的地址”；旧 IP 尝试与 Bonjour 解析同时推进，已有地址可立即尝试，不为发现再添加固定等待。发现新地址或实际设备模式变化时，尽快结束无进展的旧地址尝试。只有完成身份校验后才接管会话；不把 mDNS 缓存命中等同于设备在线。

**约束：** 项目是设备侧 Wi-Fi/BLE 互斥模式。可以同时监听两种发现信息，不应为了抢快建立两个竞争的业务会话，或让未选中的设备无线服务长期运行。

### M6. 次级优化：扫描与 GATT 缓存

目前 BLE 一直允许重复广告，以支持同地址设备返回的快速恢复。可仅在离线恢复/选择设备时保留所需的重复事件，连接稳定后减轻扫描和主线程发布压力；恢复时必须正确重启，不能破坏 `a4f7442`。Apple 提醒重复扫描会增加功耗和应用处理开销；实际建联收益未测，应放在握手和启动优化之后。[Apple 扫描实践][H-best]

现有代码已经按指定 service/characteristic 发现，并未全量枚举。不要把“限定服务发现”再做一遍。若进一步研究复用 CoreBluetooth 提供的服务对象，要实现 `didModifyServices` 失效处理，并在 OTA 后重新发现；不建议自行永久缓存裸 GATT handle。[现有 GATT 发现][M-gatt]、[Apple 服务失效][H-invalid]

## 固件现状与确定的关键路径

锁定的 `firmware/platformio.ini:7` 使用 `espressif32@6.12.0`，平台清单指定 Arduino 2.0.17；该 Arduino 所带 ESP32-S3 SDK 为 ESP-IDF 4.4.7。BLE 实际使用原生 **Bluedroid**，不是 NimBLE：`ble_link.cpp:176` 调用 `esp_bluedroid_init/enable`。下面核查均使用对应版本的官方源码/API。[PlatformIO v6.12.0 清单](https://raw.githubusercontent.com/platformio/platform-espressif32/v6.12.0/platform.json)、[Arduino SDK 版本头](https://raw.githubusercontent.com/espressif/arduino-esp32/2.0.17/tools/sdk/esp32s3/include/esp_common/include/esp_idf_version.h)

| 场景 | 源码能确定的顺序与等待 | 不能据此声称的收益 |
| --- | --- | --- |
| 开机／PM1 深睡后拿起 | `main.cpp:79–139`：总线恢复 → M5 初始化 → 串口后无条件 `delay(200)` → IMU 初始化 → 深睡寄存器清理 → UI／模式／设置载入／恢复出厂检查 → 最后启动 BLE 或 Wi-Fi。总线有 5ms 等待，LCD 失败分支另有 100ms。 | IMU、显示、NVS、无线初始化各耗时未测；200ms 是现有等待，不等于整个启动只需 200ms，也不保证全部能安全删除。 |
| BLE 已绑定重连 | `ble_link.cpp:91–98`：连接后立即请求加密；`69–77` 收到成功事件后请求 DLE 251 与 2M PHY；CCCD 和业务写入均要求加密（`115–127,187–191`）。 | 现有固件已经使用绑定与自动加密，不能把“记住绑定／开启 2M”列为新的优化收益。 |
| BLE 会话可用 | `device.cpp:403–431` 支持独立 `info → challenge → auth → status`；auth 只排队 connect，`640–643` 才生成新 epoch；有效控制连接还需首个 heartbeat（`658–660` 与 `control_lease.h:11`）。 | “BLE 已连接”“已认证”“控制租约已生效”“首帧可播放”是不同里程碑。 |
| Wi-Fi 普通启动 | `main.cpp:135–138` 直接异步 `WiFi.begin(ssid,pass)`；没有先做一次应用层全网扫描；`ota.cpp:164` 等到 `WL_CONNECTED` 才启动 HTTP 和 mDNS（`device.cpp:540–542`）。 | 不存在本项目手写的固定 DHCP 等待；Arduino 2.0.17 在 GOT_IP 后才设 `WL_CONNECTED`，应测关联与 DHCP 各自耗时。 |
| 首次配网成功 | `wifi_prov.cpp:178–187`：获 IP、保存凭据后，普通门户固定再等 2 秒重启；重启后再次加入网络。受管换网成功有 1.5 秒后重启（`43`），此前切入验证启动还会重启（`transport.cpp:61–67` 的 700ms）。 | 这些等待不发生在每次普通重连；不能把 2 秒计入所有建联场景。 |

现有 BLE 广播参数是 `0x30–0x60`，即 30–60ms，三个广播信道都开（`ble_link.cpp:51–52`）；主广播含服务 UUID，名称位于扫描响应（`193–194`）。快连接请求 15–22.5ms、慢连接请求 60–90ms；音频接受实际间隔不超过 30ms（`power_save_policy.h:13,25,39–41`）。参数单位、连接更新只能在已连接时调用，均由 [IDF 4.4.7 GAP API](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/bluetooth/esp_gap_ble.html) 确认。

## 固件与跨端候选，按实施顺序排序

### P0：先增加分阶段耗时记录，定位当前瓶颈

这是验证其他优化的前提，不能仅依赖“点击连接到图标变绿”的总时长。现有日志没有覆盖每个阶段的单调时钟时间戳。

- BLE 固件记录：无线初始化开始／结束、广告 start complete、CONNECT、AUTH complete、MTU、control/audio CCCD、收到 challenge/auth、新 epoch 发布、首个有效 heartbeat、首次 audio notify。绑定复用与首次安全配对分别统计；不要记录密钥、PIN 或 proof。
- Wi-Fi 固件记录：STA start、STA connected、GOT_IP、HTTP ready、mDNS 注册完成、首个通过认证的控制会话、首个有效 heartbeat；同时记录断线 reason、信道、RSSI、IP 是否变化。依据官方事件顺序，关联完成后才开始 DHCP，GOT_IP 后再创建套接字；这是两个阶段。[IDF 4.4.7 Wi-Fi Driver](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-guides/wifi.html)
- macOS 对应记录发现、连接、服务／特征发现、订阅、凭据准备、认证、第一份有效 epoch 状态、音频输出就绪、首帧。用 attempt ID 关联同一尝试；两端单调时钟不能直接相减，靠事件序列或握手关联。

分别记录首个已认证基线、控制租约就绪、音频路径就绪和首帧有效四个完成点，避免把后续音频准备误归为无线建联。场景、样本量和统一验收方法见文末“验证方案与实施顺序”；配对的人为输入时间单列。

### P1：利用 CONNECT 已知的实际 BLE 间隔，去掉一次不必要的等待依赖

**代码确定的机会。** `ble_link.cpp:94–96` 每次连接把 `intervalObservation` 清零；`bleTick:203–214` 随后从未知状态发起快间隔更新。`BLEPowerPolicy::audioReady()` 必须 `actual>0` 才通过（`power_save_policy.h:39–41`）。即使 Mac 建立连接时已经给了可接受的 30ms，现有代码仍依赖后续 GAP 参数更新事件才能确认。

IDF 4.4.7 的 `ESP_GATTS_CONNECT_EVT` 已携带 **current** `conn_params.interval/latency/timeout`；其 BTC 实现也确实在派发回调前复制这三个值。可在 CONNECT 时发布这个已观测结果，由主循环初始化 policy；若实际间隔本来 ≤24 单位，沿用现有快速判据，避免冗余更新；初始间隔较慢或无效时继续现有请求／确认流程。[官方 GATTS 事件结构](https://raw.githubusercontent.com/espressif/esp-idf/v4.4.7/components/bt/host/bluedroid/api/include/api/esp_gatts_api.h)、[官方 BTC 事件实现，856–861 行](https://raw.githubusercontent.com/espressif/esp-idf/v4.4.7/components/bt/host/bluedroid/btc/profile/std/gatt/btc_gatts.c)

风险较低，但要保留连接 generation 隔离，并拒绝 0／非法值；接受慢连接后恢复快连接仍必须等确认，不能把历史的 30ms 当作当前值。验证初始 15／30／90ms、更新拒绝、更新不回调、断开后旧回调、空闲唤醒及音频开头。**当前可省多少时间待测**，不是固定省 2 秒；2 秒是现有参数请求超时界限（`power_save_policy.h:21`）。

### P1：减少 BLE 应用握手往返与首份大快照

**跨端协议机会。** `device.cpp:403–431` 将设备信息、nonce、认证和状态拆开；`ble_link.cpp:137–148` 每片实际业务数据不超过 `min(MTU−11,236)`，因此多次完整状态在 BLE 上会放大占用。现有通知最多两条待 BTC 完成、控制优先的流控是有意保留安全／写响应空间，不能直接取消上限。

可分两步做：先由 Mac 复用已经取得且 epoch 有效的认证状态，消除丢弃后再次请求；再对已有授权的快路径增加可选 `hello`（info + nonce）以及 auth 后的最小 ready snapshot，把能力／新 epoch／必要会话字段一起返回，其他设置在上线后补齐。旧客户端和旧固件保留原握手。

必须处理两个约束：nonce 当前仅 10 秒有效（`device.cpp:410–411,425`），首次 PIN 和钥匙串交互可能超时，所以仅已有凭据快路径合并，或先预取凭据，过期就重取 challenge；auth 当前在 `429` 排队 connect，但 epoch 要到 `640–643` 才更新，不能在 auth 分支直接附上旧 `published`。ready 响应需延迟到主循环处理 connect 并发布当前 epoch 后，且仍要首个有效 heartbeat 才有控制租约。收益可以表示为减少的请求次数／字节／分片数，时间仍需实测。

验证新旧固件互通、钥匙串慢读、nonce 过期／重放、候选密钥提交失败、认证后立刻断线、epoch 更新与 ready 的顺序、低 MTU、拥塞、同一连接重复 auth。不得为提速移除 BLE 加密或应用 HMAC。

### P1：把无线启动与非关键外设准备重叠，缩短冷启动／深睡唤醒

**顺序确定，节省幅度待测。** `main.cpp:88–139` 在无线开始前完成 IMU、深睡寄存器清理、显示／UI 及大量启动日志。可把启动拆成“最小安全启动”和“外围就绪”：先完成 M5 基础初始化、按键采样、NVS 日志／恢复出厂／模式事务检查、设备身份和凭据读取，再启动所选无线；IMU完整初始化、UI度量和音频外围准备在无线建联期间完成。`imu_service.cpp:261–262` 目前是同步 `bmi270_legacy_init`，不能未经测量声称其就是最大瓶颈。

先单独审计 `main.cpp:92` 的 200ms 固定 delay 是否只为串口稳定；可用串口可用条件或把日志延后，但需验证 USB 插拔和看门狗复位，避免盲删设备初始化必要时序。恢复出厂确认／清理期间不启用无线的约束（`main.cpp:122–127`，`connection_setup.cpp:98–106`）必须保留，不能为了早广播先泄露旧服务。所有尚未就绪的外设要显式报告 ready=false，不能提前允许姿态快捷键／录音。

PM1 深睡现有实现实际调用 `powerOff()`（`deep_sleep.cpp:107`），主控断电后只能完整冷启动。若用户目标是拿起立即操作，保留连接的在线待机应作为体验选项；当前深睡默认关闭（`device.cpp:516`），所以这不是默认重连慢的通用解释。IDF 区分普通 deep/light sleep 与可保持连接的 modem sleep／自动 light sleep；自动 light sleep 涉及 SDK 配置和外围锁验证，不是加一个 `delay` 就能完成。[IDF 4.4.7 Sleep Modes](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/system/sleep_modes.html)

### P2：Wi-Fi 记住最近成功信道，先做同网络快速尝试

**适用当前依赖，收益依环境。** 普通启动只保存 SSID／密码（`wifi_prov.cpp:78–91`），`main.cpp:136` 没传 channel/BSSID。Arduino 2.0.17 已默认 `WIFI_FAST_SCAN`、自动重连，不要把“打开快速扫描／自动重连”列为新功能；其 `WiFi.begin` 确实把可选 channel/BSSID 传入配置。[Arduino 2.0.17 WiFiSTA 源码](https://raw.githubusercontent.com/espressif/arduino-esp32/2.0.17/libraries/WiFi/src/WiFiSTA.cpp)

可保存最近成功连接的信道，下一次同 SSID 从该信道优先扫描。IDF 4.4.7 的 station channel 参数表示从指定信道开始找 AP；BSSID 则会限制目标 AP，不能长期锁死家中 mesh 的单一节点。第一版宜只做信道提示，若增加 BSSID 快路径则需短而有界的失败回退，重新按 SSID 找网；只在成功参数变化时落盘，避免每次重连写 NVS。[IDF 4.4.7 Wi-Fi 配置字段](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/network/esp_wifi.html)

验证 AP 自动换信道、mesh 节点切换、同名不同密码网络、弱信号、路由器重启和凭据更新。先测 STA start→STA connected 的分布；若主要耗时在 DHCP 或 Mac 服务发现，此优化作用有限。避免自动复用旧 IP 跳过 DHCP；可由用户在路由器做 DHCP 保留地址，但不应把它当本固件的普适方案。

### P2：Wi-Fi 建联短窗口暂时关闭 modem sleep

**有明确机制，需实测电量与时延。** 项目没有显式设置 Wi-Fi sleep；Arduino 2.0.17 ESP32-S3 默认 `WIFI_PS_MIN_MODEM`，收到 STA start 时应用这个配置。可只在 GOT_IP 后到控制／首帧就绪的短窗口，或用户主动开始时，使用 `WiFi.setSleep(false)`，完成后恢复策略。官方说明 modem sleep 的接收延迟可能达到 AP 的 DTIM；它主要影响关联后的收包与应用往返，不能声称会加速无线扫描或 DHCP 的全部阶段。[Arduino 2.0.17 WiFiGeneric](https://raw.githubusercontent.com/espressif/arduino-esp32/2.0.17/libraries/WiFi/src/WiFiGeneric.cpp)、[IDF 4.4.7 Station Sleep](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-guides/wifi.html#esp32-s3-wi-fi-power-saving-mode)

设定最长窗口，失败／断开也恢复；测不同 DTIM 路由器上的认证 RTT／首帧时间、丢包与电池电流。本项目运行时只开 Wi-Fi 或 BLE，现状不需要为并发双无线做折中。不要默认永久关闭 Wi-Fi 省电。

### P2：首次配网成功后原地转入运行，避免再次扫描、关联、DHCP

**存在确定的 2 秒等待与重复建网；工程改动比删除 delay 大。** `wifi_prov.cpp:187` 的门户成功路径会重启。受管改网还涉及事务持久化／回滚与之前的切换重启。可先优化普通门户路径：凭据原子保存成功后，停止 DNS 和门户 HTTP，退出 AP 保留 STA，重置 prov 状态，启动正式 HTTP/mDNS，使用已获得的网络继续首次配对。可移除的确定部分是门户成功后固定 2 秒等待；额外重启、外设启动、关联与 DHCP 耗时需测。

必须先实现明确 teardown：当前 `provActive()` 对 `PROV_OK` 仍为 true（`wifi_prov.cpp:70`），正式服务器受 `!provActive()` 阻挡（`ota.cpp:164`），门户与正式服务器使用同一 HTTP 端口，不能只删除 `ESP.restart()`。`deviceBegin` 里的身份、设置、配对及音频初始化现已做完，可复用；切换 AP+STA→STA 是否保留当前连接需实机验证。受管换网与 transport transaction 留到下一步，保留断电恢复和失败回滚语义。

验证首次配网、错误密码后重试、页面 `/status` 的最终成功可见、门户关闭、AP 消失、Mac重新加入家中网络、正式端口可用、NVS 写失败、保存成功后断电、旧模式回滚。这一项改善首次安装／换网，不是日常 BLE 重连。

### P3：不重启切换 Wi-Fi/BLE，作为独立生命周期改造

现有 `transport.cpp:38–75` 在保存事务、停止业务后固定安排 700ms 重启，随后重新走外设启动和目标无线建联。若经测量模式切换仍明显慢，可以保留“先关旧无线、再启新无线”的互斥行为，只重启无线服务而不重启整个设备，从而避免重复的显示/IMU/音频初始化。它改善的是主动切换，不是日常同模式重连。

这需要正式的 stop/start 生命周期：HTTP/WebSocket、UDP、mDNS、配网 DNS/AP、BLE GATT/绑定上下文、任务和队列都要释放或复用。当前 `bleBegin` 会分配队列/快照（`ble_link.cpp:151–160`），没有一套完整对称的运行时关闭入口，不能仅把 `ESP.restart()` 替换为再次调用 begin。还要保留事务持久化、目标失败回退、单 owner、新 epoch 和旧音频/按键失效；蓝牙控制器内存释放 API 对本启动周期有不可逆限制。先做多轮切换与内部堆/最大连续空闲块验证，再决定是否替代现有重启路径。[IDF 4.4.7 Controller 生命周期](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/bluetooth/controller_vhci.html)

## 不应优先实施的固件改动

- **直接换 NimBLE／升级 Arduino 3.x 或 IDF 5.x。** 当前是 Bluedroid API，换栈会触及安全绑定存储、GATT／CCCD、流控、DLE／PHY、音频背压与电源策略；官方说明 ESP-NimBLE 使用的底层 controller 与 Bluedroid 相同。没有证据证明换 host 就能缩短当前发现／认证／音频准备路径。应等埋点证明确有 stack 初始化／内部堆瓶颈后，独立原型评估。[IDF 4.4.7 NimBLE 概览](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/bluetooth/nimble/index.html)
- **只把广告从 30–60ms 改为更短，或扩大 MTU／提高发射功率。** 广播已较快，服务 UUID 也在主广播；DLE251 和2M请求已存在。更密广播只影响发现概率且增加功耗，不能消除应用串行等待；更大发射功率也不是近距离建联的已证实瓶颈。可以A/B测试短时快广播，但排在握手和启动顺序之后。
- **缓存 GATT handle／跳过加密、绑定或 HMAC。** 前者有固件服务表变化和系统缓存一致性风险；后者改变安全模型。保留 UUID发现、协议能力协商和认证，再减少重复工作。
- **删除 60秒 OTA健康确认。** `main.cpp:154–157` 与 `ota.cpp:147` 的60秒限制用于确认新固件与回滚，不阻止普通控制连接。不能把安全确认当作建联固定60秒，也不应为提速删去。
- **同时永久运行 Wi-Fi 与 BLE 自动竞速。** 当前 `main.cpp:130–139` 明确择一启动。双无线会改动内存／供电／共存、单主控租约与事务语义，不能作为小改；先把单通道与已授权跨通道恢复做好。

## 固件建议的验收边界

每个优化独立 A/B，除时延还检查成功率、P95、恢复次数、BLE断开reason／加密失败、Wi-Fi失败reason、音频首帧是否缺失及控制租约是否误判。需要把“短暂失联”“用户主动断开／锁定”“设备预计深睡”“设备重启／换网”分开；重连不能绕过Mac的连接意图。

当前能确定的是代码依赖、固定等待和可减少的协议往返。各阶段真实时长、短广播与信道提示的改善幅度、Wi-Fi modem sleep窗口的电量代价、PM1拿起到可用的物理时延，均待设备测量。

## 官方资料索引（固件部分）

- [PlatformIO espressif32 v6.12.0平台清单](https://raw.githubusercontent.com/platformio/platform-espressif32/v6.12.0/platform.json)
- [Arduino-ESP32 2.0.17版本](https://raw.githubusercontent.com/espressif/arduino-esp32/2.0.17/cores/esp32/esp_arduino_version.h) 与 [其 ESP32-S3 IDF版本](https://raw.githubusercontent.com/espressif/arduino-esp32/2.0.17/tools/sdk/esp32s3/include/esp_common/include/esp_idf_version.h)
- [Arduino 2.0.17 WiFiSTA](https://raw.githubusercontent.com/espressif/arduino-esp32/2.0.17/libraries/WiFi/src/WiFiSTA.cpp)、[WiFiGeneric](https://raw.githubusercontent.com/espressif/arduino-esp32/2.0.17/libraries/WiFi/src/WiFiGeneric.cpp)
- [ESP-IDF 4.4.7 ESP32-S3 Wi-Fi Driver](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-guides/wifi.html)、[Wi-Fi API](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/network/esp_wifi.html)
- [ESP-IDF 4.4.7 ESP32-S3 BLE GAP](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/bluetooth/esp_gap_ble.html)、[GATTS参数头](https://raw.githubusercontent.com/espressif/esp-idf/v4.4.7/components/bt/host/bluedroid/api/include/api/esp_gatts_api.h)、[BTC GATTS事件分发实现](https://raw.githubusercontent.com/espressif/esp-idf/v4.4.7/components/bt/host/bluedroid/btc/profile/std/gatt/btc_gatts.c)
- [ESP-IDF 4.4.7 Sleep Modes](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/system/sleep_modes.html)、[NimBLE概览](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/bluetooth/nimble/index.html)

## 验证方案与实施顺序

建议第一批做 **阶段埋点 → 连接事件初始 interval 复用 → 去除重复状态读取 → 音频准备并行**。先做边界清楚、易回归的改动；之后用分阶段数据决定启动重排、Wi-Fi AP 缓存和协议合并的先后。免重启切换属于单独的较大项目。

1. **分别定义完成点。** `T_authenticated` 为连接意图到首个已鉴权的新 epoch 完整基线；`T_control` 另计首个有效心跳建立控制租约的时点；`T_audio_ready` 还需本机桥、传输性能及音频租约就绪；`T_first_audio` 为显式开始传音后首帧到达/渲染，不能混入用户等待 PIN 或姿态触发的时间。还要统计失败率与取消行为，不能只挑成功快样本。
2. **记录阶段。** Mac 记录发现、connect 调用/回调、GATT、加密订阅、info、凭据、challenge/auth、首基线、音频准备、首有效心跳。固件记录上电/唤醒、外设初始化、广播或 Wi-Fi start/connected/got-IP、服务 ready、应用认证提交、首快照和音频性能 ready。沿用 attempt/generation 关联，不记录 key、PIN、nonce/proof 或音频内容。
3. **补充系统指标。** HTTP 用 URLSessionTaskMetrics 分解名称解析、TCP、请求/响应；重用连接时某些日期字段为空是正常现象。现有 HardwareCheck 是整体状态和切换记录，通常每 200 ms 检查一次，不足以解释几十毫秒级阶段变化。[当前 HardwareCheck][M-check]、[Apple HTTP 指标][H-metrics]
4. **场景分开测。** 已配对设备下重启 Mac 应用、BLE/Wi-Fi 设备冷启动、PM1 唤醒、Mac 解锁/唤醒、旧 IP/AP 换信道、首次配对、两种模式切换分别统计。探索每组至少 30 次，正式验收增加样本并看 p50/p95；同步记录 RSSI、MTU、实际 interval、AP 环境和供电方式。
5. **时间与回归。** 两端使用各自的单调时钟，通过 attempt 和共同事件关联；不能直接相减两台设备的未对齐时间戳。回归旧固件兼容、旧 epoch 拒绝、重新配对让位、取消后不重连、旧回调不误断新连接，以及语音就绪/按键释放。广播和快速重连还需配合耗电测试。

历史参考：`artifacts/wifi-ble-short-results-20261009.json` 记录四次模式切换，Wi-Fi 约 3.24/3.46 秒、BLE 约 5.66/6.59 秒。它属于 10 月 9 日旧实现，**不能代表 `a4f7442` 的当前基线，更不是本轮优化收益**。本轮未操作实体设备、重启连接、构建或刷写固件，没有当前版本的端到端时延实测。

### 本节源码与官方依据

[M-flow]: https://github.com/numbero/stick-s3_thin-cube/blob/a4f7442f39b03186b6a5f48c6d5df35707d23d9e/macos/Sources/ThinCube/DeviceModel.swift#L381-L470
[M-auth]: https://github.com/numbero/stick-s3_thin-cube/blob/a4f7442f39b03186b6a5f48c6d5df35707d23d9e/macos/Sources/ThinCube/DeviceModel.swift#L472-L481
[M-fw-auth]: https://github.com/numbero/stick-s3_thin-cube/blob/a4f7442f39b03186b6a5f48c6d5df35707d23d9e/firmware/src/device.cpp#L398-L431
[M-baseline]: https://github.com/numbero/stick-s3_thin-cube/blob/a4f7442f39b03186b6a5f48c6d5df35707d23d9e/macos/Sources/ThinCubeCore/BLEStatusSnapshots.swift#L4-L27
[M-audio-call]: https://github.com/numbero/stick-s3_thin-cube/blob/a4f7442f39b03186b6a5f48c6d5df35707d23d9e/macos/Sources/ThinCube/DeviceModel.swift#L875-L880
[M-audio]: https://github.com/numbero/stick-s3_thin-cube/blob/a4f7442f39b03186b6a5f48c6d5df35707d23d9e/macos/Sources/ThinCube/AudioBridge.swift#L12-L56
[M-teardown]: https://github.com/numbero/stick-s3_thin-cube/blob/a4f7442f39b03186b6a5f48c6d5df35707d23d9e/macos/Sources/ThinCube/DeviceModel.swift#L1108-L1128
[M-subscribe]: https://github.com/numbero/stick-s3_thin-cube/blob/a4f7442f39b03186b6a5f48c6d5df35707d23d9e/macos/Sources/ThinCube/BLEConnection.swift#L121-L151
[M-recovery]: https://github.com/numbero/stick-s3_thin-cube/blob/a4f7442f39b03186b6a5f48c6d5df35707d23d9e/macos/Sources/ThinCube/DeviceModel.swift#L1007-L1105
[M-ble-open]: https://github.com/numbero/stick-s3_thin-cube/blob/a4f7442f39b03186b6a5f48c6d5df35707d23d9e/macos/Sources/ThinCube/BLEConnection.swift#L76-L106
[M-ble-request]: https://github.com/numbero/stick-s3_thin-cube/blob/a4f7442f39b03186b6a5f48c6d5df35707d23d9e/macos/Sources/ThinCube/BLEConnection.swift#L172-L185
[M-http]: https://github.com/numbero/stick-s3_thin-cube/blob/a4f7442f39b03186b6a5f48c6d5df35707d23d9e/macos/Sources/ThinCube/DeviceModel.swift#L180-L183
[M-discovery]: https://github.com/numbero/stick-s3_thin-cube/blob/a4f7442f39b03186b6a5f48c6d5df35707d23d9e/macos/Sources/ThinCube/Discovery.swift#L5-L50
[M-mode]: https://github.com/numbero/stick-s3_thin-cube/blob/a4f7442f39b03186b6a5f48c6d5df35707d23d9e/macos/Sources/ThinCubeCore/ConnectionMode.swift#L18-L28
[M-gatt]: https://github.com/numbero/stick-s3_thin-cube/blob/a4f7442f39b03186b6a5f48c6d5df35707d23d9e/macos/Sources/ThinCube/BLEConnection.swift#L106-L126
[M-check]: https://github.com/numbero/stick-s3_thin-cube/blob/a4f7442f39b03186b6a5f48c6d5df35707d23d9e/macos/Sources/ThinCube/HardwareCheck.swift#L35-L56
[H-connect]: https://developer.apple.com/documentation/corebluetooth/cbcentralmanager/connect(_:options:)
[H-connected]: https://developer.apple.com/documentation/corebluetooth/cbcentralmanager/retrieveconnectedperipherals(withservices:)
[H-invalid]: https://developer.apple.com/documentation/corebluetooth/cbperipheraldelegate/peripheral(_:didmodifyservices:)
[H-best]: https://developer.apple.com/library/archive/documentation/NetworkingInternetWeb/Conceptual/CoreBluetooth_concepts/BestPracticesForInteractingWithARemotePeripheralDevice/BestPracticesForInteractingWithARemotePeripheralDevice.html
[H-metrics]: https://developer.apple.com/documentation/foundation/urlsessiontasktransactionmetrics/connectstartdate

## 固定提交的固件源码入口

上文 `文件:行号` 均可在以下固定版本核对，避免后续主干行号变化：

- [main.cpp：启动与主循环](https://github.com/numbero/stick-s3_thin-cube/blob/a4f7442f39b03186b6a5f48c6d5df35707d23d9e/firmware/src/main.cpp#L78-L157)
- [ble_link.cpp：GAP/GATT、安全、流控与性能策略](https://github.com/numbero/stick-s3_thin-cube/blob/a4f7442f39b03186b6a5f48c6d5df35707d23d9e/firmware/src/ble_link.cpp)
- [device.cpp：认证、会话、快照与心跳](https://github.com/numbero/stick-s3_thin-cube/blob/a4f7442f39b03186b6a5f48c6d5df35707d23d9e/firmware/src/device.cpp)
- [power_save_policy.h：实际 interval 与 audioReady](https://github.com/numbero/stick-s3_thin-cube/blob/a4f7442f39b03186b6a5f48c6d5df35707d23d9e/firmware/src/power_save_policy.h#L9-L42)
- [wifi_prov.cpp：配网与成功后重启](https://github.com/numbero/stick-s3_thin-cube/blob/a4f7442f39b03186b6a5f48c6d5df35707d23d9e/firmware/src/wifi_prov.cpp)
- [transport.cpp：模式事务](https://github.com/numbero/stick-s3_thin-cube/blob/a4f7442f39b03186b6a5f48c6d5df35707d23d9e/firmware/src/transport.cpp)、[ota.cpp：HTTP 启动与健康确认](https://github.com/numbero/stick-s3_thin-cube/blob/a4f7442f39b03186b6a5f48c6d5df35707d23d9e/firmware/src/ota.cpp)
- [deep_sleep.cpp：PM1 掉电唤醒](https://github.com/numbero/stick-s3_thin-cube/blob/a4f7442f39b03186b6a5f48c6d5df35707d23d9e/firmware/src/deep_sleep.cpp)、[connection_setup.cpp：恢复出厂约束](https://github.com/numbero/stick-s3_thin-cube/blob/a4f7442f39b03186b6a5f48c6d5df35707d23d9e/firmware/src/connection_setup.cpp)
- [imu_service.cpp：IMU 初始化](https://github.com/numbero/stick-s3_thin-cube/blob/a4f7442f39b03186b6a5f48c6d5df35707d23d9e/firmware/src/imu_service.cpp)、[control_lease.h：控制租约](https://github.com/numbero/stick-s3_thin-cube/blob/a4f7442f39b03186b6a5f48c6d5df35707d23d9e/firmware/src/control_lease.h)
- [platformio.ini：依赖锁定](https://github.com/numbero/stick-s3_thin-cube/blob/a4f7442f39b03186b6a5f48c6d5df35707d23d9e/firmware/platformio.ini)
