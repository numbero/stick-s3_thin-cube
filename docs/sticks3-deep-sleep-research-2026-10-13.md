# StickS3 深睡与续航优化调研

日期：2026-10-13。对象：StickS3 K150、当前固件 v0.4.17。范围：官方资料和本地依赖源码静态核查；本次不操作设备、不进行放电或电流实测、不修改或烧录固件。

## 结论

设备具备低功耗能力，而且存在两条不同路径：ESP32-S3 自身的 Light-sleep / Deep-sleep；M5PM1 直接切断 ESP32-S3 电源，仅保留电源管理芯片，或额外保留 IMU。后者适合长时间离线待机，通常比保留 ESP 供电更省电。当前固件的在线息屏阶段仍有优化空间；进入已有的物理关机后，改用 ESP Deep-sleep 并不会进一步省电。[ESP-IDF 睡眠模式](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/system/sleep_modes.html)、[StickS3 电源分级说明](https://docs.m5stack.com/zh_CN/arduino/m5sticks3/m5pm1)

Deep-sleep 和断电待机期间无线连接不能持续接收 Mac 指令；唤醒后需要重新初始化并重连。因此优化需先确定是否接受离线待机，以及按电源键、移动或定时唤醒的交互。不能把无线持续在线、微安级整机功耗和随时接收远程命令同时视为已具备的能力。[ESP-IDF 无线与睡眠](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/system/sleep_modes.html#wifi-bt-and-sleep-modes)

## 模式与用户体验

| 模式 | 运行与状态 | 可用的唤醒路径 | 适合用途 |
| --- | --- | --- | --- |
| 息屏但 MCU 运行 | 只关闭背光、停绘，CPU 和无线仍运行 | 应用收到按键、运动或无线命令 | 保持在线，但不等于硬件睡眠 |
| ESP Light-sleep | CPU 和多数外设时钟暂停；恢复后保留运行状态 | 配置后的定时器、GPIO 等 | 短时暂停；直接手动进入会断无线连接 |
| ESP Deep-sleep，保留 L2 供电 | CPU、普通 RAM 和大部分数字外设关闭；RTC 等按需保留 | 配置后的 RTC 定时器、RTC GPIO 等 | 较长离线待机；醒来重新启动应用 |
| M5PM1 L1 | ESP 断电，M5PM1 和 IMU 保留供电 | 电源键、PM1 定时器、正确配置的 IMU 中断 | 拿起唤醒、离线待机；醒来重新上电 |
| M5PM1 L0 | ESP 与外设断电，仅 M5PM1 留电 | 电源键、PM1 定时器、外部电源插入等 | 最低功耗的关机/长期待机 |

ESP 模式依据 [IDF 4.4.7](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/system/sleep_modes.html)；M5PM1 模式依据 [官方分级与案例](https://docs.m5stack.com/zh_CN/arduino/m5sticks3/m5pm1)。L1、L2、L3 的供电来自 L0，不能按串联电源理解。外设实际关闭情况取决于各级电源及 HOLD 配置，调用 ESP 深睡不会自动关闭所有板载外设。

## M5PM1 唤醒的实际条件

- **电源键**：机身电源/复位键接 PM1，默认单击开机/复位、双击关机、长按下载。它与 ESP GPIO11/12 的 A/B 按键不同；ESP 断电后，不能把 A/B 的普通应用按键处理视为 PM1 唤醒来源。[产品按键与管脚](https://docs.m5stack.com/zh_CN/core/StickS3)
- **定时**：官方例程使用 `pm1.timerSet(10, M5PM1_TIM_ACTION_POWERON)` 再 `pm1.shutdown()`，由 PM1 在断电后计时并重新开机；不能用 ESP RTC 定时器唤醒已经断电的 ESP。[官方 Timer 例程](https://docs.m5stack.com/zh_CN/arduino/m5sticks3/m5pm1)
- **IMU**：官方例程先初始化 BMI270、启用 `BMI2_ANY_MOTION`、设置 INT1 输出电平与引脚映射，再设置 PM1 PYG4 输入唤醒和下降沿，并保持 L1。BMI270 INT1 → PM1 PYG4 的电路已经具备；“保持 IMU 电源”这一行本身不足以实现运动唤醒。阈值、持续时间、轴、极性及事件清理需要正确配置，不能承诺任意轻微移动均可立即唤醒。[官方 IMU 唤醒例程](https://docs.m5stack.com/zh_CN/arduino/m5sticks3/m5pm1)
- **ESP GPIO13 链式唤醒**：另一条官方路径是 BMI270 INT1 → PM1 PYG4 → PM1 PYG1_IRQ → ESP GPIO13；它适用于 ESP 仍保留电源的睡眠。需要配置 IRQ 掩码、清除旧标志并设置 ESP 唤醒源，不能与 PM1 断电唤醒混为一谈。[官方 IMU ESP32-S3 唤醒例程](https://docs.m5stack.com/zh_CN/arduino/m5sticks3/m5pm1)

PM1 GPIO 唤醒默认关闭；GPIO0/2、GPIO3/4 两组内分别互斥，GPIO1 不支持 PM1 自身 WAKE。`WAKE_SRC` 用于读出/清除原因，并不是写入后直接指定唤醒源。[M5PM1 手册 V1.9，GPIO_WAKE_EN / WAKE_SRC](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/M5PM1_Datasheet_CN.pdf)

### I2C 空闲休眠不是整机深睡

`I2C_CFG.SLP_TO` 是 PM1 控制芯片自身的 I2C 空闲休眠配置；不能据此认为 ESP 已睡眠或 L2 已切断。第一次 I2C 访问用于唤醒，会失败，随后需要在指定时窗完成通信；PM1 固定每 100 ms 唤醒检查。PWM 使能时该空闲休眠失效。当前 M5Unified 的 PM1 `begin()` 明确把此配置写为 0，所以并未自动启用。[M5PM1 手册，第 14 / 26 / 27 页](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/M5PM1_Datasheet_CN.pdf)、[固定版本驱动 begin()](https://github.com/m5stack/M5Unified/blob/8b63555e7bd9ac71978c55ad868fd1d1d3b2ef3e/src/utility/power/M5PM1_Class.inl#L69)

### USB 与关机

官方手册的关机流程关闭 3.3V DCDC 与 LED_EN，充电使能不受影响，没有列出“USB 在场则不能关机”的条件。原理图中 USB 和电池合流到 VBUS_L0，然后经受 PM1 控制的 JW5712 供电给 L2；没有看到 USB 绕过此开关直接给 ESP 3.3V 供电的路径。**这是支持 USB 供电时也可切断 L2 的硬件依据，尚未实测本机行为。** USB 插入是 PM1 的唤醒事件，和“USB 一直插着必然阻止关机”是不同命题。[M5PM1 手册，关机流程 / 唤醒来源](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/M5PM1_Datasheet_CN.pdf)、[StickS3 原理图，第 2 张](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150_Stick_S3_PRJ_V0.6_20251111_2025_11_17_16_10_24.pdf)

## 当前固定 M5Unified 的 API 路径

本机依赖确认为提交 `8b63555e7bd9ac71978c55ad868fd1d1d3b2ef3e`，按该版本核查，避免从其他板型或最新分支推断。

| API | 此版本 StickS3 / ESP32-S3 的真实路径 | 影响 |
| --- | --- | --- |
| `M5.Power.timerSleep(seconds)` | 设 ESP RTC timer；显示睡眠；进入 `_powerOff(true)`；S3 的 PM1 分支因 `withTimer=true` 跳过 PM1 关机；最后 `esp_deep_sleep_start()` | ESP Deep-sleep，保留 L2；不是 PM1 timer + 断电 |
| `M5.Power.powerOff()` | 显示睡眠；`_powerOff(false)`；调用 `M5pm1.powerOff()`；若 MCU 仍运行则继续 ESP 深睡路径 | 先尝试 PM1 断电；不能把后备路径当成断电已成功的证据 |
| `M5.Power.M5pm1.powerOff()` | 直接写 `SYS_CMD=0xA1`，返回通信成功与否 | 不执行 Power_Class 的 ESP 深睡后备；当前项目使用这条路径 |
| `M5.Power.deepSleep(...)` / `lightSleep(...)` | 独立的 ESP 睡眠包装 | StickS3 初始化分支没有配置 `_wakeupPin` 为 GPIO13；不会自动配置 BMI270 运动唤醒 |

证据：[Power_Class 初始化 / 关机 / timerSleep](https://github.com/m5stack/M5Unified/blob/8b63555e7bd9ac71978c55ad868fd1d1d3b2ef3e/src/utility/Power_Class.inl#L434)、[PM1 直接关机](https://github.com/m5stack/M5Unified/blob/8b63555e7bd9ac71978c55ad868fd1d1d3b2ef3e/src/utility/power/M5PM1_Class.inl#L350)。源码在本地同提交依赖中逐行读取；网络抓取器未能展开该提交的 GitHub 页面。

## 官方功耗数字应怎样使用

| 官方状态 | 官方整机电流，供电条件 4.2 V |
| --- | --- |
| 关机 | 14.02 µA |
| L1 | 52.47 µA |
| L2 | 102.40 µA |
| L3A | 36.69 mA |
| 满载 | 519.02 mA |

这些数字来自 [StickS3 官方规格](https://docs.m5stack.com/zh_CN/core/StickS3)。L2 对应 ESP 睡眠，L3A 对应 ESP 工作；它们不能替代当前 ThinCube 固件各阶段的实测电流。规格没有给出完整的无线连接、外设配置、亮度、温度和测量时间条件，也不能把 L2 数字直接当作所有 Light-sleep 配置的功耗。满载电流不能作为日常静置电流。保留 IMU 的 L1 比 L0 多耗电，却能改善拿起唤醒体验。

250 mAh / 某一个官方电流只能得到理想静态除法，忽略实际可用容量、自放电、转换损耗、唤醒/联网耗电与工作占比；本次不据此承诺具体小时数或天数。续航收益应按 `平均电流 = Σ(各状态电流 × 时间占比) + 唤醒开销` 评估。

## 当前固件静态审计

已审计提交 `dcd46538f2c4c2e6cb9395ea9eb85279d48b44ca` 的 v0.4.17，未通过串口重新采样或实测。

| 当前阶段/实现 | 静态证据 | 意义 |
| --- | --- | --- |
| 静置约30秒视觉睡眠 | `ui_timing.h:10`、`ui.cpp:446–451,615`：PET_SLEEP，背光12 | 仍绘制睡觉画面；不是 MCU 睡眠 |
| 5分钟在线息屏 | `idle_power.h:6`、`ui.cpp:691–724`：亮度0、停止绘制 | 无线、按键和 IMU 仍处理；`main.cpp:177` 只改为 `delay(5)` |
| 10分钟纯电池关机 | `ui.cpp:697–712`：复查供电及 OTA 互斥后直接 `M5.Power.M5pm1.powerOff()` | 已采用 PM1 断电路线；USB不自动关机是本项目策略 |
| 外部5V/音频电路 | `main.cpp:77` 的 `cfg.output_power=false`；`audio_input.cpp:53–61` 关闭 ES8311；睡眠后停 Mic/Speaker | 已有省电措施，不能把再次关闭这些外设算作新收益 |
| 主循环与 IMU | `main.cpp:127–145`、`ui.cpp:495`、`motion.cpp:51–59`：每轮更新 IMU/无线；没有进入 ESP 睡眠 API | 息屏后仍有周期唤醒、传感器读取和状态处理 |
| BLE连接间隔 | `ble_link.cpp:88` 固定请求 min=12/max=18，单位1.25ms，latency=0 | 请求15–22.5ms，未区分待机/传音；实际协商结果由Mac决定并在回调记录 |
| 状态构建/发送 | `device.cpp:502,510,528`：完整JSON约每40ms重建；BLE快速状态200ms；空闲完整快照5s | 可优先减少没有变化的完整JSON构建；通知和控制租约有独立时限 |

对应源码：[main](../firmware/src/main.cpp)、[idle policy](../firmware/src/idle_power.h)、[UI](../firmware/src/ui.cpp)、[BLE](../firmware/src/ble_link.cpp)、[device](../firmware/src/device.cpp)、[audio](../firmware/src/audio_input.cpp)。当前任务没有开启 MCU deep/light sleep，也没有初始化 BMI270 的低功耗运动中断唤醒路径。

**两端通信时限必须一起处理。** Mac 的 `DeviceModel.swift:688–701` 在400ms没有状态更新时释放快捷键/清音频缓冲，2秒判失联；BLE心跳每200ms发送。固件 `VoiceState::tick()` 的语音租约为1秒。因此不能只把状态或心跳降到1Hz，就声称保留现有交互；要保留正在传音/按键期间的快速通道，或明确协商待机状态、同步调整两端契约，再在新指令前恢复。[Mac代码](../macos/Sources/ThinCube/DeviceModel.swift)、[语音状态](../firmware/src/voice_state.h)

本机实际使用的 Arduino SDK 配置 `tools/sdk/esp32s3/qio_opi/include/sdkconfig.h` 定义 CPU默认240MHz、`CONFIG_BT_CTRL_SLEEP_MODE_EFF=0`，没有定义 `CONFIG_PM_ENABLE` 或 `CONFIG_FREERTOS_USE_TICKLESS_IDLE`。其共享 `esp_bt.h` 中0表示 `ESP_BT_SLEEP_MODE_NONE`，`BT_CONTROLLER_INIT_CONFIG_DEFAULT()`直接使用该值；项目初始化未覆盖它。当前控制器没有启用这个省电模式，自动 Light-sleep 也未启用。BLE控制器结构允许配置 sleep_mode，能否用现有二进制库和时钟设置可靠启用仍需后续验证；自动 Light-sleep 需要相应SDK构建配置支持，不能只在应用添加宏而认为预编译库已改变。[IDF睡眠与无线](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/system/sleep_modes.html#wifi-bt-and-sleep-modes)、[IDF电源管理](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/system/power_management.html)

## 优化优先级与待确认项

1. 默认先保留既有5分钟息屏/10分钟关机规则，优化在线待机：减少无变化的完整状态JSON构建、降低闲置绘制/IMU轮询、评估CPU空闲时手动降至80或160MHz、待机BLE放宽连接间隔并配置控制器省电，活动/音频时恢复。CPU频率切换与自动DFS/Light-sleep是不同路径；Arduino已有 `setCpuFrequencyMhz()`，但音频时钟、任务吞吐和切换行为仍需验证。连接参数需检查Mac实际接受值，状态/租约时限需按上节保护。这些是工程方案，具体节省幅度尚未实测。提前息屏可作为另一个用户设置，不把改动现有时限作为硬件深睡的必要条件。
2. 接受断开连接时，在较短空闲后进入 PM1 L0；已有的物理关机是这条路线，调整进入时机比用 ESP 深睡替换物理关机更有意义。
3. 若需要拿起即用，增加 PM1 L1 + BMI270 中断唤醒；换来的是唤醒便利，而非比 L0 更低的关机电流。需处理运动误唤醒及重新连接延时。
4. 保持无线连接的自动DFS/Light-sleep应单列为SDK与驱动改造：自动 Light-sleep 需要编译开启 `CONFIG_PM_ENABLE` 和 Tickless Idle，运行时配置 `esp_pm_configure()`；驱动持有的PM锁也会影响能否入睡。它比手动调整应用轮询复杂，不能只加一次 `esp_light_sleep_start()` 就声称保持BLE在线且省到L2。[IDF 电源管理](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/system/power_management.html)

推荐实现顺序：先做可恢复的在线省电；若希望长期放置也能拿起唤醒，再增加可选的 PM1 L1 模式。睡前先停止音频、释放快捷键、通知Mac预期离线，并完成配置保存及OTA/配对互斥；醒来走正常初始化和已有认证重连，重置旧会话及按键状态。深睡只能减少闲置占比的耗电，不能据此推导持续亮屏或传音续航同幅提升。

未来实施需确认本机 PM1 版本和唤醒配置、BMI270 所用驱动对低功耗 feature 的支持、长时间静置与实际拿起的误触发边界、断电前保存配置及互斥规则、Mac 重连与首个指令的行为。原有低压保护、配对、音频和 OTA 不应在新增睡眠流程中丢失。本次仅提出方案，未产生新固件，也未进行实机验证。

## 实现接口核查：复用现有 BMI270 配置

本节是用户授权实施后的接口研究，仍只读取资料和源码，不操作设备。本节没有测量运动阈值、电流或续航。

### 已确认 feature blob 与 offset

M5Unified 固定提交中的 `BMI270_Class::begin()` 软件复位 BMI270、上传 `BMI270_config.inl` 的 `bmi270_config_file`，随后打开正常的加速度、陀螺仪和温度数据采集。该 blob 已与 Bosch 官方 SensorAPI 提交 `41129fcfe39c583ee5462d79195741945d51c1fe` 的 **base** `bmi270.c` 配置数组逐字节比对：均为 **8192 bytes**，SHA-256 均为 `2d75e68e343a13ff99be98261dfbd99d9e8c6f267da9e3c5aeb883276ad178db`。因此能够使用 base ANY_MOTION 的页与位定义，不需要另一份 blob 或 SparkFun/Bosch 全量依赖。[现有加载器](https://github.com/m5stack/M5Unified/blob/8b63555e7bd9ac71978c55ad868fd1d1d3b2ef3e/src/utility/imu/BMI270_Class.inl)、[现有 blob](https://github.com/m5stack/M5Unified/blob/8b63555e7bd9ac71978c55ad868fd1d1d3b2ef3e/src/utility/imu/BMI270_config.inl)、[Bosch 固定版本 base 配置与 feature 表](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270.c)

现有 `BMI270_Class` 没有公开 ANY_MOTION 或低功耗配置 API，但继承了公开的 `I2C_Device::readRegister(reg, buf, len)` 和 `writeRegister(...)`；`M5.Imu.getImuInstancePtr(0)` 可取得对象及 `getAddress()`。应先验证 `getType()==imu_bmi270`、芯片 ID `0x24`、初始化状态，再使用现有 I2C 对象做小范围桥接。BMI270 地址从对象读取，不把其他板型的默认 `0x69` 写死。PM1 同样继承这些接口；其固定驱动没有 GPIO_WAKE / HOLD 的专用 API，可做带错误返回与读回校验的寄存器操作。[M5Unified IMU 接口](https://github.com/m5stack/M5Unified/blob/8b63555e7bd9ac71978c55ad868fd1d1d3b2ef3e/src/utility/IMU_Class.hpp)、[I2C_Device 接口](https://github.com/m5stack/M5Unified/blob/8b63555e7bd9ac71978c55ad868fd1d1d3b2ef3e/src/utility/I2C_Class.hpp)

### BMI270 参数与访问规则

| 寄存器/feature | 已核实定义与建议起点 |
| --- | --- |
| `FEAT_PAGE` `0x2F` | 低3位选择页；ANY_MOTION 在 **page 1，offset 0x0C**，即寄存器 `0x3C..0x3F` |
| ANYMO_1，`0x3C/0x3D` | LE 16-bit；duration bits12:0，每单位20ms；xyz选择 bits13/14/15。官方默认 `0xE005`：三轴、100ms |
| ANYMO_2，`0x3E/0x3F` | threshold bits10:0；out_conf bits14:11；enable bit15。官方默认 threshold `0xAA` 约83mg、out_conf=7（状态/映射bit6）。启用后的值为 `0xB8AA`，不是仅向某字节写 `0x80` |
| `INT1_IO_CTRL` `0x53` | 有效字段 mask `0x1E`，值 `0x08`：低有效、推挽、输出开、输入关 |
| `INT_LATCH` `0x55` | bit0。M5Stack官方例程为0（不锁存）；1为永久锁存。若选择1，应明确这是工程策略，先读 `INT_STATUS_0` 清旧事件并确认引脚回到HIGH |
| `INT1_MAP_FEAT` `0x56` | bit6=`0x40` 映射 ANY_MOTION；睡眠专用状态可只保留该feature |
| `INT2_MAP_FEAT` `0x57` / `INT_MAP_DATA` `0x58` | 睡眠阶段设0，避免其他feature、数据就绪或FIFO事件参与唤醒；正常值必须保存并恢复 |
| `ACC_CONF` `0x40` | Bosch低功耗示例 `0x17`：50Hz、avg2、filter_perf=0；低功耗feature要求ODR至少50Hz，不使用12.5Hz作为ANY_MOTION起点 |
| `PWR_CTRL` `0x7D` | mask低4位设 `0x04`：仅加速度开，陀螺仪/AUX/温度关；ACC_RANGE保留原值 |
| `PWR_CONF` `0x7C` | 全部feature与映射配置完成后，最后开启APS bit0；保留其他位。Bosch完整低功耗示例值为 `0x03`，并不表示必须改动本项目原有FIFO设置 |
| `INTERNAL_STATUS` `0x21` | `value&0x0F` 必须为1，`value&0x60` 必须为0（轴映射、50Hz带宽错误） |
| `INT_STATUS_0/1` `0x1C/0x1D` | 读取清除状态；锁存模式下也用于释放INT引脚。条件仍成立时可能再次断言，不能读一次就假定已经静止 |

定义可交叉核查 [Bosch bmi270 feature 表及配置函数](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270.c)、[位 mask](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi2_defs.h)、[BMI270 手册 Rev1.6，第20/41/43/54/92–93页](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmi270-ds000.pdf)。这些是官方默认/示例及合法配置起点，不能称为已经标定的拾取阈值。

访问约束：先关闭APS、等待至少450µs，再访问feature页；FEATURE写必须从偶地址开始、以完整16-bit word结束，不能向0x3F单独写enable位。可一次写 `0x3C` 开始的4 bytes，保存并恢复原FEAT_PAGE。每次写后尊重时序；桥接统一使用至少1ms延迟较保守。APS开启时，Bosch API按单字节写并每字节等待450µs，不能照普通模式随意burst写；恢复时也先关闭APS、等待再恢复其他寄存器。[Bosch读写与APS函数](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi2.c)

### PM1 配置与读回

| 寄存器 | 应操作的 mask / 目标 |
| --- | --- |
| `GPIO_WAKE_EN` `0x18` | mask `0x18`：先关闭GPIO3/4唤醒；最终只有bit4=`0x10`，GPIO3与4互斥 |
| `GPIO_WAKE_CFG` `0x19` | bit4=`0`，下降沿唤醒；其他位保留 |
| `GPIO_MODE` `0x10` | bit4=`0`，GPIO4输入 |
| `GPIO_FUNC1` `0x17` | bits1:0=`00`，GPIO4普通GPIO |
| `GPIO_PUPD1` `0x15` | bits1:0=`01`，GPIO4上拉；若要求关机后保留该设置，另外启用HOLD bit4 |
| `GPIO_IN` `0x12` | 用含错误返回的整字节读取确认bit4=`1`，不要使用读失败也返回false的单pin getter判定 |
| `PWR_CFG` `0x06` | 保留充电/DCDC等位，确认LDO bit2=`1`；官方例程另设LED_EN bit4=`1` |
| `HOLD_CFG` `0x07` | LDO保持bit5=`1`；GPIO4保持bit4仅用于保留其输入/pull配置。不要保持LCD/音频GPIO2或5V boost |
| `WAKE_SRC` `0x05` | 先记录后清除；官方驱动采用写0清除，不应写1清除，也不应按手册正文的错误交叉引用误写0x2F |
| IRQ Status `0x40..0x42` | 清醒因后按需清旧IRQ；写0清除并读回，不把IRQ掩码等同 GPIO_WAKE_EN |
| `SYS_CMD` `0x0C` | 所有配置和末次准入校验成功后才发送 `0xA1`；它是PM1关机命令，不调用ESP睡眠作为失败补救 |

来源：[PM1手册寄存器表](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/M5PM1_Datasheet_CN.pdf)、[官方GPIO wake/hold/清醒因驱动](https://github.com/m5stack/M5PM1/blob/main/src/M5PM1.cpp)、[M5Stack IMU断电唤醒例程](https://docs.m5stack.com/zh_CN/arduino/m5sticks3/m5pm1)。

所有 RMW 都应使用返回bool的 `readRegister()`，成功后按mask修改、写入、再读取验证目标mask；`readRegister8()`无独立错误返回，读失败得到0时会误清其他位。对只读/动态状态不要求全字节相等；对配置则验证有效字段。禁止在PM1电源、I2C读写失败时继续发关机命令。

### 启动、失败和普通关机的清理顺序

建议把睡眠准备做成可回滚事务：准备前保存上述将改变的普通IMU寄存器、ANY_MOTION四字节、FEAT_PAGE与PM1配置；先停运动轮询，避免另一条I2C读取消耗状态。先禁止PM1 GPIO4 wake，再配置BMI270和LDO保持，最后才允许GPIO4 wake。关机前至少确认没有旧运动状态、INT1为HIGH、BMI内部状态正常、纯电池供电及应用准入条件仍满足；任何新活动、来源变化、读失败或读回不符都回滚并继续在线。连续HIGH时窗是工程防误入睡措施，不能消除末次检查与关机之间所有硬件竞争。

失败回滚时，先关闭PM1 GPIO4 wake并取消本流程的LDO/GPIO4 HOLD，再关闭BMI APS并等待、取消ANY_MOTION启用/映射、清旧中断、恢复保存的普通寄存器及FEAT_PAGE，最后恢复原PWR_CONF；逐项检查，不能因第一项失败就短路跳过后续清理。正常IMU数据应恢复陀螺仪和原有 `INT_MAP_DATA`，否则 `M5.Imu.update()` 依赖的数据就绪状态和后续运动识别可能异常。若总线持续故障而无法完整恢复，应记录硬件错误、禁用此次睡眠及重试，MCU保持在线，不宣称外设已恢复。

每次重新上电后，即使只读到了ESP普通复位原因，也应读PM1 `WAKE_SRC`，先禁用GPIO4 wake、取消本流程的HOLD，再清醒因/IRQ。M5Unified本身不会清 `GPIO_WAKE_EN`，不能依赖“重启会自动解除”。`M5.begin()` 的现有BMI270初始化会重新加载同一个blob并恢复常规模式；之后仍须检查状态和关闭本流程的wake/hold残留。这一清理能防止**醒来之后**再次手动关机继续运动唤醒。

边界：L1模式已经切断ESP供电时，MCU没有运行，无法拦截机身电源键双击并清掉PM1唤醒配置。官方关机流程遵守HOLD，不能承诺在L1睡着时双击就会切回完整L0。需要完全关机时，产品应允许唤醒后显式执行普通关机流程，或保持原有L0作为默认、L1作为明确的可选模式。

未来修改M5Unified或更换BMI270 base/context/legacy/max-fifo配置时，必须重新核对blob与feature表；页和offset不能跨配置沿用。本次研究仅确认现有固定版本适合最小寄存器桥接，尚未验证本机运动唤醒、电源切断和重连表现。
