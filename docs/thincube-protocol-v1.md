# ThinCube 局域网协议 v1

实现版本：设备 v0.4.18，工作区兼容 Mac v0.4.11；后续版本的兼容扩展见文末。设备 HTTP 端口 80，Bonjour `_thincube._tcp`，名称 `ThinCube-xxxxxx`。除 info 与 PIN 配对外，运行时请求均需 `X-ThinCube-Key`，32 位十六进制随机密钥。单个 Mac 控制连接拥有设备；新的鉴权连接接管并停止旧会话。旧 Mac v0.3.0 存在发送二进制控制帧的缺陷，应使用当前版本。

## HTTP

| 请求 | 内容 |
|---|---|
| GET `/api/info` | name、version、protocol=1、paired |
| POST `/api/pair` | `{pin:"六位数字"}`；两分钟物理开启窗口、每秒限速、最多十次；成功返回 key/name |
| GET `/api/status` | 当前发布的状态快照 |
| POST `/api/settings` | 可选 allowed/muted/localSound/deepSleep 布尔值，brightness 8…255 整数，gain 0.1…4，soundVolume 0…100 整数、soundMuted 布尔值 |
| POST `/api/wifi` | `{ssid,password}`；SSID 1…32 字节，密码为空或 8…63 字节，进入热点验证新网络 |
| GET→WebSocket `/control` | 鉴权握手；控制与状态快照 |
| POST `/ota/begin` | `{ver,size,sha256}`；返回单次 token，设备等待 A 确认 |
| GET `/ota/status` | stage、pct、err |
| POST `/ota` | 原始 app.bin；额外头 `X-ThinCube-OTA: token`，Content-Length 必须精确匹配 |

配网热点是独立服务：`/` 网页、GET `/scan`、GET `/status`、POST `/save` `{ssid,password}`；DNS 将域名指向 192.168.4.1。热点使用每台设备独立的 8 位随机数字密码，保存在 NVS，v0.3.1 自动迁移旧密码。Wi-Fi 凭据以单个 NVS blob 写入，避免断电造成新 SSID 与旧密码错配。

## 控制及快捷键边沿

快捷键映射由 Mac 本地保存和执行；设备只发送 A 键边沿。Mac v0.3.11 支持录制和直接选择两种方式，通过单一开关控制左右修饰键、仅修饰键及一个普通主键的组合，映射变化无需更改协议或重新烧录。录制或选择窗口打开期间仍确认设备边沿但不执行注入；录制完成或选择确认后即时保存新映射并释放原组合，下一次新的按下边沿才使用新绑定。 Mac 按键测试日志显示每个新边沿的序号及处理原因；重复传输不重复执行或刷屏，受阻边沿仍确认。系统注入和本窗口测试回读分别记录，事件发送权限未生效时不执行注入。

Mac 约每 100 ms 发送 `{op:"heartbeat",epoch,ack,port,ready}`。port 是 Mac 的 UDP 接收端口（1024…65535）；ready 表示音频桥就绪。设备返回最新状态快照；连接初始 epoch=0 的探测先获取设备随机 epoch，后续命令使用该值。

控制 JSON 使用 WebSocket 文本帧，固件兼容读取二进制 JSON 帧；每帧最多 768 字节，拒绝不支持的类型和无效 JSON，不遗留未读取载荷。

其他 op：`start`、`stop`、`calibrate_inverted`、`calibrate_faceup`，均携带 epoch。v0.3.5 新增 `calibrate_inverted`：屏幕面向用户、A 键在上、显示屏在下时校准；将当前竖轴符号取反，仍按正常竖持方向存储 NVS `upright`，因此原校准数据含义不变。旧 `calibrate_upright` 保留原语义以兼容旧客户端。校准命令只在符合固定轴姿态条件时生效，通过状态中的 `calibration:{seq,op,ok}` 确认本次结果（v0.3.6 起，每次实际处理的校准请求递增 seq）；`calibrated/faceCalibrated` 只说明存在已保存的校准。成功后停止会话并以本次已知姿态设置基准；屏幕朝上平放再倒立产生新的边沿并开始传音。无效姿态不修改旧校准。

v0.3.6 新增 `pose`（upright/inverted/face_up/face_down/moving）、`voiceBlock`（disabled/muted/busy/disconnected/calibration/sleeping/microphone/rearm/invert；传音中为空），以及可选校准回执 `calibration`。朝上平放终止姿态会话并释放快捷键，但保留手动开始的持续会话。初次开机观察当前姿态不自动开始；只有新倒置动作可开启姿态会话。新动作允许麦克风启动短暂延迟，静音/停止/断线等会清除待启动动作。

v0.3.7 不改变线协议：竖向语音姿态稳定确认 220 ms，FaceUp 为 160 ms，FaceDown 仍为 1500 ms。显示方向独立以 90 ms 确认，并逐帧播放目标 180 ms 的 180° 旋转过渡；平放保持最近屏幕方向，不代表仍在语音会话。确认时间从有效样本进入候选区开始，不包含用户翻转过程、采样与调度开销。

状态包含 epoch、allowed、muted、localSound、streaming、manual、held、micReady、inverted、calibrated、faceCalibrated、gain、brightness、level、audioFrames（主循环已取出的 480 样本帧累计数）、audioDropped（采集队列溢出累计数）、ota、bootVerified、session，以及可选 `edge:{seq,down}`。

按键每个按下/释放边沿保留在设备队列中，Mac 去重执行并用 ack 确认队首，未确认前重复发送。队列上限 32，溢出停止会话并释放。禁止仅靠当前 held 快照推导边沿，短按可能发生在两个快照之间。

设备超过 1000 ms 未收到有效就绪心跳即停止传音。Mac 超过 400 ms 无有效状态即释放按键并清空音频会话，2 秒无状态断开。新的 epoch、静音、禁止语音、停止、锁屏、休眠、OTA 或退出均清理快捷键与缓冲。网络停止与按键释放没有第三方识别成功的含义。

## UDP 音频

每包 988 字节，48 kHz、单声道、S16LE，每帧 480 样本（10 ms）。

| 字节偏移 | 长度 | 内容 |
|---|---|---|
| 0 | 4 | ASCII `TCM1` |
| 4 | 16 | 每次开始会话生成的随机 session nonce |
| 20 | 4 | uint32 大端包序号，从 0 开始、按模 2³² 回绕 |
| 24 | 4 | uint32 大端采样时钟，seq × 480 按模 2³² 回绕 |
| 28 | 960 | 480 个小端有符号 16 位样本 |

接收端仅接受状态快照中的当前 session、正确长度、魔数与时钟。nonce 防止缓冲中的旧会话回放，不提供网络加密或消息完整性认证。设备向控制连接的 IPv4 地址发送，不接受由客户端任意指定目标 IP。

当前 SDK 的双栈 HTTP 服务返回的对端可能是 IPv4 映射 IPv6 地址；必须使用完整 sockaddr_storage，识别 AF_INET 或 ::ffff:a.b.c.d 后提取 IPv4 地址，不将 sockaddr_in6 截断为 sockaddr_in。依据：[ESP-IDF v4.4.7 HTTP server 源码](https://github.com/espressif/esp-idf/blob/v4.4.7/components/esp_http_server/src/httpd_main.c)。本版不支持向原生 IPv6 对端发送音频。

采集任务在缓冲完成回调中复制帧并立即补充同一缓冲，8 帧有界队列交给主循环。主循环每轮最多取出 4 帧，避免慢网络持续占用主循环。停止采集先终止回调补充，再等待 Mic.end 完成并清空队列；所有帧仅在 RAM 中流转。

Mac 使用固定 64 帧槽、约 40 ms 初始预缓冲、序号去重与重排、丢帧补零及 ±0.2% 的缓冲占用修正；长间断重新预缓冲。音频回调不阻塞等待网络锁，竞争时输出静音。单声道复制到 BlackHole 两个通道；总端到端延迟仍需真机测量。

## OTA 镜像及健康确认

仅接受 352 字节至 2 MiB app 镜像。首块要求 ESP 镜像 magic、ESP32-S3 chip_id=9、SDK app descriptor magic，以及文件偏移 288 处 32 字节零填充 `ThinCube-StickS3-v1` 标识。偏移 320 是 32 字节固件版本。自定义描述放在 `.rodata_custom_desc`，由当前固定 SDK 链接布局提供；更换 SDK 时必须重新核验偏移并同步客户端。

状态：idle → confirm → ready → receiving → verifying → done；失败进入 failed，B 返回日常。确认与等待上传各 30 秒；上传总时限 120 秒，连续无数据 10 秒终止。HTTP worker 独占 OTA 写入句柄，主循环只发送取消信号。物理确认及首块身份校验通过之前不擦除备用槽。精确长度、SHA-256、esp_ota_end 验证通过后才设置启动分区。

启动健康确认要求至少运行 60 秒：屏幕缓冲正常、IMU 已更新、运行时资源创建成功、必要时已经收到麦克风帧；持久静音或完全关闭采集时不强制为检查开启麦克风。90 秒仍 pending 则请求无效镜像回滚。依赖设备安装具备 rollback 支持的 bootloader 与本项目 A/B 分区表；app OTA 不能修复不兼容 bootloader 或分区布局。

v0.3.8 增加 `pose=landscape`，仍为可选字符串字段。横置稳定确认 160 ms，结束姿态语音并释放快捷键，保留 Mac 手动会话；横屏 A 用于本地探索，不产生快捷键边沿。显示四方向独立以 90 ms 确认，平放保留最近方向。配网、PIN 配对和 OTA 强制使用完整竖屏布局，不进入探索玩法。

## v0.3.9 Clawd 音效

`POST /api/settings` 增加可选 `soundVolume`（0…100 整数）和 `soundMuted`（布尔值）。默认 35/false；0 或静音均禁止播放，静音不改音量。独立于麦克风 `muted`、输入 `gain` 和本地声音反应 `localSound`，设置保存至设备 NVS `sndVol/sndMute`。音量范围映射至扬声器 0…191/255，所有供电状态均保持同一上限。

状态增加可选字段 `soundVolume/soundMuted/soundPlaying/soundCount`，旧客户端忽略即可；新客户端连接旧固件时隐藏不可用控件并提示升级。`soundCount` 是本次启动成功提交的音效次数，并不证明用户听到声音。鉴权 WebSocket 命令 `{op:"sound_preview",epoch}` 使用当前控制连接所有者和 epoch，播放唤醒音；受相同的静音与语音优先策略约束，不保证每次请求均播放。

短电子音与唤醒、A 互动、横屏扫描/发现、声音引发的好奇、晕眩和入睡动作关联。单个待播放音效最多保留 350 ms；传音、待启动姿态、倒置候选（语音可用时）、配网、配对窗口和 OTA 均丢弃请求。手动开始语音立即终止正在播放的音效，再恢复麦克风；中断后不补播。Mac 音量滑块停止变化 300 ms 后写入，减少连续 NVS 写入。

麦克风和扬声器共用硬件通路：播放前停止并等待麦克风回调退出，音效使用固定 16 kHz PCM 缓冲交给播放任务；结束或抢占后 Speaker.end，再按当前采集策略立即恢复麦克风。音效结束后 500 ms 屏蔽本地声音动作，防止 Clawd 对自己的电子音反复回应；不屏蔽语音采集。

## v0.4.0 兼容扩展

v1 Wi-Fi API 和 TCM1 音频保持兼容。新增 BLE GATT、TCB3 IMA ADPCM 音频（上位机兼容早期 TCB2 / TCB1）、transport_switch / transport_commit 和状态字段，详见 [Wi-Fi / BLE 实现](wifi-ble-implementation.md)。运行时 B 3 秒松手改为切换模式；空闲时 A+B 同时按住 3 秒松开两键重新配对。

设备 v0.4.4 / Mac v0.4.3 将两种传输统一为一次屏幕 PIN 应用配对，BLE 自动协商并保存链路加密密钥，无需系统验证码。目标模式启动即 `committed`，不再因宿主机未连接而回退；`transport_commit` 保留为幂等兼容接口。详细流程与验证见 [一次配对与持久模式](pairing-v0.4.4.md)。

## v0.4.15 电源诊断扩展

完整状态新增可选 `power` 对象；BLE 快速状态每次有新电源样本时附带该对象（约 1 秒一次，传音时也发送），其余快速状态沿用同一连接 epoch 的缓存。旧客户端忽略该字段，新客户端仍支持不提供电源信息的旧固件。`level` 始终为麦克风电平，不复用为电量。

`power` 字段：`boot` 为本次启动随机标识；`ms` 为本次采样的 MCU 毫秒数（UInt32，可回绕）；`rawMv/medianMv/filteredMv` 为电池电压；`percent/otaPercent` 为显示/保守 OTA 估计；`source` 为 0 电池、1 外部、-1 未知；`vinMv` 为外部输入电压；`charging/chargeKnown/low` 为布尔值；`cutoffMv` 为启动读取的 PM1 低压保护配置；`failures` 为本次启动电池 I2C 读取失败累计数。无效电压和未知百分比使用 -1；中值、滤波值可以保留此前样本，是否可用于显示由 `percent` 的新鲜度结果决定。低压保护配置仅供诊断，固件不修改它。

Mac 0.4.8 在 `~/Library/Application Support/ThinCube/power.jsonl` 记录新样本和断连事件，按设备/boot/ms 去重并拒绝同一启动内倒序的旧样本。每份最多 2 MiB，保留当前和 `power.previous.jsonl` 两份。日志包含电源值、亮度、无线方式、传音/音效状态；不记录 PCM、密钥、PIN 或会话 cookie。断连不能单独证明低电关机，必须结合断连前的数据分析。USB `power` 命令可读回 PM1 配置并输出当前电源状态，不修改配置或刷新活动计时。

v0.4.16 起另有可选 `chip/model/actualBrightness/idleMs/cal` 字段；`chip` 为12位十六进制设备标识，`model` 为显示换算表标识，亮度字段记录实际背光，`cal` 为标定状态诊断。v0.4.17 正常使用 `model=lipo-ref-an4189` 的通用 LiPo 分段参考表，屏幕直接显示百分比；该版本禁用标定启动入口，正常 `cal.state=0`。这些新增字段不改变 `percent=-1` 表示未知的语义。

## v0.4.18 在线省电与可选深睡

HTTP `/api/settings` 与BLE `settings` 增加可选布尔值 `deepSleep`，默认false，保存至设备NVS。完整状态增加可选 `deepSleep`（保存的开关）、`deepSleepAvailable`（启动清理及BMI270检查通过）及 `sleep:{id,phase}`；`phase` 为 `preparing` 或 `awake`，`id` 为当前睡眠尝试的UInt32标识。BLE快速状态携带 `sleep`，Mac仅在同一epoch的完整状态基线中合并。旧固件缺少这些字段时，客户端禁用深睡控件。

设备准备深睡时通过当前控制通道发布 `sleep`。Mac先释放快捷键及音频缓冲，再发送 `{op:"sleep_ack",epoch,sleepId}`；`sleepId` 必须为1…2³²−1的整数，并匹配当前尝试，设备只接受当前控制所有者及epoch的确认。BLE无 `rid` 的确认与心跳一样不附加应用回执。已连接时，设备等待最多2秒；新活动、普通控制命令、失联或供电/忙碌条件变化取消睡眠。原本离线时可在350ms准备窗口后进入，新连接出现后则必须收到确认。等待期间 `/ota/begin` 返回409及 `error=sleep_pending`。

Mac仅将5秒内已确认的当前通知之后的断连显示为深睡待机，并保留正常自动重连；取消、新epoch及重新连接使旧通知失效。USB供电下不进入深睡。主控断电后A/B或Mac指令不能唤醒；运动或机身电源键是配置的唤醒源。实现与实机验证范围见 [v0.4.18记录](power-saving-v0.4.18.md)。

`power` 增加可选 `cpuMHz`（当前CPU频率）、`bleInterval`（实际BLE连接间隔，单位1.25ms；0表示未知/未连接）、`onlineSaving`（主循环的在线闲置省电模式）字段。BLE待机请求60–90ms，语音开始前要求已确认的连接间隔不超过30ms；恢复期间 `voiceBlock=radio`，Mac显示正在恢复语音连接速度。手动开始请求最多保留5秒，过期、停止、失联或禁用/静音时清除，音频采样格式不变。
