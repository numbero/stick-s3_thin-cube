# Base 腕部手势作为快捷键：核对证据

> 历史记录：v0.4.56 / Mac v0.4.24 已按用户要求移除双甩识别、映射和调试工具；以下描述对应当时版本。

日期：2026-10-18。只读核对，不包含产品代码修改、构建或硬件操作。结论：可保留当前 Base 配置和 M5Unified 原始采样，补接片上腕部手势；是否适合手持 ThinCube，仍须实测。

## 片上输出与动作含义

| 输出码 | Bosch 名称 | 使用时应如何解释 |
| --- | --- | --- |
| 0 | Unknown / No gesture | 不产生快捷键 |
| 1 | Push arm down | 前伸手臂快速下压，再正常返回 |
| 2 | Pivot up | 前伸手臂快速向上转起，再正常返回 |
| 3 | Wrist shake / jiggle | 腕部快速抖动，非任意手持平移摇晃的保证 |
| 4 | Flick in | 慢速向外转腕，快速转回 |
| 5 | Flick out | 快速向外转腕，慢速转回 |

这是为 Wear OS 腕戴导航设计的五类动作，存在左右腕与坐标假设。不能把五个输出码理解为五种无需训练、任意握持都稳定的输入。[Bosch 数据手册 §4.8.7，第 50–52 页](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmi270-ds000.pdf#page=50)

## 时间与配置约束

- `wearable_arm`：0 左腕（默认），1 右腕。
- `min_flick_peak`：`2048 * sin(angle)`，1448–1774，默认 1774。
- `min_flick_samples`：50 Hz 下 3–5 样本，60–100 ms，默认 80 ms；这是转出与转回的最小时间差，不是端到端延迟。
- `max_duration`：50 Hz 下 150–250 样本，3–5 秒，默认 4 秒；这是动作完成上限，不代表每次必等 4 秒。

数值也见[固定提交 `bmi2_defs.h` 的 `bmi2_wrist_gest_config`](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi2_defs.h#L2460)。本次核对的 v1.6 手册 §4.8.7 没有给出统一“动作前后必须静止 N 秒”的数字；产品增加停稳和冷却期属于自己的防误触策略，应与官方约束分开表述。

片上算法使用加速度。高性能滤波时功能性能不依赖主机设置的 ODR/带宽；低功耗滤波时至少 50 Hz，并满足带宽/平均约束。轴重映射仅作用于 feature 输入，不改变 raw DATA/FIFO，因此可独立适配手势坐标，保留现有 M5Unified 软件坐标。[数据手册 §4.8.1，第 41–42 页](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmi270-ds000.pdf#page=41)

## 当前驱动能否最小接入

本机 M5Unified `BMI270_config.inl` 与固定 Bosch base 的配置数组均为 8192 字节，逐字节一致；SHA-256 为 `2d75e68e343a13ff99be98261dfbd99d9e8c6f267da9e3c5aeb883276ad178db`。当前已经上传能运行该算法的配置，无须更换为 Legacy。

最小扩展可参考固定官方源码，在现有 BMI270 单一初始化完成后，通过同一 I²C 总线封装 feature 的配置、使能、状态与输出读取：

- 配置在 page 6 / offset `0x06`；`enable` 是该字节 bit 5，`wearable_arm` 是 bit 4；保持原有其他位，按完整 16 位 word 或完整 page 读改写。
- `out_conf=5` 将腕部手势分配给 `INT_STATUS_0` bit 4；硬件 INT 引脚映射是另一层设置。诊断可轮询状态，不要求先新增 MCU 中断处理链。
- 输出在 page 0 / offset `0x06`；只有新手势状态成立才读入一次事件。禁用 feature 后输出保留旧值，不能把持续非零输出当作持续触发；也不能只比较“输出码改变”，否则会丢失连续两次相同手势。
- M5Unified raw 读取 `INT_STATUS_1`（`0x1D`）和数据寄存器；腕部事件在 `INT_STATUS_0`（`0x1C`），正常采样可共存。但本项目睡眠准备代码会连读 `0x1C` 两字节以清状态，必须由同一 IMU 管理入口统一仲裁，进入睡眠前关闭手势，取消睡眠或重新初始化后恢复。
- **不应并列创建并初始化第二个 Bosch 完整驱动。** `bmi270_init` 会走芯片重置/配置上传；这会破坏原 M5Unified 的采样和状态。采用最小 feature 桥接即可，或将来统一迁移全部 IMU 所有权。

依据：[固定 feature 表](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270.c#L487)、[使能实现](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270.c#L2489)、[参数设置](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270.c#L3093)、[输出读取](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270.c#L3785)、[官方示例的状态驱动流程](https://github.com/boschsensortec/BMI270_SensorAPI/blob/master/bmi270_examples/wrist_gesture/wrist_gesture.c)、[M5Unified 实现](https://github.com/m5stack/M5Unified/blob/master/src/utility/imu/BMI270_Class.inl)。

## 对方案的约束

优先把抖腕、Flick in/out 作为三个实验候选，先观察诊断事件、在固定握姿下评估识别和误触。不要首轮把抬起/压下动作映射为快捷键，它们更容易与正常取放、现有翻转对讲机姿态发生冲突。手势属于离散事件，宜映射一次按下加释放；不能直接代替 A/B 的持续按住输入。模式确认、连接状态、按键同时操作、睡眠切换、冷却与事件去重仍需业务层处理。
