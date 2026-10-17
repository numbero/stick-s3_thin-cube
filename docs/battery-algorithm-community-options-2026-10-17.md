# 可借鉴的开源电量算法（2026-10-17）

本轮围绕“关机充电显示 100%，插电开机后迅速显示 90%”核查了四个项目的实际源码。**InfiniTime 的供电状态处理最相关，但它改善的是显示一致性；未找到可直接移植、且经 StickS3 原装电池验证的高精度纯软件算法。** 当前 M5PM1 提供的电压、外部供电状态和 CHG_STAT，不包含电池侧电流或实际容量；加入状态机不会自动补齐这些信息。官方 M5Stack 方案另见本轮主报告。

以下链接固定到本轮从仓库 API 核实的提交；核查范围是列出的实现分支，不代表各项目所有硬件都采用同一种估算方式。本轮没有修改固件、充电设置或设备。

## 1. InfiniTime：最值得借鉴状态管理，不能原样复制

- 固定提交：`6c119eb52206b580b556b41633dddc1e1b66a8da`。
- [BatteryController.cpp](https://github.com/InfiniTimeOrg/InfiniTime/blob/6c119eb52206b580b556b41633dddc1e1b66a8da/src/components/battery/BatteryController.cpp#L18)，[许可证 GPL v3](https://github.com/InfiniTimeOrg/InfiniTime/blob/6c119eb52206b580b556b41633dddc1e1b66a8da/LICENSE)。
- 输入为电池电压、独立的电源存在信号、充电信号，与本机现有输入类型接近。其电芯和电源电路是 PineTime，不是 StickS3。

源码规则逐项核实：

| 规则 | 实际位置 | 对本机的意义 |
| --- | --- | --- |
| 外电存在且未充电，就设 `isFull=true`；只在拔掉外电时清除 | L18–27 | **不能直接采用**：本机在约 4.0V 已观察到未充电状态，这不能证明充满 |
| 六点分段曲线：3500/0、3616/3、3723/22、3776/48、3979/79、4180/100（mV/%） | L63–65 | 借鉴表结构；不能把 PineTime 曲线当作 StickS3 标定 |
| 未锁存满充时，充电中最多显示 99%；满充时目标 100% | L78–82 | 借鉴区分“接近满”和“满充已确认”的语义；本机仍需可靠确认条件 |
| 外电存在只允许百分比增加；离电只允许减少；首次测量例外 | L84–88 | 能压住插电 100→90 的跳变；但可能把真实插电放电或充电异常藏起来 |

`isFull` 在外电不断开的情况下即使后来重新开始充电也仍保持。故整套照搬会让当前设备容易错误锁定 100%。适合参考的是“原始电压估计”和“展示状态”分层，以及明确管理供电状态；满充判据、降电量条件必须重新设计。上述显示规则没有提供本机 SOC 误差保证。

## 2. Meshtastic：成熟的 OCV 插值与充电封顶，未解决本次根本限制

- 固定提交：`fad52bba0db68aff926aa02a55f4d2c4989f68dd`。
- [AnalogBatteryLevel::getBatteryPercent](https://github.com/meshtastic/firmware/blob/fad52bba0db68aff926aa02a55f4d2c4989f68dd/src/Power.cpp#L380)，[电压滤波](https://github.com/meshtastic/firmware/blob/fad52bba0db68aff926aa02a55f4d2c4989f68dd/src/Power.cpp#L502)，[许可证 GPL v3](https://github.com/meshtastic/firmware/blob/fad52bba0db68aff926aa02a55f4d2c4989f68dd/LICENSE)。
- ADC 分支把单节电压在可配置 OCV 点间线性插值；可用充电引脚时，正在充电且估值超过 99% 就封顶 99%。电压包含低通滤波，系数 0.5。
- 本机已有分段表、滤波和充电引脚，复制这部分不会消除充电极化、开机负载及停充回落导致的偏差。其“99% 直到停充”的约束仅避免过早显示 100%，没有可靠判定本机实际满充，也没有进行电流积分。
- 源码另有 PMIC/专用电量计分支；那些分支报告的 SOC 不能归功于这段 ADC 算法。不能把其他硬件的效果当作 M5PM1 效果。

## 3. ZMK：适合区分传感器能力，电压模式没有更高级估算

- 固定提交：`4dd95d7840eb3d5a3f59bb29fbf1ab054715ef85`。
- [app/src/battery.c](https://github.com/zmkfirmware/zmk/blob/4dd95d7840eb3d5a3f59bb29fbf1ab054715ef85/app/src/battery.c#L37)，[许可证 MIT](https://github.com/zmkfirmware/zmk/blob/4dd95d7840eb3d5a3f59bb29fbf1ab054715ef85/LICENSE)。
- `LITHIUM_VOLTAGE` 模式在低于等于 3450mV 时返回 0%、高于等于 4200mV 返回 100%，中间用整数公式 `mV * 2 / 15 - 459`。
- `STATE_OF_CHARGE` 模式直接读传感器的 SOC 通道，不在这里估算。它需要底层设备或驱动提供 SOC；选择这个模式不会让 M5PM1 增加 SOC 能力。
- 相比本机现有分段曲线，这个电压模式没有可证实的优势，且未加入充电/回落处理。不建议为了引用成熟项目而换回它的线性公式。

## 4. LibreSolar：真正跟踪电荷的开源路线，需要电池侧电流

- 固定提交：`779d20199bff2666e01371ce2f562dcda4d6bda3`。
- [app/src/bms_soc.c](https://github.com/LibreSolar/bms-firmware/blob/779d20199bff2666e01371ce2f562dcda4d6bda3/app/src/bms_soc.c#L11)，[许可证 Apache-2.0](https://github.com/LibreSolar/bms-firmware/blob/779d20199bff2666e01371ce2f562dcda4d6bda3/LICENSE)。
- 初始化或显式重置时，从指定百分比或 OCV/SOC 点插值获得起点；日常更新积分实际电流乘时间，并按额定容量换算百分比；累计变化超过 0.1 个百分点才更新输出，范围限制 0–100。
- 它展示了电压只负责起点/校正、运行中跟踪电荷的基本路线。输入需要电池侧实际电流、时间间隔和容量参数；本机 M5PM1 没有这个电流输入，不能拿 USB 电流或“充电=true”代替。
- 额定容量也不等于实测满充容量；电流偏置、容量衰减、初始误差仍需校准。此代码不是可直接安装到现有 StickS3 的无硬件解决方案。

## 对 ThinCube 的具体建议

这是根据源码与本次日志作出的工程判断，尚未实施或做电量精度验收：

1. **优先改善状态处理和可解释性。** 明确区分外电存在、正在充电、停充后的回落阶段、满充确认不足、正常放电。外电存在不等于电池净充电，CHG_STAT 不活跃不等于满充。
2. 借鉴 InfiniTime 把展示状态与瞬时电压估值分开。可以限制短时电压扰动引起的展示跳变，但不能无限锁存 100% 或规定插电绝不降电量。持续电压下降、低压保护和充电异常必须仍可见。
3. 本机曲线需使用本机电芯和实际工况数据，并经过独立周期验证。停充/开机瞬态不宜直接用作真实容量变化的证据。不同负载曲线或经验补偿需要实测支持，不能随意加固定毫伏数。
4. 若产品要求真实 SOC、mAh 和容量老化，评估专用电量计或增加电池侧电流测量；再使用库仑计/模型融合。只把查表换成“卡尔曼滤波”而缺少模型、电流及标定数据，不能解决信息不足。

此前社区调研见 [2026-10-13 记录](sticks3-community-battery-research-2026-10-13.md)，包括同款 StickS3 项目、Zephyr、PaperColor 标定和 MAX17048/BQ27441 路线。本轮最有新增价值的是 InfiniTime 的状态管理实例，以及明确区分显示稳定、曲线适配与真实容量估算三件事。
