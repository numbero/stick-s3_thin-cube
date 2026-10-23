# 手机连接设备热点升级固件调研

日期：2026-10-23。范围：核对当前 ThinCube 固件、既有手机配网页、OTA 协议与 Espressif 官方实现，提出可落地的手机浏览器升级方案。本轮只调研；未修改固件或 Mac 客户端，未构建、安装、烧录或读写设备。

后续用户已授权实施，当前实现与验证范围见[手机热点升级 v0.4.65](phone-hotspot-ota-v0.4.65.md)。以下保留调研时的现状与建议。

## 结论

**可以实现，而且已有大部分底层能力。推荐在设备上增加临时「手机升级」维护模式：手机连接 ThinCube 热点，在完整浏览器中打开 `http://192.168.4.1/`，选择事先下载的应用 `.bin`，设备按 A 确认后通过现有 OTA 引擎写入备用槽，校验完成再重启。** 不需要手机 App、路由器或云端，也不要求先完成家庭 Wi-Fi 配网。[现有热点](../firmware/src/wifi_prov.cpp#L134)、[现有 OTA](../firmware/src/ota.cpp#L44)

「已有手机配网，所以通过同一条 Wi-Fi 链路传固件应该不难」这个判断基本正确。新增工作主要是把维护热点、手机网页、临时鉴权和设备 UI 接起来，复用已有 Flash 写入与校验。但不能只在配网页加一个文件按钮：当前配网与 OTA 互斥，OTA 需要 Mac 配对密钥，UI 的配网分支也会挡住 OTA 按键。[服务启动条件](../firmware/src/ota.cpp#L163)、[鉴权](../firmware/src/device.cpp#L554)、[UI 优先级](../firmware/src/ui.cpp#L617)

**调研发现一个应在实施前处理的现有缺口：当前构建产物链接了 Arduino 默认的启动即确认 OTA 路径，仓库期望的「运行 60 秒再确认、90 秒不健康回滚」不能直接认为已生效。** 应修正此路径并做真实崩溃/断电回滚试验，再承诺新固件有故障会自动恢复。详见下文「回滚核验」。

## 当前已经有什么

| 能力 | 当前证据与限制 |
| --- | --- |
| 设备热点与手机网页 | `ThinCube-Setup-xxxxxx`，每设备 8 位随机数字密码存 NVS，APSTA、通配 DNS、门户探测重定向、内嵌 HTML/JS 已有。当前页面只有扫描、填写和保存路由器凭据。[实现](../firmware/src/wifi_prov.cpp#L134) |
| OTA 接口 | `POST /ota/begin` 接收 `{ver,size,sha256}`；`GET /ota/status`；`POST /ota` 接收原始 app 二进制。当前三者都要求 `X-ThinCube-Key`，上传另需单次 `X-ThinCube-OTA` token。[接口](../firmware/src/ota.cpp#L38)、[协议](../docs/thincube-protocol-v1.md#L5) |
| 设备端防护 | 已知且新鲜的电量至少 50%；传输切换、深睡、校准等忙碌条件拒绝；设备 A 确认；确定长度；首块芯片与 ThinCube 型号校验；分块 SHA-256；`esp_ota_end`；最后才设置启动分区。[准入](../firmware/src/ota.cpp#L44)、[上传](../firmware/src/ota.cpp#L70) |
| 双分区 | `app0`、`app1` 各 2 MiB，`otadata` 8 KiB；只升级 app，NVS、bootloader、分区表均不在这条 OTA 写入路径中。[分区](../firmware/partitions.csv#L1) |
| 可分发固件 | 已有打包脚本生成 `thincube-<ver>.bin` 和 `firmware-manifest.json`，含型号、版本、长度、SHA-256。[打包脚本](../scripts/package-firmware.py#L13) |
| 版本与容量 | PlatformIO 钉在 `espressif32@6.12.0`；本机安装 Arduino-ESP32 2.0.17、ESP-IDF 4.4.7。当前源码版本 v0.4.64。只读查看既有 `firmware.bin` 为 1,804,192 字节，距 2 MiB 上限剩 292,960 字节；这不是本轮新编译结果。[构建配置](../firmware/platformio.ini#L7) |

历史 [配网与 OTA 设计](../docs/wifi-prov-ota.md) 自己已注明过时。其中固定热点密码、浏览器上传设想、开机持 B 强制配网等描述不能作为当前功能依据。当前开机持 B 进入的是恢复出厂确认，手机升级应有独立且不清数据的入口。[启动入口](../firmware/src/main.cpp#L117)

## 官方方案是否支持

Espressif 的 Arduino-ESP32 2.0.17 自带 `WebUpdate` 示例：浏览器选择文件，经 `WebServer` 上传事件调用 `Update.begin/write/end`，完成后重启。这直接证明浏览器传应用固件是官方支持的常规路径。不过该示例启动服务仍要求 STA 连接成功，不能把其中 `WIFI_AP_STA` 一行当作「示例已经完成独立热点升级」；本项目需要自行接通现有 SoftAP 服务。[官方 WebUpdate 示例](https://github.com/espressif/arduino-esp32/blob/2.0.17/libraries/WebServer/examples/WebUpdate/WebUpdate.ino)

当前项目已经直接使用更底层的 `esp_ota_begin/write/end/set_boot_partition`，并有型号、SHA-256、物理确认、取消和超时保护。**推荐继续复用这一实现；无需为手机再引入另一套 `WebServer + Update` 写入逻辑。** 官方 `Update` 本身同样会找下一个 OTA 分区、检查容量并最终切换启动分区，迁过去不会自动解决本项目的鉴权和状态机问题。[官方 Update 源码](https://github.com/espressif/arduino-esp32/blob/2.0.17/libraries/Update/src/Updater.cpp)、[本地写入引擎](../firmware/src/ota.cpp#L70)

## 推荐用户流程

1. 手机先从发布入口下载 `thincube-vX.Y.Z.bin`，保存到本机文件中；不要依赖连接设备热点后仍能访问互联网。
2. 按住设备正面 A，再短按电源/复位键开机或重启；看到「手机升级」提示后松开 A，进入维护热点。设备显示热点名称、密码和 `192.168.4.1`。也可从已连接的 Mac 固件页发起，详见下节。此入口不恢复出厂，不改日常 BLE/Wi-Fi 模式或已有配对。
3. 手机连接热点，选择保留无互联网连接，在 Safari/Chrome 中手动打开完整地址。自动弹出的门户可做提示，但不作为唯一入口。
4. 网页选择本地 `.bin`，显示从镜像读取的型号、当前版本、目标版本、文件大小；本地算 SHA-256，预检查明显错误。
5. 网页申请本次升级会话，设备再次显示目标版本；用户按 A 才允许实际上传，B 取消。确认与等待上传沿用现有 30 秒窗口。
6. 网页用原始二进制上传，显示「发送进度」；设备屏幕显示真实写入进度。最终响应成功后显示「写入及镜像校验完成，设备正在重启」。
7. 重启后退出临时维护模式，恢复原 BLE/STA 日常路径，保留配对、网络、音量和校准等 NVS 设置。试启动健康确认完成后才算此次升级正常；手机与热点断开本身不能证明新版本健康。

以上为推荐产品流程，尚未实现或实机验收。第一版接受单个 `.bin` 即可；无需要求用户另选 JSON 清单。现有 `.bin` 偏移 288/320 已有设备标识与版本，网页读取规则必须与固件/打包脚本共用约定；升级 SDK 后重新核验布局。[镜像描述](../firmware/src/firmware_identity.cpp#L1)、[打包检查](../scripts/package-firmware.py#L14)

## 如何进入与退出手机升级模式

以下为新增交互建议，当前固件尚未提供这些入口。推荐设备物理入口作为必备能力，Mac 按钮作为便利入口；手机升级可以在没有 Mac、没有可用路由器时独立完成。

| 入口 | 推荐操作 | 适用场景 |
| --- | --- | --- |
| 设备本身，必备 | 按住正面 A，随即短按电源/复位键开机或重启；启动阶段 A 单键稳定按住约 2 秒，看到「手机升级」提示后松开，开启热点。时间以真机手感调整。 | 手机直接升级，设备未连接 Mac，或保存的路由器不可用 |
| Mac 固件页，辅助 | 增加「用手机升级…」按钮，说明设备会暂时断开；点击后通过当前已认证的 BLE 或 Wi-Fi 通道发送请求，设备以一次性维护启动重启并开热点。 | Mac 正常连接时，用户希望少操作设备按键 |

设备入口使用开机阶段的 A，保留现有运行时长按 A 诊断、长按 B 切连接、A+B 长按配对及开机持 B 恢复出厂的用途。启动时同时按 A+B 不解释为手机升级；应给出松键重试提示，避免歧义。进入时整次按住及松开事件必须消费，等全部松键后再接受新的点击，防止入口操作触发后续确认。[运行时按键](../firmware/src/ui.cpp#L634)、[启动 B 入口](../firmware/src/main.cpp#L117)、[已有松键保护](../firmware/src/connection_setup_policy.h#L7)

进入热点本身可以撤销，无需再单独按 A 确认一次。手机选定具体固件后，仍由设备显示目标版本并要求一次新的 A 点击才写入。Mac 经 BLE 发起维护请求也不必先切换到日常 Wi-Fi 模式；需要新增对应能力声明与控制命令，旧固件显示升级提示，不能假定现有命令已支持。[当前固件页](../macos/Sources/ThinCube/ThinCubeApp.swift#L481)、[当前 BLE 操作分发](../firmware/src/device.cpp#L438)

这是独立、临时的维护状态，不增加永久的第三种连接模式，不调用现有 `transportSetMode()` 改写原 BLE/Wi-Fi 选择。维护期间停止日常控制与音频，屏幕显示「手机升级」、热点名称、密码、本地地址及 `B: EXIT`；原配对、校准和网络配置保留。[现有模式与持久化](../firmware/src/transport.cpp#L34)

退出规则建议：

- 等待连接、选文件时，短按 B 退出；连续 5 分钟无有效用户操作自动退出（建议初值，系统门户探测请求不续期）。退出通过正常重启恢复原模式。
- 上传时 B 取消本次升级，等待 OTA worker 完成中止，再显示可重试/退出页面。接收、校验和提交过程中不因普通空闲超时直接重启。
- 上传失败留在维护页，允许重新选文件或按 B 退出；升级成功自动重启并恢复原模式。手动复位也不应再次自动进入维护。

物理入口无需持久化维护标记。Mac 入口需要跨一次重启：鉴权及统一互斥检查通过后保存单个一次性请求，启动时先清除并提交，成功后才开 AP；清除失败报错，避免每次重启都进入维护。此请求与长期 transport 记录分开保存。

启动仲裁需要先处理恢复出厂日志、未完成的配网/连接切换事务，以及新镜像的 `PENDING_VERIFY`。新固件尚在试启动时提示「固件正在自检，请稍后再试」，继续正常健康检查，不提前标 valid 来进入热点。现有 `transportBegin()`、`provPrepareChange()` 会恢复并写入事务，因此应在这类正常初始化副作用之前统一裁决维护请求。按键采样还需与 `deepSleepBoot()` 的定时关机处理协调，验证从关机、深睡和正常运行重启都可进入。[启动顺序](../firmware/src/main.cpp#L90)、[模式恢复](../firmware/src/transport.cpp#L24)、[网络事务恢复](../firmware/src/wifi_prov.cpp#L118)

此入口依赖应用固件能够运行到启动页面；应用无法启动时仍使用 USB 恢复，不将其描述为独立于应用的硬件救援模式。

## 实现方案与改动边界

### 1. 临时维护模式，复用热点基础设施

优先新增独立的 AP-only 升级维护态，复用热点名称/密码、DNS、离线页面基础设施。退出或重启后恢复之前的日常连接模式。进入维护态时停止控制/音频会话、释放按键，并统一禁止深睡、网络切换、配网提交等相冲突的动作；进入、失败重试、退出都要有可恢复路径。

APSTA 也可实现，但上传期间应冻结扫描、路由器验密和 STA 自动重连：ESP32-S3 的 STA 与 SoftAP 共用信道，STA 所连路由器信道优先，改变 STA 连接可能使热点迁移信道。选择 AP-only 是为了减少首版变量，不是芯片限制 OTA 必须 AP-only。[官方 Wi-Fi Home Channel](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-guides/wifi.html#home-channel)

当前 `otaTick()` 只在日常 Wi-Fi 已连接且 `!provActive()` 时启动 OTA HTTP 服务；当前门户与运行时服务各自想监听 80 端口。因此应共用一个维护 httpd，把「OTA 路由注册」与「启动 STA 运行时服务」拆开，例如 `otaRegisterRoutes(server, authPolicy)`；维护服务仅注册页面、会话和 OTA 所需路由，避免为了借用服务启动而注册完整 `/control`、设置与配对接口。[OTA 服务](../firmware/src/ota.cpp#L120)、[运行时接口注册](../firmware/src/device.cpp#L534)

不要仅解除 `provActive()` 的启动限制。`uiTick()` 先进入配网分支并 `return`，会导致后面的 `tickOta()` 收不到 A/B；必须让维护/配网/OTA 的屏幕、按键、电源准入由同一互斥状态决定。[UI 分支](../firmware/src/ui.cpp#L617)

### 2. 手机用临时 OTA 权限，不重新配对 Mac

手机浏览器没有 Mac 的长期 `X-ThinCube-Key`。直接复用现有配对接口会产生新 key，可能替换当前 Mac 的绑定；把长期 key 放进热点页面也会扩大权限。[鉴权与配对状态](../firmware/src/device.cpp#L554)

建议在物理进入维护模式后，使用屏幕显示的临时会话码换取随机短时维护 token；token 仅保存在 RAM，仅允许 OTA begin/status/upload，限单个活跃会话，退出、超时或重启立即失效。仍保留上传前设备 A 确认和既有单次 OTA token。对该模式的三个 OTA 接口统一使用相同的权限策略，不触碰已有 Mac key，不放开全局 `deviceAuthorized()`。

这是本项目的权限设计建议；官方最小 WebUpdate 示例没有替我们完成这些约束。单靠知道热点密码并不等于持有长期设备控制权。

### 3. 原始文件上传，保持现有协议

沿用 `POST /ota/begin {ver,size,sha256}` → 按 A → 轮询至 ready → `POST /ota` 原始 `File/Blob`。保留设备独立校验；网页的检查仅改善报错体验。官方示例用 multipart 是因为 Arduino WebServer 负责解析，而当前 `/ota` 读取的是原始镜像，直接传 `FormData` 会把边界与字段头误当作固件。[现有长度与首块检查](../firmware/src/ota.cpp#L70)

网页可以采用 `XMLHttpRequest.send(file)`，由浏览器产生请求长度；不要手设 `Content-Length`，它是浏览器禁止脚本控制的请求头。避免传未知长度的 `ReadableStream`；当前 IDF 4.4.7 `httpd_req_recv` 不支持 chunked 请求，目标手机浏览器是否实际发出预期长度应纳入抓包/服务日志验收。[WHATWG XHR](https://xhr.spec.whatwg.org/#the-send()-method)、[WHATWG 禁止请求头](https://fetch.spec.whatwg.org/#forbidden-request-header)、[IDF HTTP 接收 API](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/protocols/esp_http_server.html#_CPPv414httpd_req_recvP11httpd_req_tPc6size_t)

**SHA-256 有一个容易漏掉的网页限制：不能假定 `crypto.subtle.digest()` 在 `http://192.168.4.1` 可用。** WebCrypto 把 `subtle` 标为 SecureContext，普通 HTTP 私网 IP 不属于 HTTPS/localhost 的默认安全例外。推荐把经过验证的 SHA-256 JavaScript 实现随页面内嵌或本机提供，原生 WebCrypto 可用时作为优化；不要让升级页依赖 CDN、外部字体、云 API 或安装证书。[W3C WebCrypto](https://www.w3.org/TR/webcrypto/#crypto-interface)、[W3C Secure Contexts](https://www.w3.org/TR/secure-contexts/#is-origin-trustworthy)

### 4. 进度与完成状态必须分清

当前上传 handler 同步循环接收和写入，在同一个 httpd 服务任务上执行。多开 socket 并不等于同一服务可以同时运行 `/ota/status`；不能设计成上传时不断轮询它就必定获得实时 Flash 进度。[本地同步 handler](../firmware/src/ota.cpp#L80)、[IDF HTTP 任务模型](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/protocols/esp_http_server.html#overview)

第一版保持简单：XHR upload progress 标为「发送」，设备屏幕以现有 `written` 显示「写入」；发送到 100% 后网页继续等待最终响应。只有 `esp_ota_end` 与 `esp_ota_set_boot_partition` 成功才显示写入校验完成；这仍与重启后健康确认是两个阶段。如果未来要求网页显示实时设备进度，再单独设计并发处理/第二服务机制。[成功返回位置](../firmware/src/ota.cpp#L108)、[XHR 上传进度](https://xhr.spec.whatwg.org/#event-xhr-progress)

## 断线、断电与回滚核验

### A/B 写入提供什么保护

ESP-IDF OTA 把新 app 写到非当前启动槽，验证后再更改 `otadata`；`otadata` 由两个独立 Flash 扇区和序号支持断电恢复。本项目分区与写入顺序符合这一路径。上传未完成、SHA 不匹配或取消时不切换启动槽；可以重新上传。[官方 OTA 原理](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/system/ota.html#ota-process-overview)、[本地失败与切槽顺序](../firmware/src/ota.cpp#L108)

这保护的是 app 升级路径，不是对所有硬件故障的「永不变砖」保证。应分别验收写入中断电、写完切槽前后断电、新 app 初次启动断电。只 OTA app 也不能改变已经安装的 bootloader 或修正错误分区表；首次启用此能力必须先让设备运行支持热点维护和正确 A/B 布局的固件，可以通过现有 Mac OTA 或 USB 预装。[分区边界](../firmware/partitions.csv#L1)

### 回滚核验：本轮发现的真实缺口

官方首次试启动回滚要求 bootloader 启用 `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`，新应用保持 `PENDING_VERIFY` 到自检通过；未确认即重启会放弃新应用，回到仍可启动的旧应用。有效回滚还依赖旧槽可用，不能只看分区数量。[官方 rollback 状态机](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/system/ota.html#app-rollback)

本机 Arduino 2.0.17 SDK 配置已开启 bootloader/app rollback，未开启 anti-rollback；不能误报成「项目完全没有启用 rollback」。不过 Arduino 核心还有一层：`initArduino()` 调用弱符号 `verifyRollbackLater()`，默认 false；随后默认 `verifyOta()` 返回 true，会立即确认待验证镜像。[官方 SDK 配置](https://github.com/espressif/arduino-esp32/blob/2.0.17/tools/sdk/esp32s3/sdkconfig)、[官方启动实现](https://github.com/espressif/arduino-esp32/blob/2.0.17/cores/esp32/esp32-hal-misc.c)

只读检查本机既有构建产物得到：

- `firmware.elf`：`verifyOta @ 0x421401b0`、`verifyRollbackLater @ 0x421401b8` 均为弱符号；反汇编分别返回 1 和 0。
- `firmware.map` 显示二者来自 `libFrameworkArduino.a(esp32-hal-misc.c.o)`，不是项目覆盖。
- `initArduino()` 反汇编包含对 pending 状态调用 `esp_ota_mark_app_valid_cancel_rollback()` 的路径。

因此，**对这份既有构建产物，默认早确认路径已经链接**；[主循环的 60 秒健康确认](../firmware/src/main.cpp#L164) 和 [90 秒回滚逻辑](../firmware/src/ota.cpp#L146)不能代表真正的回滚窗口。没有读实机，不能断言板上当前镜像、bootloader 与这份产物完全一致。

实施时应以 C 链接覆盖 Arduino 的延迟确认 hook，例如 `extern "C" bool verifyRollbackLater() { return true; }`，让项目现有自检决定何时 mark valid；重新检查符号、启动日志和实际 OTA 状态，并使用可控制的早期崩溃镜像验证回滚。不要仅增加一个编译 `-D` 就假定预编译库和板上 bootloader 已改变。

修好延迟确认后还要检查新维护模式的健康依赖：`deviceHealthy()` 的 Wi-Fi 分支要求 `runtimeRegistered`，它现在由完整运行时 `deviceRegister()` 设置。若升级后长期停在 AP-only，而既未注册运行时也未调整健康判定，就可能 90 秒后被判断不健康。推荐重启后恢复正常 BLE/STA 启动，并把「无需外部路由器的设备自身健康」与「已连上用户网络」的依赖明确分开；不要为了标健康在维护 AP 暴露所有控制接口。[健康判定](../firmware/src/device.cpp#L601)、[注册标志来源](../firmware/src/device.cpp#L534)

## 镜像校验与签名边界

当前型号标识、芯片检查与 SHA-256 可以阻止明显错文件、截断和意外损坏；`esp_ota_end` 进一步验证 ESP app 镜像。但用户同时提供文件与其哈希，不代表文件一定由 ThinCube 发布者制作；可编辑的型号字符串也不是签名。当前 SDK 未开启 Secure Boot 或 signed-app 验证，不应把该方案描述为「只接受官方签名固件」。[当前检查](../firmware/src/ota.cpp#L97)、[官方 SDK 安全配置](https://github.com/espressif/arduino-esp32/blob/2.0.17/tools/sdk/esp32s3/sdkconfig)

首版个人维护工具可以沿用「可信发布下载 + 临时热点权限 + 按键确认 + 完整性校验」。若面向公开分发并需要防止非官方固件，另做设备端公钥验签与签名发行流程。IDF 支持不启用硬件 Secure Boot 的 OTA 签名验证，但需要适当的 SDK 构建配置；完整 Secure Boot V2 又涉及 eFuse、bootloader 和密钥管理，属于单独的安全工程，不是给网页加校验按钮。[IDF signed OTA](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/system/ota.html#secure-ota-updates-without-secure-boot)、[Secure Boot V2](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/security/secure-boot-v2.html)

## 手机与无互联网热点

Apple 官方说明在无互联网的门户网络中选择「Without Internet」可以继续保持连接；直接取消登录可能离开网络。因此页面和设备提示应指导用户保留连接，再用完整浏览器打开本地地址。[Apple 官方说明](https://support.apple.com/en-gb/102554)

以下是实现约束或待实机验证事项，不能仅凭当前配网页已有门户探测 URL 就声称已经支持：

- **文件必须真正在手机本机。** 文件 App 中的云盘占位文件能否在无网时读取，取决于手机状态；流程应先下载完成，再连设备热点。
- **自动门户作为辅助入口。** 自动门户窗口的文件选择器、页面重载和长请求行为需要 iOS/Android 实测；首版以完整 Safari/Chrome 手动 IP 入口作为验收目标。
- **锁屏、切后台、断线按失败可重试设计。** 不承诺后台持续上传，不在第一版做断点续传；用已有 abort、超时和旧槽保护支持整包重试。
- **蜂窝网络、自动切换、VPN、私有 DNS、HTTP 自动升级的具体表现需实测。** 不要求用户常态关闭所有网络保护；明确使用 `http://192.168.4.1/` 并提供最小可复现诊断。
- **页面与 API 同源。** 由设备自己提供离线页面并相对路径请求，减少云端页面跨域访问私网设备的额外权限与兼容成本；浏览器策略仍按目标版本验收，不承诺绕过其限制。[Chrome 本地网络访问说明](https://developer.chrome.com/blog/local-network-access)

## 建议实施顺序与验收

| 阶段 | 要完成的工作 | 通过标准 |
| --- | --- | --- |
| 1. 修复并证明回滚 | 延迟 Arduino 默认确认，核对板上 bootloader/分区/试启动状态 | 新镜像首启保持 pending；健康后 valid；首启故意崩溃或断电能回旧槽 |
| 2. 接通维护模式 | 非破坏入口、AP-only、单个 httpd、OTA 路由与临时鉴权、UI/休眠互斥 | 原 BLE/STA、Mac key、配网和校准均保留；退出/失败可恢复；多手机不抢写 |
| 3. 离线手机页面 | 本地文件、内嵌 SHA-256、版本/型号提示、原始上传、准确进度措辞 | iOS Safari、Android Chrome 在无互联网热点下均可完整升级；不依赖 crypto.subtle/CDN |
| 4. 故障和回归 | 全包重试、取消、错误文件、掉电、升级后健康 | 旧版可启动，临时权限失效，Mac 正常重新连接，升级没有要求重新配对 |

最低故障矩阵应覆盖：小于 352 字节、错误 chip/型号、SHA 不符、超过 2 MiB、长度不符；电量未知/低于 50%/采样超过 15 秒；未按 A/超时/按 B；两个浏览器抢会话；上传中手机锁屏/离开热点；写入、切槽与首启断电；无已配网路由器时健康确认；BLE 与 Wi-Fi 两种日常模式升级后恢复。实际上传耗时、内存峰值和剩余 Flash 以新构建与真机测量为准，不在调研阶段承诺秒数。
