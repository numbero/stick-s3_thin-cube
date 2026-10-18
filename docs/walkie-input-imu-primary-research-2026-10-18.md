# 对讲机输入扩展：StickS3 / BMI270 一手证据笔记

日期：2026-10-18。范围：M5Stack、Bosch 官方资料及项目固定依赖的静态核查；没有修改产品代码、操作设备、烧录固件或测量识别率。本笔记供对讲机快捷键方案决策使用。

## 可以直接用于决策的结论

- **姿态、倾斜、翻面和角速度都能通过现有六轴原始数据实现软件识别。** 当前 M5Unified 提供加速度与陀螺仪读数；没有现成的 pitch/roll/yaw 融合接口。稳定姿态可利用重力方向，绕重力轴的短时旋转可利用陀螺仪；持续绝对航向不能从这套六轴硬件直接保证。[固定 IMU 接口](https://github.com/m5stack/M5Unified/blob/8b63555e7bd9ac71978c55ad868fd1d1d3b2ef3e/src/utility/IMU_Class.hpp)、[StickS3 规格](https://docs.m5stack.com/zh_CN/core/StickS3)
- **“BMI270 支持敲击”与“当前固件能直接开启敲击”是两回事。** 本项目载入的是 Bosch **base** 配置；single/double/triple tap、orientation、flat 属于另一套 **Legacy** 配置。不能照抄 Legacy 页地址向 base 写寄存器。[Bosch 配置功能清单](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/README.md)、[base feature 表](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270.c#L487)、[Legacy feature 表](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270_legacy.c#L495)
- **首选继续用现有原始数据做软件手势；双击敲壳应作为单独实验。** 这是工程建议：它避免为一个输入改换 IMU 配置，并保留已有深睡唤醒路径。敲击、晃动的误触率与佩戴/摆放方式有关，官方资料不证明它们在 ThinCube 机壳上的可用性。

## 硬件与固定依赖

StickS3 K150 官方列出 BMI270，内部 I²C 为 SDA=GPIO47、SCL=GPIO48，地址 0x68。BMI270 是三轴加速度计加三轴陀螺仪；本机产品规格未列出磁力计。[M5Stack 产品规格与管脚](https://docs.m5stack.com/zh_CN/core/StickS3)

本地依赖 `firmware/.pio/libdeps/m5stack-sticks3/M5Unified` 为版本 0.2.23，Git 提交 `8b63555e7bd9ac71978c55ad868fd1d1d3b2ef3e`，与 [platformio.ini](../firmware/platformio.ini) 固定版本一致。依赖工作区无修改。本次读取的 Bosch `master` 文件又与提交 `41129fcfe39c583ee5462d79195741945d51c1fe` 核对：`bmi270.c`、`bmi270_legacy.c`、`bmi2_defs.h` 内容完全一致，后文使用固定提交链接。

M5Unified 的 `BMI270_config.inl` 与 Bosch `bmi270.c` 中 `bmi270_config_file` **8192 个字节完全一致**，SHA-256 均为 `2d75e68e343a13ff99be98261dfbd99d9e8c6f267da9e3c5aeb883276ad178db`。这是本地数组提取与逐字节比较的结果，并非按文件名推断。[M5Unified blob](https://github.com/m5stack/M5Unified/blob/8b63555e7bd9ac71978c55ad868fd1d1d3b2ef3e/src/utility/imu/BMI270_config.inl)、[Bosch base blob](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270.c#L50)

## base 与 Legacy 的真实边界

| 能力 | 当前 base 配置 | Bosch Legacy 配置 | 用于快捷键时的解释 |
| --- | --- | --- | --- |
| 原始加速度、角速度 | 有 | 有 | 可自行实现倾斜、翻面、旋转、晃动检测 |
| Any-motion / No-motion | 有 | 有 | 只证明运动/静止条件满足，不等同特定手势 |
| Significant motion | 有 | 有 | 面向位置变化活动；不应充当低延迟摇一摇快捷键 |
| Wrist gesture | 有 | 官方 Legacy 清单未列出 | 包括腕部 jiggle、flick 等；不是通用手持设备手势分类器 |
| Orientation / Flat | 官方 base 清单未列出 | 有 | base 可用原始数据软件实现等价的产品交互 |
| 单击、双击、三击敲壳 | 官方 base 清单未列出 | 有 | 使用片上检测须切换配置与驱动适配 |

功能归属依据 [Bosch README](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/README.md)；可启用的 feature ID 可交叉查 [base 头文件](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270.h)、[Legacy 头文件](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270_legacy.h#L172)。表中“未列出”不表示原始传感器做不到，而是不能把另一配置的片上算法接口直接当成当前能力。

base 的腕部识别假设佩戴坐标系与左右手，并依赖方向映射；官方示例启用 `BMI2_ACCEL` 与 `BMI2_WRIST_GESTURE`，不要求开启 gyro。**工程推论：手持、挂绳、磁吸桌面的 ThinCube 不能仅凭 feature 名称就承诺识别率。** [Bosch wrist 示例](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270_examples/wrist_gesture/wrist_gesture.c)、[数据手册 §4.8.7](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmi270-ds000.pdf#page=50)

## M5Unified 目前交付了什么

| 接口/行为 | 固定版本核查结果 | 实施含义 |
| --- | --- | --- |
| `M5.Imu.update()` | 返回本次更新的 sensor mask | 软件检测应检查 accel/gyro 新样本位；非零返回不保证两者同时更新 |
| `getImuData()` | 读缓存并做换算、校准和轴顺序变换 | 同一缓存不能当作无限多个独立样本 |
| `getAccel()` / `getGyro()` | 可读浮点加速度 / 角速度 | 支持软件姿态与短时旋转判断 |
| `getAhrsData()` | 仅有“计划实现”的注释 | pitch/roll/yaw 需自行计算或接入融合算法 |
| `BMI270_Class` | 载入 base blob；读取 `INT_STATUS_1` 和最新数据寄存器 | 没有 tap/orientation/flat 的上层接口，也未实现 FIFO 消费 |
| `getImuInstancePtr(0)` | 可取底层实例，继承读写寄存器接口 | 可做受控扩展；不要再初始化第二个驱动抢同一个芯片 |

证据：[IMU 头文件](https://github.com/m5stack/M5Unified/blob/8b63555e7bd9ac71978c55ad868fd1d1d3b2ef3e/src/utility/IMU_Class.hpp)、[update / getImuData 实现](https://github.com/m5stack/M5Unified/blob/8b63555e7bd9ac71978c55ad868fd1d1d3b2ef3e/src/utility/IMU_Class.inl#L425)、[BMI270 实现](https://github.com/m5stack/M5Unified/blob/8b63555e7bd9ac71978c55ad868fd1d1d3b2ef3e/src/utility/imu/BMI270_Class.inl)。以上固定源码在本地逐行读取；网络页面展开失败不影响本地源码核查。

StickS3 的 M5Unified 逻辑坐标为 `X=传感器+Y，Y=传感器−X，Z=传感器+Z`。这层软件轴变换并不会自动配置 Bosch 片上 feature 自己的轴映射；两种算法接入时必须分清坐标。[固定板型轴处理](https://github.com/m5stack/M5Unified/blob/8b63555e7bd9ac71978c55ad868fd1d1d3b2ef3e/src/utility/IMU_Class.inl#L120)

## ODR、量程与 FIFO

驱动初始化没有显式写 ACC_CONF、ACC_RANGE、GYR_CONF、GYR_RANGE；按芯片复位值，正常预期为 **加速度 100 Hz / ±8g，陀螺仪 200 Hz / ±2000°/s**。FIFO 的 acc/gyro 存入位复位为关闭。**这是源码与手册推断，尚未回读本机运行寄存器。** [初始化实现](https://github.com/m5stack/M5Unified/blob/8b63555e7bd9ac71978c55ad868fd1d1d3b2ef3e/src/utility/imu/BMI270_Class.inl#L38)、[BMI270 手册 §5.2.41–44、50](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmi270-ds000.pdf#page=100)

M5Unified 当前换算参数固定为 `8/32768 g/LSB` 和 `2000/32768 °/s/LSB`，BMI270 类只改写温度及磁场换算。**如果改量程，必须同步换算，不能仅写寄存器。** [IMU_Base 参数](https://github.com/m5stack/M5Unified/blob/8b63555e7bd9ac71978c55ad868fd1d1d3b2ef3e/src/utility/imu/IMU_Base.hpp#L55)、[BMI270 换算实现](https://github.com/m5stack/M5Unified/blob/8b63555e7bd9ac71978c55ad868fd1d1d3b2ef3e/src/utility/imu/BMI270_Class.inl#L161)

当前 [motion.cpp](../firmware/src/motion.cpp) 在省电时每 40 ms 才轮询；读取的是最新值，没有补回中间样本。**工程推论：100 Hz 芯片 ODR 不等于应用有完整 100 Hz 数据；25 Hz 主机轮询更不能保证捕获敲击短脉冲。** 低速稳定倾斜更适合现有路径。双敲实验应选择稳定采样和环形缓冲，或完整接入 FIFO / Legacy 片上检测；记录样本实际间隔、丢样与饱和率后再决定。

普通 gyro 数据寄存器 ODR 在 Bosch API 为 25–3200 Hz；芯片宣传的 6.4 kHz 涉及专用 OIS 路径，不能当作当前 I²C + M5Unified 已有吞吐。[API ODR 定义](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi2_defs.h#L1131)、[手册 GYR_CONF / OIS](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmi270-ds000.pdf#page=102)

## 双敲片上方案的限制

Bosch Legacy tap 支持选一个轴，分别启用单/双/三击；`data_reg_en=1` 时必须使用 **200 Hz** 用户加速度数据，关闭时取内部 200 Hz 未滤波数据。低功耗 filter 模式要求 tap/high-G 至少 200 Hz，其余 feature 至少 50 Hz。[Legacy AN002，第 16、36 页](https://www.bosch-sensortec.com/media/boschsensortec/downloads/application_notes_1/bst-bmi270-an002-01.pdf#page=36)

`wait_for_timeout=0` 可在一个手势窗口内产生多次报告；同时映射单/双/三击时要显式设计仲裁。若等待窗口结束再分类，可靠性与延迟会互相牵制。AN002 正文和寄存器表对默认灵敏度/窗口描述不一致，应用应显式配置并回读；不依赖抄来的默认值。[Legacy AN002，第 36、70–71 页](https://www.bosch-sensortec.com/media/boschsensortec/downloads/application_notes_1/bst-bmi270-an002-01.pdf#page=70)

官方 tap 示例先 `bmi270_legacy_init()`，启用 accel 与各 tap feature，映射 tap 中断，然后读取状态及 tap 类型。它证明硬件与官方 API 支持这条路径，没有证明当前 M5Unified 可以直接复用。[Bosch 官方 tap 示例](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270_legacy_examples/tap/tap.c)

## 中断、深睡与已有功能的冲突

BMI270 INT1 接到 **M5PM1 PYG4**；可经 PM1 IRQ 链到 ESP GPIO13。在线软件轮询不必新增硬件中断；若使用硬件 IRQ，需要配置并清除 PM1 中间状态，不能把 INT1 当成直连 ESP 的普通引脚。[M5Stack 官方 L1 / IMU ESP32-S3 Wake-up](https://docs.m5stack.com/zh_CN/arduino/m5sticks3/m5pm1)

项目已经通过 [motion_wake_registers.h](../firmware/src/motion_wake_registers.h) 实现 base ANY_MOTION 断电唤醒，操作 page1 `0x3C..0x3F`、加速度/电源、中断映射和 PM1 GPIO。Legacy 的 ANY_MOTION 起点为 **page1 offset0x06（寄存器0x36）**，与 base offset0x0C 不同；换 Legacy 必须重做配置与恢复核对。[Legacy 地址定义](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270_legacy.h#L64)、[既有深睡证据](sticks3-deep-sleep-research-2026-10-13.md)

新增 feature 状态读取应由一个地方统一消费。现有驱动读取 data-ready 状态，Bosch feature 示例会读取/清除中断源；若两个驱动各自初始化、修改量程/电源或清事件，会影响采样、姿态和睡眠。此处为源码接口推导的集成风险，尚未在本机复现。[当前 BMI270 读取](https://github.com/m5stack/M5Unified/blob/8b63555e7bd9ac71978c55ad868fd1d1d3b2ef3e/src/utility/imu/BMI270_Class.inl#L130)、[Bosch 中断 API](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi2.c)

## 实施前必须用实验回答的问题

1. 回读普通/省电/睡眠恢复后的 `0x21、0x40..0x43、0x49、0x53..0x58、0x7C..0x7D`，记录实际配置；用新样本 mask 与时间统计有效采样率。
2. 收集桌面、手持、磁吸、充电线牵引、揣兜走动、拿起/放下、正常按 A/B、播音时的负样本；分别测倾斜、晃动、双敲。
3. 单独验证机壳敲击与桌面碰撞是否可分；记录轴向、峰值、饱和、手持阻尼差异，避免预先承诺“轻敲即可”。
4. 先只在诊断页计数，不注入系统快捷键；测识别率、误触发次数/小时、延迟分位数与额外耗电，再定准入标准。
5. 若切换 Legacy，回归原有姿态、横竖屏、面朝下休息、any-motion 唤醒、失败回滚和音频并发；不让第一次唤醒动作直接变成快捷键。

以上为拟议验证方案，不是已完成测试。建议顺序：先做 B 键映射；IMU 第一阶段选择带静止基线、回中重置与冷却的倾斜离散事件；晃动与双敲默认关闭，待负样本验证后开放。
