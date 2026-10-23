# ThinCube 局域网协议 v1

实现版本：设备 v0.4.18，工作区兼容 Mac v0.4.11；后续版本的兼容扩展见文末。设备 HTTP 端口 80，Bonjour `_thincube._tcp`，名称 `ThinCube-xxxxxx`。除 info 与 PIN 配对外，运行时请求均需 `X-ThinCube-Key`，32 位十六进制随机密钥。单个 Mac 控制连接拥有设备；新的鉴权连接接管并停止旧会话。旧 Mac v0.3.0 存在发送二进制控制帧的缺陷，应使用当前版本。

## HTTP

| 请求 | 内容 |
|---|---|
| GET `/api/info` | name、version、protocol=1、paired |
| POST `/api/pair` | `{pin:"六位数字"}`；两分钟物理开启窗口、每秒限速、最多十次；成功返回 key/name |
| GET `/api/status` | 当前发布的状态快照 |
| POST `/api/settings` | 可选 allowed/muted/localSound 布尔值，brightness 8…255 整数，gain 0.1…4，soundVolume 0…100 整数、soundMuted 布尔值；v0.4.60 起旧 deepSleep 设置忽略 |
| POST `/api/wifi` | `{ssid,password}`；SSID 1…32 字节，密码为空或 8…63 字节，进入热点验证新网络 |
| GET→WebSocket `/control` | 鉴权握手；控制与状态快照 |
| POST `/ota/begin` | `{ver,size,sha256}`；返回单次 token，设备等待 A 确认 |
| GET `/ota/status` | stage、pct、err |
| POST `/ota` | 原始 app.bin；额外头 `X-ThinCube-OTA: token`，Content-Length 必须精确匹配 |
| POST `/ota/cancel` | 请求安全取消；进入 verifying/done 后返回 409，不能承诺撤回提交 |
| POST `/api/maintenance` | v0.4.65：已配对控制端请求 `{requestId}`，接受后一次性重启进入手机升级热点；保留原连接配置 |

配网热点是独立服务：`/` 网页、GET `/scan`、GET `/status`、POST `/save` `{ssid,password}`；DNS 将域名指向 192.168.4.1。热点使用每台设备独立的 8 位随机数字密码，保存在 NVS，v0.3.1 自动迁移旧密码。Wi-Fi 凭据以单个 NVS blob 写入，避免断电造成新 SSID 与旧密码错配。

## 控制及快捷键边沿

快捷键映射由 Mac 本地保存和执行；设备只发送 A 键边沿。Mac v0.3.11 支持录制和直接选择两种方式，通过单一开关控制左右修饰键、仅修饰键及一个普通主键的组合，映射变化无需更改协议或重新烧录。录制或选择窗口打开期间仍确认设备边沿但不执行注入；录制完成或选择确认后即时保存新映射并释放原组合，下一次新的按下边沿才使用新绑定。 Mac 按键测试日志显示每个新边沿的序号及处理原因；重复传输不重复执行或刷屏，受阻边沿仍确认。系统注入和本窗口测试回读分别记录，事件发送权限未生效时不执行注入。

Mac 约每 100 ms 发送 `{op:"heartbeat",epoch,ack,port,ready}`。port 是 Mac 的 UDP 接收端口（1024…65535）；ready 表示音频桥就绪。设备返回最新状态快照；连接初始 epoch=0 的探测先获取设备随机 epoch，后续命令使用该值。

控制 JSON 使用 WebSocket 文本帧，固件兼容读取二进制 JSON 帧；每帧最多 768 字节，拒绝不支持的类型和无效 JSON，不遗留未读取载荷。

其他 op：`start`、`stop`、`calibrate_inverted`、`calibrate_faceup`，均携带 epoch。v0.3.5 新增 `calibrate_inverted`：屏幕面向用户、A 键在上、显示屏在下时校准；将当前竖轴符号取反，仍按正常竖持方向存储 NVS `upright`，因此原校准数据含义不变。旧 `calibrate_upright` 保留原语义以兼容旧客户端。校准命令只在符合固定轴姿态条件时生效，通过状态中的 `calibration:{seq,op,ok}` 确认本次结果（v0.3.6 起，每次实际处理的校准请求递增 seq）；`calibrated/faceCalibrated` 只说明存在已保存的校准。成功后停止会话并以本次已知姿态设置基准；屏幕朝上平放再倒立产生新的边沿并开始传音。无效姿态不修改旧校准。

v0.3.6 新增 `pose`（upright/inverted/face_up/face_down/moving）、`voiceBlock`（disabled/muted/busy/disconnected/calibration/sleeping/microphone/rearm/invert；传音中为空），以及可选校准回执 `calibration`。朝上平放终止姿态会话并释放快捷键，但保留手动开始的持续会话。旧版初次开机只观察当前姿态，只有新倒置动作可开启姿态会话；v0.4.62 起按下述当前姿态规则处理。

v0.4.62 不改变线协议：已确认的倒立姿态持续表示进入对讲的请求。倒立开机、在线息屏后拿起、PM1 深睡重启、Mac 音频连接恢复、临时忙碌及麦克风/无线就绪延迟结束后，只要语音已启用、未静音且当前健康姿态仍确认倒立，就自动建立姿态语音会话。新会话仍使用原 220 ms 姿态确认，不从未确认候选、失效 IMU 或正在形成其他候选的旧稳定姿态启动。停止、静音、禁用语音、校准和游戏隔离保留重新翻转要求；`voiceBlock=rearm` 只表示倒立请求被这些操作中止，可离开倒立后重新进入，或使用 Mac 手动开始。临时暂停立即释放旧快捷键；恢复时生成新的音频 session，并重新建立 A/B 按键上下文，已持有的按键必须松开后重按，不重放旧边沿或手势。手动会话不被姿态请求替换，普通姿态下失联的手动会话也不自动恢复。

v0.4.63 修复恢复边界：重新配对及解除配对使用临时暂停，取消、超时或完成后在合法音频连接和其他条件恢复时按当前倒立姿态启动；不会覆盖此前的显式停止。停止标记只由当前健康、候选与稳定姿态一致的非倒立姿态解除，翻转过程中旧稳定姿态不解除停止。待启动姿态会话在每次状态推进时都检查 IMU 健康，覆盖麦克风在本轮音频初始化后才就绪的路径；健康恢复后可继续启动，已经建立的会话仍沿用原倾斜容错。Mac v0.4.36 将连接切换的临时输出拦截与停止/连接初始化的待命确认标记分开，切换请求被拒绝或超时后不因缺少停止快照而长期静音；显式停止和旧按键丢弃继续生效。

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

v0.4.65 用强 C 符号 `verifyRollbackLater() = true` 覆盖 Arduino 2.0.17 默认的初始化早确认路径，真正将 pending 状态保留至应用自检。启动健康区分本地资源与外部网络：离线路由器不直接判失败；已关联 Wi-Fi 则要求本地 HTTP 服务完成注册。维护/配网门户及首次连接选择有各自资源条件。确认采用每会话 generation，先消费已有按压与松键，再接受新的 A 点击；声明版本必须与镜像描述一致。取消与接收→校验边界、超时与开始接收边界在同一锁内仲裁，主循环时间戳落后 HTTP 更新时不会下溢误判超时。

## v0.4.65 手机热点升级

正常 HTTP/BLE info 与状态提供 `maintenanceOTA:true`。已授权 Mac 经 `POST /api/maintenance {requestId:"UUID"}` 或 BLE `{op:"maintenance",requestId:"UUID",rid}` 请求，成功返回 `{accepted:true}`；同一待执行 requestId 幂等。错误包括 busy、boot_unverified、storage_failed、invalid_request。设备约 700 ms 后重启，仅消费一次 RAM/NVS 升级意图，不改原永久连接方式。设备按住正面 A 开机、约两秒后松开也可独立进入；启动 A+B 不进入升级或恢复出厂，B-only 保留恢复出厂确认。

维护态独占 AP-only + DNS + HTTP 80，SSID `ThinCube-Update-xxxxxx`；v0.4.66 起热点无需 Wi-Fi 密码，六位升级码及临时 token 每次重新生成；不注册普通 `/api/settings`、配对或 `/control`。接口：

| 请求 | 内容 |
|---|---|
| GET `/` | 完全离线的手机升级页面，内置 SHA-256，支持原始 File/Blob 上传 |
| GET `/maintenance/info` | device=`ThinCube-StickS3-v1`、name、version、level（电量）、maxSize、stage、err、ready |
| POST `/maintenance/session` | JSON `{code:"屏幕六位码"}`；返回 RAM token，五次错码后退避 30 秒 |
| `/ota/begin`、`/ota/status`、`/ota`、`/ota/cancel` | 维护态只认 `X-ThinCube-Maintenance: token`；上传仍额外携带单次 `X-ThinCube-OTA` |

token 不写 NVS、不替换 Mac key，退出/重启失效。页面先读镜像型号/版本并本地算摘要；发送进度与设备写入不同，只有上传最终 HTTP 200 且 `{ok:true}` 才报告写入校验成功，断线后看到 done 也不代替回包证明。接收中的同一 HTTP worker 不能并行响应状态轮询；设备屏幕显示实际写入进度。五分钟无有效操作退出，OTA 保护阶段不因闲置退出；取消只发信号，worker 安全收尾后才能退出。退出及成功重启恢复原 BLE/STA。细节见[实施说明](phone-hotspot-ota-v0.4.65.md)。

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

## 自动休眠（固件 v0.4.60 起）

固件统一执行从最后一次真实活动起五分钟在线息屏、十分钟纯电池深睡、二十分钟纯电池关机，不再提供开关。提前活动或唤醒后重新计时；USB、受保护任务及传感器/供电检查沿用原有保护。旧 NVS `deep` 不再读取或写入，HTTP / BLE `settings` 中旧 `deepSleep` 字段忽略，不能改变策略。

完整状态增加可选 `deepSleepUnified:true`，表示自动执行统一策略。为兼容旧客户端，继续提供 `deepSleep:true`、`deepSleepStaged:true`；`deepSleepAvailable` 仍表示当前启动的硬件准备是否通过，而非开关。新 Mac 移除设置入口，根据这些字段只读说明实际固件行为；没有统一策略字段的旧固件提示升级，不假装已启用新策略。BLE 快速状态仅在同 epoch 的完整基线中保留这些能力字段。通知与确认流程保持兼容，见下文和[统一休眠策略](unified-sleep-v0.4.60.md)。

### v0.4.18–v0.4.59 兼容记录

v0.4.58 扩展：完整状态增加可选 `deepSleepStaged:true`，表示深睡开关开启时，从最后一次真实活动起五分钟在线息屏、十分钟纯电池深睡、二十分钟纯电池完全关机。提前唤醒会取消旧关机计时并重新起算；开关关闭时仍采用五分钟息屏、十分钟纯电池关机。Mac 根据此能力显示相应说明，旧固件缺少字段时保留原策略说明；睡眠通知、确认和设置格式兼容。见 [5/10/20 分钟深睡策略](idle-stages-v0.4.58.md)。

旧版 HTTP `/api/settings` 与BLE `settings` 支持可选布尔值 `deepSleep`，默认false，保存至设备NVS。完整状态增加可选 `deepSleep`（保存的开关）、`deepSleepAvailable`（启动清理及BMI270检查通过）及 `sleep:{id,phase}`；`phase` 为 `preparing` 或 `awake`，`id` 为当前睡眠尝试的UInt32标识。BLE快速状态携带 `sleep`，Mac仅在同一epoch的完整状态基线中合并。新版 Mac 不再提供深睡开关。

### 睡眠通知与确认（继续适用）

设备准备深睡时通过当前控制通道发布 `sleep`。Mac先释放快捷键及音频缓冲，再发送 `{op:"sleep_ack",epoch,sleepId}`；`sleepId` 必须为1…2³²−1的整数，并匹配当前尝试，设备只接受当前控制所有者及epoch的确认。BLE无 `rid` 的确认与心跳一样不附加应用回执。已连接时，设备等待最多2秒；新活动、普通控制命令、失联或供电/忙碌条件变化取消睡眠。原本离线时可在350ms准备窗口后进入，新连接出现后则必须收到确认。等待期间 `/ota/begin` 返回409及 `error=sleep_pending`。

Mac仅将5秒内已确认的当前通知之后的断连显示为深睡待机，并保留正常自动重连；取消、新epoch及重新连接使旧通知失效。USB供电下不进入深睡。主控断电后A/B或Mac指令不能唤醒；运动或机身电源键是配置的唤醒源。实现与实机验证范围见 [v0.4.18记录](power-saving-v0.4.18.md)。

`power` 增加可选 `cpuMHz`（当前CPU频率）、`bleInterval`（实际BLE连接间隔，单位1.25ms；0表示未知/未连接）、`onlineSaving`（主循环的在线闲置省电模式）字段。BLE待机请求60–90ms，语音开始前要求已确认的连接间隔不超过30ms；恢复期间 `voiceBlock=radio`，Mac显示正在恢复语音连接速度。手动开始请求最多保留5秒，过期、停止、失联或禁用/静音时清除，音频采样格式不变。

## v0.4.21 启动指示灯与运行时常亮调试

默认自动控制电源指示灯：`M5.begin()` 返回后立即确认亮灯，覆盖后续初始化；主循环确认以下本机条件全部成立并连续运行2000ms后熄灯：显示缓冲及屏幕尺寸有效、至少完成一帧显示；IMU 有有效且一秒内新鲜的样本；麦克风配置与捕获队列初始化成功；控制队列、发布互斥量及本机初始化健康；BLE 服务启动成功或 Wi-Fi 工作模式已启用。正常请求采集时，还要求采集已启动并已取得音频帧；静音、休息或配网等主动停止采集的状态无需等待音频帧。

任一条件失败或观察期间两次主循环检查间隔超过250ms，重新计算连续就绪时间；启动未就绪时保持亮灯，每五秒串口输出 `[boot-ready] waiting` 及各模块状态，成功时输出一次 `local startup stable`。完成后本次启动的就绪状态锁定，后续离线、休眠或闲置不重新进入启动指示。此判断不要求 AP 可达、Wi-Fi 连接成功、Mac 连接、配对、倒置校准或传音，不修改60秒 OTA 健康确认及90秒失败回滚逻辑。熄灯表示本机启动检查通过，不替代 OTA 确认或完整硬件检测。

HTTP `/api/settings` 与 BLE `settings` 增加可选布尔值 `powerLedDebug`：`true` 为“运行时常亮（调试）”，`false` 为默认自动。非布尔值拒绝，鉴权与排队行为沿用既有设置协议。完整状态包含 `powerLedDebug`（保存的选择）、`powerLedReady`（本次本机启动就绪）、`powerLedOn`（最后确认的输出）及 `powerLedAvailable`（该输出已成功写入读回）。不可用时 `powerLedOn` 不表示当前物理灯状态；固件最多每秒重试一次，确认恢复后更新状态。就绪、灯输出或可用性变化与设置命令均触发新的完整 BLE 快照，快速状态沿用同一 epoch 的完整基线。

选择保存于 `thincube/ledDebug`，缺少配置时默认false；兼容读取 v0.4.20 原型的 `pwrLed` 选择，第一次调试设置保存后以 `ledDebug` 为准。无论保存的选择如何，启动阶段始终亮灯；启动中关闭调试仅恢复自动，不能提前熄灯。就绪后开启调试立即亮灯，关闭立即熄灯。硬件写入读回并保存成功后才发布新的选择；失败时尝试恢复原策略的输出，不将失败请求发布为成功。

通过固定 M5Unified 的 `M5.Power.M5pm1.setLedEnLevel()` 只修改 `PWR_CFG (0x06)` bit4，保留充电、DCDC、LDO 和外部5V位。PM1 在业务固件运行前的复位/下载闪灯保持硬件原有行为。Mac v0.4.13 在“设备 → 设备设置”的亮度下方提供“运行时常亮（调试）”，采用设备确认值，等待期间禁用重复操作，3秒未确认会提示重试，断连取消等待。旧固件缺少 `powerLedDebug` 或指示灯暂不可控时禁用开关。

硬件依据：[M5PM1 官方手册 V1.9](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/M5PM1_Datasheet_CN.pdf) 的 `PWR_CFG` 位定义及 LED 指示状态；固定驱动 [M5PM1_Class.inl](https://github.com/m5stack/M5Unified/blob/8b63555e7bd9ac71978c55ad868fd1d1d3b2ef3e/src/utility/power/M5PM1_Class.inl#L106)。本次没有电流或放电实测，关闭常亮灯的续航收益尚未量化。

验证包括启动条件失败/恢复、连续两秒门限、主循环停顿、实际 IMU 数据失效及过期、静音离线配网、调试切换、重启与计时回绕。macOS Swift 测试需要 Mac；当前云端未连接设备，未烧录或进行真机灯光验证。Mac 上应运行 `bash scripts/build-macos.sh`，由脚本完成签名并替换 `/Applications/ThinCube.app`；固件包通过现有升级流程安装。

云端验证结果：`bash scripts/test-local.sh` 的24个 C++ 测试程序及5个 Python 测试通过；新增启动 LED 测试在固件所用的 C++11 下再次通过。`pio run -d firmware` 最终编译成功，RAM 105592 / 327680 字节，Flash 1719209 / 2097152 字节。`python3 scripts/package-firmware.py` 生成 `artifacts/thincube-v0.4.21.bin` 及清单，镜像型号、版本、大小、SHA-256 与最终编译产物一致性校验通过。Linux 跳过 macOS Swift 测试，未构建或安装 Mac 应用。

真机验收：分别在 Wi-Fi / BLE、USB / 电池供电下确认启动亮灯、就绪两秒后熄灯，Wi-Fi 不可达、无 Mac、未配对、静音、朝下休息和配网均可正常进入就绪；本机初始化失败时灯保持亮并能从串口定位条件。确认调试选择切换、重启恢复、启动中关闭调试不能提前熄灯，以及充电与原有供电功能正常。验证断连、旧固件、设置失败时界面不误报成功。实际电流与续航需另行测量。


## v0.4.41 对讲机 A/B 快捷键

完整状态增加可选 `keyProtocol:2`（能力）、`keyMode:1|2`（当前协商结果）、`heldB`；旧 `held` 与 BLE flags 中 held 位仍表示 A。新 Mac 在当前 owner/epoch 的鉴权心跳中发送 `keyProtocol:2` 后，设备启用双键，并在原 `edge:{seq,down}` 中增加 `key:"a"|"b"`。新旧队列不并行，A/B 共用原有 32 项有序队列与队首 `ack`。未协商时只发送旧 A 边沿，B 保持旧功能，避免旧 Mac 将 B 当作 A。快速 BLE 状态额外附带 `keyMode/heldB`，原七项 `s` 数组及 flags 位布局保持不变。非法来源、模式或非布尔 held 值被新 Mac 拒绝。

仅在 A 原有快捷键可用的倒置对讲机条件下接管 B：按下保持、松开释放，B 未绑定时不执行操作；对讲机内 B 不静音、长按不切换连接、A+B 不重新配对。普通模式和系统页面保持原行为。按住 B 退出模式后消费这次松手；预按 A/B 进入时各自需松开再按。绑定保存在 Mac，B 不默认复制 A，左右修饰键设置沿用共享开关。

新连接重置协商；协商变化清空旧队列并重新武装，语音会话开始/结束清除旧队列。Mac 按来源去重、确认并管理共享合成按键；松开 A 不释放 B 仍持有的键。epoch、语音 session、协商模式变化，以及原有停止/锁屏/编辑/超时等条件释放全部来源，受阻事件不在恢复后重放。详情与实测边界见 [B 键实现记录](walkie-b-shortcut-v0.4.41.md)。

## v0.4.44 连续双甩离散事件

完整状态新增 `gestureProtocol:1`、`gestureMode:0|1`、`gesture:null|{seq,type:"double_flick",ageMs}`。当前 owner/epoch 的鉴权心跳发送 `gestureProtocol:1,gestureAck:UInt32` 后启用，旧客户端不协商则不发送双甩事件。A/B 的 `keyProtocol/ack/edge` 保持独立，不改变保持语义。

双甩事件队列上限 4，序号跳过 0，匹配队首的 `gestureAck` 出队；未确认前重复报告，超过 750 ms 丢弃。离开有效对讲状态、控制超时、按下 A/B、新 owner 或协商改变清空队列。BLE 快速状态增加 `gestureMode/gesture`，即使没有事件也携带 `gesture:null`；原 `s` 数组与 flags 保持不变。

Mac 在同一 epoch 中按序号消费一次；首次进入新语音/协商上下文、编辑/未设置/禁用期间、未知类型与过期事件均确认丢弃，之后不补放。有效事件在 Mac 构造完整按下和松开后一次提交，禁止把手势当作持续 held，禁止等待下一包松键。按设备保存独立映射。实现及验收边界见 [双甩快捷键](double-flick-shortcut-v0.4.44.md)。
