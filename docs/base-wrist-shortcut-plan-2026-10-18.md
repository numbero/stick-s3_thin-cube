# 保留 Base，使用片上腕部手势映射快捷键

> 历史记录：v0.4.56 / Mac v0.4.24 已按用户要求移除双甩识别、映射和调试工具；以下描述对应当时版本。

日期：2026-10-18。范围：当前 M5Unified/Base 驱动、Bosch Base 腕部手势和现有 v0.4.41 A/B 输入链路。本文只做方案核对，未启用设备 feature、修改产品代码、构建安装应用或操作设备。

## 结论

**可以把 Base 已有的腕部手势事件作为快捷键输入来源，建议先验证这条路线。** 它可以保留当前 Base 配置镜像、M5Unified 原始数据读取与校准，以及现有 base any-motion 睡眠布局；无需为这组手势先切换 Legacy。

需要完成小范围 feature 适配和事件映射。当前 M5Unified 没有开放腕部手势的上层接口，产品也没有开启这组算法；“镜像具备能力”尚不等于当前固件已经能向 Mac 报告动作。手持 StickS3 与手表佩戴不同，识别效果仍需实测。官方示例已展示 Base 开启手势、读取状态和动作码的路径。[Bosch 示例](https://github.com/boschsensortec/BMI270_SensorAPI/blob/master/bmi270_examples/wrist_gesture/wrist_gesture.c)、[当前 M5Unified 驱动](https://github.com/m5stack/M5Unified/blob/8b63555e7bd9ac71978c55ad868fd1d1d3b2ef3e/src/utility/imu/BMI270_Class.inl)

## 可用手势与建议

| 官方事件 | 动作含义 | 用作快捷键的建议 |
| --- | --- | --- |
| `wrist_shake_jiggle`，码 3 | 快速抖手/抖腕 | 优先实验；一次完整动作执行一次组合键 |
| `flick_in`，码 4 | 缓慢转出，再快速转回原位 | 优先实验；可独立绑定一个组合 |
| `flick_out`，码 5 | 快速转出，再缓慢转回原位 | 优先实验；可绑定另一个组合 |
| `push_arm_down`，码 1 | 手臂在身前快速向下推动，再正常返回 | 先仅诊断，较容易与放下设备等日常操作重叠 |
| `pivot_up`，码 2 | 手臂在身前快速向上抬转，再正常返回 | 先仅诊断，较容易与拿起和改变说话角度重叠 |
| `unknown`，码 0 | 未识别 | 不映射 |

事件含义及码值来自官方手册 §4.8.7；表中试验顺序是产品判断。Flick 的区别是往返速度关系，不能仅凭名称给它们贴上“左甩 / 右甩”标签。算法有左右腕和坐标假设。[Bosch 手册第 50–52 页](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmi270-ds000.pdf#page=50)

## 为什么比 Legacy 迁移范围小

| 工作 | 保留 Base 的本方案 | 切换 Legacy |
| --- | --- | --- |
| 更换芯片配置镜像 | 不需要 | 需要 |
| 替换原始数据采样/校准路径 | 可继续使用 M5Unified | 需确定兼容方案 |
| 重做 any-motion 的配置布局 | 不需要换布局；需回归状态共存 | 需要迁移 |
| 新增动作 feature 启停和结果消费 | 需要 | 需要 |
| Mac 动作映射、一次性注入 | 需要 | 需要 |
| 手持误触、动作手感、音频影响验证 | 需要 | 需要 |
| 单/双/三敲片上识别 | 不提供 | 提供 |

因此，此路线降低的是驱动迁移风险，并不预先证明腕部算法的手持识别效果好于软件摇晃或 Legacy 敲击。只有目标明确需要敲击时，才继续评估 Legacy 路线。

## 最小接入路线

1. **继续由 M5Unified 初始化和读取 IMU。** 新增本地 Base 手势适配器，通过 `getImuInstancePtr(0)` 的公开寄存器接口进行受控访问，参考固定 Bosch 源码的 feature 定义。不开启第二个会调用 reset/init/upload 的驱动，不额外加载一份 Base 镜像。[底层实例接口](https://github.com/m5stack/M5Unified/blob/8b63555e7bd9ac71978c55ad868fd1d1d3b2ef3e/src/utility/IMU_Class.hpp)、[Base feature 表与函数](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270.c)
2. **检查配置后开启腕部算法。** 验证芯片类型、初始化状态、加速度配置及左右手参数；所有 feature 页读写遵守 APS 时序，保存并恢复页选择。先保留原始轴与校准路径，片上 feature 坐标单独核对。M5Unified 的逻辑轴变换不会自动配置 Bosch feature 坐标。[原始数据轴处理](https://github.com/m5stack/M5Unified/blob/8b63555e7bd9ac71978c55ad868fd1d1d3b2ef3e/src/utility/IMU_Class.inl)、[官方手势配置](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270.c#L3097)
3. **统一消费手势状态。** 可先轮询腕部 feature 状态，不要求新增 ESP 硬件 IRQ。M5Unified 当前读取 `INT_STATUS_1`；适配器优先只读 `INT_STATUS_0`，避免照抄 SDK 读两个字节的函数、抢走原始数据就绪位。读取动作结果时按所用输出寄存器/页的定义完成访问。配置页操作与睡眠准备由主循环串行管理。[原始采样](https://github.com/m5stack/M5Unified/blob/8b63555e7bd9ac71978c55ad868fd1d1d3b2ef3e/src/utility/imu/BMI270_Class.inl#L130)、[官方事件示例](https://github.com/boschsensortec/BMI270_SensorAPI/blob/master/bmi270_examples/wrist_gesture/wrist_gesture.c)
4. **按新事件生成一次性动作。** 动作结果会保持，禁用 feature 后也保留旧输出，不能持续读取到 3 就持续发键，也不能只靠“码值改变”判新事件，否则连续两次相同抖腕会漏掉。以新状态事件为依据生成序号，加冷却、过期和同会话校验。[手册 §4.8.7](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmi270-ds000.pdf#page=50)
5. **沿用现有语音上下文保护。** 只在用户启用并绑定、对讲机已就绪时对动作武装；进入前停稳并消费旧状态，退出/停止/断线/锁屏/编辑后丢弃候选。手势过程中确认退出语音姿态，就取消这次快捷键。睡前关闭腕部算法并清状态，只让原 any-motion 路径参与运动唤醒；失败恢复后按普通状态重新配置，醒来不补发动作。

具体寄存器、输出源与采样条件核对见[证据补充](base-wrist-shortcut-evidence-2026-10-18.md)。这些是拟议适配步骤，尚未做板上验证。

## 与现有 A/B 快捷键的关系

当前 v0.4.41 已有 A/B 独立 hold 输入、共享按键所有权和能力协商。本方案应接在这份实现之上，不能继续假定产品只有 A，也不能让手势覆盖 A/B 的松键状态。[v0.4.41 说明](walkie-b-shortcut-v0.4.41.md)、[设备输入来源枚举](../macos/Sources/ThinCubeCore/DeviceKeyStream.swift)、[注入器](../macos/Sources/ThinCube/KeyInjector.swift)

手势属于 pulse 输入：一个事件执行一次按下与自动释放。建议另加可选的带类型/序号/语音会话的手势事件及独立确认，或显式协商新版输入协议。现有 `DeviceButton` 只接受 `a/b`；把 `flick_in` 塞入旧 `edge.key` 会导致旧客户端解码失败，不能直接复用字段冒充按钮。

第一轮注入实验可只在 A/B 都松开时接受手势，不缓存被拦截动作。后续若需要“按住 A 的同时抖腕”，应为 pulse 增加独立所有权，并设计共享修饰键、同主键和不同组合同时保持时的行为。单纯不释放 A/B，仍可能使新组合受到它们持有的修饰键影响，必须明确产品策略。

## 先做的验证

建议先做**只记录动作、不发键**的诊断原型：

- 在实际倒置对讲机握姿下测试抖腕、flick in/out；分别尝试左右手和稳定握持基准，记录动作码、最大轮询间隔、识别耗时及语音是否仍处于同一会话。
- 将有意动作和按 A/B、拾放、正常说话换握姿、转动 USB 线、横屏游戏等负样本分开记录；不把“偶尔识别一次”当作成功。
- 观察是否必须采取接近腕表的姿态才能触发。如果动作幅度不自然或持续与退出语音冲突，优先保留 Base，用原始数据做设备专用手势，不继续强套腕部分类。
- 原型通过后才增加 Mac 的动作列表、逐项绑定、pulse 所有权及旧端兼容；动作默认关闭，无绑定时只确认消费。

最终判断标准是手持场景的命中、误触、手感和音频影响。保持 Base 已足够支持这个实验，暂时没有必要为验证腕部手势执行 Legacy 驱动迁移。
