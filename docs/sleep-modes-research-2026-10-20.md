# StickS3 睡眠模式调研：芯片睡眠与板级断电

调研日期：2026-10-20（Asia/Shanghai）。范围：ESP32-S3-PICO-1-N8R8、StickS3 / M5PM1，以及本仓库当前 Arduino ESP32 2.0.17 / ESP-IDF 4.4.7。本文仅记录调研，没有修改、构建或烧录固件。

后续固件 v0.4.60 已统一自动执行 5/10/20 分钟策略并移除深睡开关，见[最终统一策略及验证记录](unified-sleep-v0.4.60.md)。下文“当前项目”部分保留调研时 v0.4.59 的实现记录。

## 先分清两个层次

StickS3 采用 **ESP32-S3-PICO-1-N8R8**，集成 8 MB Flash 和 8 MB Octal PSRAM；N8R8 的 SPI 存储器工作电压为 3.3 V。[StickS3 官方规格](https://docs.m5stack.com/en/core/StickS3)、[PICO-1 数据手册 v1.2，表 1-1](https://www.espressif.com/sites/default/files/documentation/esp32-s3-pico-1_datasheet_en.pdf)

**ESP32-S3 的睡眠**是在主控仍有供电的前提下，关闭部分内部电路；**M5PM1 的 L1/L0**可以直接切断主控电源。本项目界面称为“深睡”的模式实际采用后者。二者名称相近，但保留能力和唤醒路径不同。[M5Stack 电源分级](https://docs.m5stack.com/en/arduino/m5sticks3/m5pm1)、[当前实现](../firmware/src/deep_sleep.cpp)

## ESP32-S3 支持的模式

官方 S3 数据手册列出 **Active、Modem-sleep、Light-sleep、Deep-sleep** 四种电源模式；其中 Active 是运行态，Modem-sleep 是无线省电态，SDK 的主要睡眠 API 是 Light-sleep 与 Deep-sleep。[S3 数据手册 v2.2，4.1.3.5](https://documentation.espressif.com/esp32-s3_datasheet_en.pdf)

| 模式 | 主 CPU 与内存 | 无线连接 | 醒来后程序如何继续 | 适用需求 |
|---|---|---|---|---|
| Active，正常运行 | CPU 运行，正常使用 SRAM / PSRAM | 按应用工作 | 正常执行 | 显示、声音、交互、数据处理 |
| Modem-sleep，无线省电 | CPU 继续运行，可降频；正常使用应用内存 | 驱动在无线事件之间暂停 RF，可保留连接 | 无需重新启动 | 设备要在线、仍需处理按键或动作 |
| Light-sleep，浅睡 | CPU 暂停；保留执行上下文和应用内存，部分电路门控或按配置断电 | 手动直接睡眠通常断连；驱动协调的自动浅睡是另一种机制 | 从睡眠调用之后继续，不重新跑应用启动 | 要更低待机电流，又希望快速恢复 |
| Deep-sleep，芯片深睡 | CPU、大部分 SRAM、APB 数字外设断电；可按需要保留 RTC 域 | 原连接不能持续在线，需要重连 | 经过唤醒启动流程重新加载应用；不是接着原调用继续 | 接受离线与重新启动，需要定时或硬件事件唤醒 |

模式行为依据 [IDF 4.4.7 Sleep Modes](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/system/sleep_modes.html)。Light-sleep 可以通过配置保存/恢复 CPU 上下文后关闭 CPU 电源，因此不能简单地把“CPU 是否物理断电”作为浅睡与深睡的唯一分界；**能否无感继续原程序**更有区分价值。[IDF 4.4.7 配置项 `CONFIG_PM_POWER_DOWN_CPU_IN_LIGHT_SLEEP`](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/kconfig.html#config-pm-power-down-cpu-in-light-sleep)

### 内存保留的实际含义

- **普通 SRAM、任务栈、堆和 PSRAM**：浅睡恢复需要保留其内容。PSRAM 的电源必须保留；Flash / PSRAM 共享供电，不能为了降低电流直接关掉这条电源又期待 PSRAM 数据不变。[官方低功耗说明，Flash / PSRAM 供电限制](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/low-power-mode/low-power-mode-soc.html)
- **芯片 RTC RAM**：S3 有 8 KB RTC FAST 和 8 KB RTC SLOW，可在深睡中保留指定代码/数据；是否保留由睡眠配置、唤醒源和变量属性决定，不是整个应用内存自动保留。[S3 数据手册，4.1.2.1](https://documentation.espressif.com/esp32-s3_datasheet_en.pdf)
- **深睡后普通应用状态**：其他 RAM 不应当作有效的保留数据；必须把需要的状态显式放入 RTC RAM 或非易失存储。Flash / NVS 保存的数据可以跨断电保存。[IDF 4.4.7 Deep Sleep Wake Stubs](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-guides/deep-sleep-stub.html)
- **PM1 L1/L0 主控断电**：连 ESP 自身的 RTC RAM、RTC 定时器与 ULP 都失去供电。M5PM1 自己的计时器和 RTC RAM 是另一套硬件；当前二十分钟关机标记保存在 PM1 RTC RAM 中。[本项目计时实现](../firmware/src/deep_sleep_timer.h)

### 哪些事件可以唤醒

以下是当前 IDF 4.4.7 对 S3 的能力，实际引脚还受 StickS3 接线、引脚占用和外设供电影响。[官方唤醒源说明](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/system/sleep_modes.html#wakeup-sources)

| 唤醒来源 | Light-sleep | ESP Deep-sleep | 关键约束 |
|---|---|---|---|
| RTC 定时器 | 支持 | 支持 | 参数以微秒表达，实际精度受 RTC 时钟影响 |
| EXT0 / EXT1 外部输入 | 支持 | 支持 | 使用 RTC GPIO；EXT0 与 EXT1 的供电和逻辑条件不同 |
| 电容触摸 | 支持 | 支持 | 芯片具备能力，板上的普通 A/B 按键不等于触摸电极 |
| ULP 检测事件 | 支持相应睡眠配置 | 支持 | 必须预先加载、启用 ULP 程序并保留所需 RTC 电源域 |
| 普通 GPIO 电平 | 支持 | 当前此 API 不支持 | `gpio_wakeup_enable()` 可使用数字 GPIO；深睡走 RTC EXT0/EXT1 |
| UART RX | 支持 | 不支持该方式 | 触发唤醒的字符可能丢失，不能当作完整接收首包 |
| Wi-Fi / BLE 无线事件 | 自动睡眠驱动按需协调 | 不能维持深睡前连接 | 不等于随便调用睡眠 API 后就能远程唤醒 |

这里的“芯片支持”不代表 ThinCube 已经实现这些入口。例如 A/B 按钮可作为浅睡 GPIO 输入，但当前 PM1 L1 会切断 ESP 供电，A/B 没有由此自动变成 PM1 唤醒按钮。

### 无线连接：手动浅睡和自动浅睡要分开

直接调用 `esp_light_sleep_start()` 前，SDK 要求应用处理无线停止；普通浅睡不能据此承诺原 Wi-Fi / BLE 连接仍在。保留连接需要驱动与 Modem-sleep、自动 Light-sleep 协调，按协议时间表醒来收发。

自动浅睡依赖编译时的 `CONFIG_PM_ENABLE`、FreeRTOS Tickless Idle，以及运行时 `esp_pm_configure()`；活动任务、定时器和外设持有的电源锁也可能阻止进入浅睡。**单纯 `delay()` 或手动降低 CPU 频率不等于进入了 Light-sleep。** [IDF 4.4.7 Power Management](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/system/power_management.html)

**后续核对源码后的修正：IDF 4.4.7 本身支持 S3 的 BLE 保连自动浅睡，当前 Arduino 预编译包没有启用所需配置。** 该版本通用电源管理文档写有 Bluetooth 持有 `ESP_PM_NO_LIGHT_SLEEP` 的限制，但 S3/C3 控制器源码只在特定配置下创建该锁；配置主晶振在浅睡时保电后，有不持有该锁的路径。不能据通用文档断言整个 IDF 4.4.7 都不支持。[IDF 4.4.7 通用驱动说明](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/system/power_management.html#dynamic-frequency-scaling-and-peripheral-drivers)、[S3/C3 控制器 `bt.c`](https://github.com/espressif/esp-idf/blob/v4.4.7/components/bt/controller/esp32c3/bt.c#L1136-L1254)

同版本官方 `bluetooth/nimble/power_save` 示例提供 `sdkconfig.40m.esp32s3`，启用 BLE Modem-sleep、主晶振低功耗时钟、浅睡期间主晶振保电；通用配置启用 PM 与 Tickless Idle。因而存在沿用现有主晶振的官方路径，不应先假定必须增加外部 32 kHz 晶振或升级 IDF。该示例采用 NimBLE，本项目使用 Bluedroid GATT；它证明底层能力存在，不能代替当前双核应用、协议栈和外设组合的实机验证。[同版本示例](https://github.com/espressif/esp-idf/tree/v4.4.7/examples/bluetooth/nimble/power_save)、[S3 40 MHz 配置](https://github.com/espressif/esp-idf/blob/v4.4.7/examples/bluetooth/nimble/power_save/sdkconfig.40m.esp32s3)、[公共配置](https://github.com/espressif/esp-idf/blob/v4.4.7/examples/bluetooth/nimble/power_save/sdkconfig.defaults)

### ULP 是深睡中的协处理器，不是第五种睡眠

S3 有两种 ULP：FSM 适合简单测量/条件判断，使用专用指令或宏；RISC-V 可以用 C 编写，适合更复杂的低速判断。它们在主 CPU 深睡时工作、访问 RTC 内存与相关低功耗外设，条件满足后再唤醒主 CPU。两者不是让原来的 FreeRTOS 应用继续运行，也不能维持正常 BLE / Wi-Fi 协议栈。[IDF 4.4.7 ULP FSM](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-guides/ulp.html)、[IDF 4.4.7 ULP RISC-V](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-guides/ulp-risc-v.html)

对于本板的“拿起唤醒”，BMI270 已能独立检测动作并通知 PM1，通常无需保持 ESP 的 ULP 供电来重复做同一件事；这是根据电路和现有方案作出的工程判断，并非比较实测。

### 不把 Hibernation 列为 S3 的独立第五档

本次核对 S3 系列 v2.2、PICO-1 v1.2 数据手册和 IDF 4.4.7 / 最新 S3 睡眠说明，**没有把 Hibernation 列为 S3 的独立模式或入口**。应按 S3 的四种官方模式以及 Deep-sleep 内部电源域选项理解。不能把其他 ESP32 型号教程中的“Hibernate 5 µA”等名称、电流直接搬过来。数据手册另列的 **Power off（CHIP_PU 低）**也属于关断，不是保留运行状态的第五种睡眠。[S3 数据手册，4.1.3.5 / 表 5-10](https://documentation.espressif.com/esp32-s3_datasheet_en.pdf)、[PICO-1 数据手册，表 3-9](https://www.espressif.com/sites/default/files/documentation/esp32-s3-pico-1_datasheet_en.pdf)

最新 IDF 文档还列了 `DSLP_ULTRA_LOW`、`DSLP_DEFAULT`、`LSLP_DEFAULT` 等内部子模式，它们是所需电源域/时钟/外设的组合，不能直接当作当前 4.4.7 SDK 已有的新用户模式。[新版 Sleep Modes，Sub Sleep Modes](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/sleep_modes.html#sub-sleep-modes)

## 官方电流参考：先看测试条件

### 芯片 / SiP 参考，不是 StickS3 电池输入电流

PICO-1 v1.2 表 3-8、3-9 明确提示 PSRAM 可能让实际电流更高。表 3-9 没有列出板载 IMU、PM1、DCDC、LCD、音频、USB 等损耗。[PICO-1 数据手册，3.6.2](https://www.espressif.com/sites/default/files/documentation/esp32-s3-pico-1_datasheet_en.pdf)

| 芯片状态与条件 | 官方典型参考 |
|---|---:|
| Modem-sleep，40 MHz、双核 WAITI、所有外设时钟关闭 | 13.2 mA |
| Modem-sleep，160 MHz、双核 WAITI、所有外设时钟关闭 | 27.6 mA |
| Modem-sleep，240 MHz、双核 WAITI、所有外设时钟关闭 | 32.9 mA |
| Light-sleep，VDD_SPI / Wi-Fi 断电、GPIO 高阻，相关 SPI 脚上拉 | 240 µA，另计 PSRAM |
| 8 MB Octal PSRAM / 3.3 V 的 Light-sleep 附加参考 | 140 µA |
| Deep-sleep，RTC RAM 与 RTC 外设保电 | 8 µA |
| Deep-sleep，仅 RTC RAM 保电、RTC 外设关断 | 7 µA |
| Power off，CHIP_PU 为低 | 1 µA |

数据来源：[PICO-1 数据手册，表 3-8 / 3-9 及脚注](https://www.espressif.com/sites/default/files/documentation/esp32-s3-pico-1_datasheet_en.pdf)。`240 + 140 = 380 µA` 只是按官方脚注理解 N8R8 PSRAM 成本的算术参考，**不是当前 SDK 在 StickS3 上必能达到的浅睡电流**。应用保持 PSRAM 时不能直接照搬“VDD_SPI 断电”的全部测试条件；实际 GPIO、Flash、CPU 电源域与无线醒来占空比也会改变结果。

S3 系列数据手册还列出 ULP-FSM 保电约 **170 µA**、ULP RISC-V 保电约 **190 µA**；ULP / 传感器周期监测示例约 **18 µA**，其明确条件是触摸传感器 **1% 占空比**。18 µA 不是任意 ULP 程序、持续读 IMU、持续听声音都能达到的数字。[S3 v2.2，表 5-10 及脚注 2、3](https://documentation.espressif.com/esp32-s3_datasheet_en.pdf)

Active 无法用单个电流概括。PICO 官方 RF 测试在 3.3 V、25°C、发射 100% 占空比下，Wi-Fi TX 示例约 280–350 mA，RX 示例约 100–105 mA；它们是特定 RF 条件数值，不能充当本应用日常平均电流。[PICO-1，3.6.1 / 表 3-7](https://www.espressif.com/sites/default/files/documentation/esp32-s3-pico-1_datasheet_en.pdf)

### StickS3 整板电源层级

| 板级状态 | 哪些部分仍供电 | ESP 的状态与恢复 | 官方 4.2 V 输入参考 |
|---|---|---|---:|
| L3A，主控运行 | ESP 及相关供电；其他支路按配置开启 | 程序运行 | 36.69 mA |
| L2，主控睡眠 | ESP 仍有供电；外围按电源配置 | 可以使用芯片浅睡或深睡，恢复语义取决于所选模式 | 102.40 µA |
| L1，保留 IMU | PM1、IMU 与需要的唤醒电路 | ESP 断电，唤醒后重新上电初始化 | 52.47 µA |
| L0 / Power off | PM1 保电，其他支路关断 | ESP 断电，重新上电启动 | 14.02 µA |

层级定义依据 [M5PM1 多级电源说明](https://docs.m5stack.com/en/arduino/m5sticks3/m5pm1)，电流依据 [StickS3 官方规格](https://docs.m5stack.com/en/core/StickS3)。**各电源支路来自 L0，并非 L1→L2→L3 串联开关。** L3B 另控制背光/音频等外设，EXT_5V 和功放还有独立开关。

M5Stack 没有在该规格表提供完整的睡眠 API、GPIO、无线、IMU 配置、温度和采样条件。因此 L2 的 102.40 µA **不能当作任意 ESP Light-sleep 的整板实测**，也不能与 3.3 V 的芯片电流直接相减比较。L1 与 L0 的意义更直接：保留动作唤醒会多耗电，完整关机更省电，但失去拿起唤醒。

PM1 L1 的 IMU INT1 接到 **PM1 PYG4**，动作触发后由 PM1 恢复 ESP 供电；不是 ESP 自己的 RTC GPIO 在工作。当前关机后 PM1 仍耗微安级电流，因此“完全关机”是产品意义的主控/外设关断，并非电池物理断开。[官方 L1 唤醒例程](https://docs.m5stack.com/en/arduino/m5sticks3/m5pm1)

## 当前项目实际用了什么

当前本地 SDK 头文件确认 Arduino ESP32 **2.0.17**、IDF **4.4.7**。实际 `qio_opi/include/sdkconfig.h` 定义 `CONFIG_SPIRAM_MODE_OCT`，没有定义 `CONFIG_PM_ENABLE`、`CONFIG_FREERTOS_USE_TICKLESS_IDLE`，ULP 保留内存为 0。当前代码采用 160 / 240 MHz 切换及 BLE 空闲参数；这些都不等于已经运行自动浅睡或 ULP。

调研时工作区已经有后续 **v0.4.59** 的源码改动：BLE 初始化显式设置 `ESP_BT_SLEEP_MODE_1` 和主晶振睡眠时钟，并调用 `esp_bt_sleep_enable()`；失败则清理控制器、退回普通配置。因此，尽管 SDK 默认 `CONFIG_BT_CTRL_MODEM_SLEEP` 宏未定义，也不能据此认定运行时启用一定不支持。既有日志已出现 `[ble] modem sleep 1 main XTAL`，确认对应运行中的启用接口返回成功；它不是平均电流测量，也不代表启用了芯片浅睡。[v0.4.58 USB 日志](../artifacts/idle-stages-v0.4.58/after-usb.log)、[后续固件运行日志](../artifacts/connection-optimization-20261020/hardware/after-firmware-serial.log)。本文没有新增设备操作。

Wi-Fi 库在此芯片上的默认选项是 `WIFI_PS_MIN_MODEM`，本项目源码未覆盖 `setSleep` / `esp_wifi_set_ps`；这说明库默认允许 Wi-Fi Modem-sleep，不能代替运行时功耗测量。证据：本机 Arduino framework 的 `libraries/WiFi/src/WiFiGeneric.cpp:769`。

核对入口：[PlatformIO 配置](../firmware/platformio.ini)、[在线省电实现](../firmware/src/power_save.cpp)、[BLE 初始化与参数](../firmware/src/ble_link.cpp)。本机版本证据：`/Users/numbero/.platformio/packages/framework-arduinoespressif32/cores/esp32/esp_arduino_version.h`、`tools/sdk/esp32s3/include/esp_common/include/esp_idf_version.h`、`tools/sdk/esp32s3/qio_opi/include/sdkconfig.h`（后两条相对同一 framework 根目录）。

M5Unified 的 `M5.Power.timerSleep()` 也不要与 PM1 定时断电混淆：本项目固定驱动的定时睡眠最终走 ESP 深睡；实际 PM1 L1 使用本项目显式配置 IMU / HOLD / PM1 定时器后 `M5.Power.M5pm1.powerOff()` 的流程。[本项目深睡实现](../firmware/src/deep_sleep.cpp)、[M5Unified 固定版本](https://github.com/m5stack/M5Unified/blob/8b63555e7bd9ac71978c55ad868fd1d1d3b2ef3e/src/utility/Power_Class.inl)

开启“深睡待机”后，当前源码的策略为：

| 从最后一次真实活动起 | 当前行为 | 连接与恢复 |
|---|---|---|
| 5 分钟 | 在线息屏 | CPU 与连接继续工作；这是显示/应用策略，不是 Light-sleep |
| 10 分钟 | 满足纯电池与保护条件后进入 PM1 L1 | 主控断电并离线；动作或电源键使其重新启动、重连 |
| 20 分钟 | PM1 定时唤醒主控，启动早期解除 IMU 保电/运动唤醒，再进入 L0 | 普通电源键重新开机；期间若出现 USB / 用户输入等优先正常启动 |

依据：[空闲时间策略](../firmware/src/idle_power.h)、[PM1 计时与醒因检查](../firmware/src/deep_sleep_timer.h)、[深睡及到期关机](../firmware/src/deep_sleep.cpp)。10 分钟后的时钟由 **PM1** 维持；20 分钟处理含秒级取整及一次短暂启动，实际断电不应描述成毫秒精确。唤醒/真实活动后重新计时，USB 下不自动深睡或关机；关闭深睡开关仍为五分钟息屏、十分钟纯电池关机。[现有交付记录](idle-stages-v0.4.58.md)

现有记录验证过固件构建、回归、烧录读回与 USB 运行；**纯电池完整二十分钟、实际拿起唤醒、最终断电时刻及各阶段电流仍待实机验收**。本次没有新增测量，所有官方数字都保留其参考性质。

## 对后续方案讨论的含义

- 要在线接收 Mac 命令：优先讨论在线省电与无线参数；芯片 Deep-sleep 和 PM1 L1 都会离线。
- 要暂停主 CPU、快速继续原程序：可以研究 Light-sleep，但要同时解决当前 SDK / BLE、外设、PSRAM 和唤醒条件，不能只换一个 API。
- 要离线后拿起唤醒：现有 BMI270 + PM1 L1 是板上已有的硬件路径；主要代价是重新启动和重连。
- 要长时间最低静态耗电：L0 关机更合适。是否改变五/十/二十分钟门限属于产品规则，与芯片具备哪些睡眠模式是两个问题。

这些是根据官方能力及当前实现得出的方案判断，没有在本次调研中实施任何改动。
