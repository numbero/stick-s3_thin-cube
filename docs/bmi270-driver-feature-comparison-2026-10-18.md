# BMI270 官方配置与 ThinCube 当前驱动功能对比

日期：2026-10-18。范围：当前工作区固定 M5Unified `8b63555` 与 Bosch 官方 BMI270 SensorAPI 各应用配置。当前工作区固件标识已为 v0.4.41，IMU 仍使用 M5Unified/Base；本轮只更新调研文档，没有切换驱动或操作设备。

## 先区分两个层次

| 层次 | ThinCube 当前使用 | 前一轮迁移方案 |
| --- | --- | --- |
| ESP32 侧驱动/API | M5Unified 的 `M5.Imu`，外加项目自己的少量运动唤醒寄存器操作 | Bosch SensorAPI，由本地 `ImuService` 适配 |
| 加载到 BMI270 的应用配置镜像 | Bosch **Base**；已核对与官方 base blob 相同 | Bosch **Legacy** |

M5Unified 是板级库，仍负责屏幕、音频、按键和电源。Bosch 的 Base / Context / Legacy / Maximum FIFO 是同一 BMI270 芯片的不同应用配置，各自配套初始化及 feature 定义；它们不能被当成全部功能累加的升级序列，也不能把另一配置的 feature 地址直接写入当前配置。

当前驱动来源：[platformio.ini](../firmware/platformio.ini)、[M5Unified BMI270 初始化](https://github.com/m5stack/M5Unified/blob/8b63555e7bd9ac71978c55ad868fd1d1d3b2ef3e/src/utility/imu/BMI270_Class.inl)、[既有 blob 核对](walkie-input-imu-primary-research-2026-10-18.md)。官方配置分类：[Bosch README](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/README.md)。

## 官方应用配置功能

下表的“有”指对应配置提供片上算法/能力，**不代表 ThinCube 已接入，也不代表其他配置无法用原始数据自行实现相似交互**。“—”指对应配置没有提供这一片上 feature。

| 功能 | Base：当前镜像 | Context | Legacy：拟迁移 | Maximum FIFO | DSD |
| --- | --- | --- | --- | --- | --- |
| 原始三轴加速度、三轴角速度 | 有 | 有 | 有 | 有 | 有 |
| Any-motion：运动条件检测 | 有 | — | 有 | — | 有 |
| No-motion：静止条件检测 | 有 | — | 有 | — | 有 |
| Significant motion：显著位移活动 | 有 | — | 有 | — | — |
| 计步与步数事件 | 有，针对腕部佩戴 | 有，活动/计步用途 | 有，手机用途 | — | — |
| 活动识别 | 静止/走路/跑步 | 上下文活动，带时间戳 FIFO 事件 | 静止/走路/跑步 | — | — |
| 腕部手势：放下手臂、抬转、抖腕、内/外甩 | 有 | — | — | — | — |
| 抬腕唤醒识别 | 有 | — | — | — | — |
| 横竖屏与正反面方向检测 | — | — | 有 | — | — |
| Flat：平放条件检测 | — | — | 有 | — | — |
| High-G / Low-G：冲击/低重力检测 | — | — | 有 | — | — |
| 单敲 / 双敲 / 三敲 | — | — | **有** | — | — |
| 专用门状态检测 | — | — | — | — | 有 |
| 主要定位 | 腕部设备、手势与运动 | 活动与计步 | 敲击、设备方向、冲击等传统交互 | 大批量原始数据缓存 | 门状态 |

功能依据：[官方 README 功能清单](https://github.com/boschsensortec/BMI270_SensorAPI/blob/master/README.md)，并由各配置 feature 表复核；补充核对见[配置证据笔记](bmi270-variants-evidence-2026-10-18.md)。Context 的 AN000 还给出 Others/Still/Walk/Run/Bicycling/Invehicle 的上下文 FIFO 编码，携带前后活动及时间戳，不能只凭 README 三类简表认定它与 Base 活动输出完全相同；使用时须按选定配置与 API 解析验证。[Context AN000，第 13 页](https://www.bosch-sensortec.com/media/boschsensortec/downloads/application_notes_1/bst-bmi270-an000.pdf#page=13)

Maximum FIFO 官方给出 6 KiB 缓存；通用数据手册的标准 FIFO 为 2048 B。FIFO 是采样数据缓存，不是敲击事件队列，增大 FIFO 不会自动得到 tap。[FIFO 数据手册](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmi270-ds000.pdf#page=34)、[Maximum FIFO AN001](https://www.bosch-sensortec.com/media/boschsensortec/downloads/application_notes_1/bst-bmi270-an001.pdf)

这里的“显著运动”面向位置变化活动，不能直接等同于“摇一摇”；“腕部手势”也有佩戴与坐标假设，不等同于任意手持 StickS3 动作。Legacy 的 orientation/flat 可以用于交互，但与我们目前的姿态阈值、确认时间、会话仲裁不一定相同，不宜直接替换现有策略。

## 我们当前实际使用到什么

| ThinCube 能力 | 当前实现 | 切到 Legacy 后的功能影响 |
| --- | --- | --- |
| 正立、倒立、平放、朝下、横置 | 项目 `PoseDetector` 处理原始六轴数据 | 可保留；无需改成 Legacy 的 orientation 算法 |
| 显示旋转、对讲机进出与容错 | 项目姿态/显示/语音策略 | 可保留；需回归轴、单位和时间，防止迁移引入差异 |
| 横屏游戏倾斜控制 | 项目角度与相对基准计算 | 可保留 |
| 摇晃晕眩动画、普通拿起活动 | 项目 `motion.cpp` 与 `MotionActivity` | 可保留；并非调用 Base 的腕部手势算法 |
| 深睡运动唤醒 | Base 的片上 Any-motion + 项目 PM1 唤醒事务 | Legacy 也有 Any-motion，但 feature 地址/配置及恢复流程须迁移 |
| Bosch 片上抖腕、甩腕、抬腕唤醒 | 没有接入 | 换 Legacy 会放弃这组潜在片上能力；当前产品没有正在使用的对应功能 |
| 官方计步、走路/跑步分类 | 没有接入 | 不是此次切换的功能收益 |
| 机身敲击识别 | 没有接入 | Legacy 可提供新的片上识别事件，仍需外壳参数标定与防误触 |
| FIFO 批量读取 | 当前 M5Unified BMI270 路径未消费 FIFO | 换配置不会自动启用，需要额外采集代码 |

依据：[motion.cpp](../firmware/src/motion.cpp)、[活动检测](../firmware/src/motion_activity.h)、[姿态检测](../firmware/src/pose_detector.h)、[横屏输入](../firmware/src/landscape_input.h)、[运动唤醒](../firmware/src/motion_wake_registers.h)、[M5Unified 最新寄存器读取](https://github.com/m5stack/M5Unified/blob/8b63555e7bd9ac71978c55ad868fd1d1d3b2ef3e/src/utility/imu/BMI270_Class.inl#L130)。

因此，Base → Legacy 对本项目的核心价值是：**增加片上敲击及传统设备交互算法，保留原始六轴能力与运动检测；代价是放弃专门的腕部手势算法，并迁移兼容层与睡眠事务。** 现有摇晃、倾斜、对讲机姿态主要由项目软件实现，不能把它们误算为会随 Base 腕部算法一起消失。

## DSD 与 OIS 的补充定位

官方仓库还提供 DSD 专用配置，以及独立的 OIS 接口代码；它们的目的不在当前对讲机输入扩展上。

- **DSD**：源码 feature 表提供门状态及 any/no-motion，没有 tap。官方 README 的四类通用清单未覆盖这一专用配置，因此表格另按实际 DSD 源码核对，详见[证据笔记](bmi270-variants-evidence-2026-10-18.md)。[官方 DSD feature 表](https://github.com/boschsensortec/BMI270_SensorAPI/blob/master/bmi270_dsd.c)、[官方产品资料入口](https://www.bosch-sensortec.com/en/products/motion-sensors/imus/bmi270)
- **OIS**：面向光学防抖的独立数据接口/API，官方示例使用专用 SPI OIS 通道，并与所需应用配置配合初始化；不是增加敲击算法的另一种应用镜像。当前项目通过板内 I²C 使用 BMI270，切换 API 不会凭空得到专用 OIS 硬件连接。[官方 OIS 说明](https://github.com/boschsensortec/BMI270_SensorAPI/blob/master/OIS_README.md)

## 对本项目的选择

| 目标 | 适合的路线 |
| --- | --- |
| 维持现状，增加小倾斜或软件摇晃输入 | 继续 M5Unified/Base 即可 |
| 增加单/双/三敲片上识别 | **Legacy** |
| 腕戴、抬腕、甩腕类设备 | Base，另验证佩戴假设 |
| 计步与活动分类设备 | Context 或对应 Base/Legacy；按佩戴方式实测 |
| 高吞吐记录原始 IMU 数据 | Maximum FIFO；需另做应用层识别与唤醒方案 |
| 门状态检测 | DSD |

本次对比不证明任一配置在 ThinCube 外壳上的识别率、延迟或功耗优于另一配置。若选择 Legacy，后续执行已保存的[驱动迁移计划](bmi270-legacy-migration-plan-2026-10-18.md)，先完成诊断原型再决定产品化。
