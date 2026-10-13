# StickS3 开源电量方案核查（2026-10-13）

**本次未找到公开了 StickS3 原装 250mAh 电池全周期标定数据、并证明百分比误差的即用方案。** 同款项目多数仍采用线性电压换算。最值得借鉴的是 Zephyr 的可配置分段表框架，但其默认曲线不能直接作为本机校准结果。本次只读研究，没有改代码或设备。

## 五个主要实现

| 实现与固定提交 | 算法和可靠性 | 判断 |
| --- | --- | --- |
| [M5Unified e125e73f](https://github.com/m5stack/M5Unified/blob/e125e73f28b6b83e621e48df432af426bb2ff935/src/utility/Power_Class.inl#L2472) | M5PM1 分支读取 VBAT，再用 `(mV−3300)×100/(4150−3350)`，限制0–100；实际4100mV就饱和 | 官方读取基础；没有原装电芯表、负载补偿或精度报告 |
| [78/voicestick e865d68c](https://github.com/78/voicestick/blob/e865d68c1d96411571cbe1501a301ebe3c98f3b3/firmware/components/stick_s3_board/stick_s3_board.c#L245) | StickS3 BLE麦克风+Mac；公式与M5Unified相同；读失败返回错误，充电与USB供电分别读取 | 工况最接近本项目，但没有更准确的换算表 |
| [78/xiaozhi-esp32 e076ac49](https://github.com/78/xiaozhi-esp32/blob/e076ac496e2d93dc3a2308a0491d745afe9489ab/main/boards/m5stack/stick-s3/m5stack_stick_s3.cc#L219) | 同款StickS3；3000–4200mV线性换算，PMG0检测充电 | 没有分段标定；`discharging=!charging`会误归类外部供电但已充满的状态 |
| [ivanthengwr/sticks3_mic da37513b](https://github.com/ivanthengwr/sticks3_mic/blob/da37513b66f2141fcb8998f2bfc1af8b5058d0d0/main/display.c#L60) | 同款BLE麦克风；3000–4200mV线性，每秒显示；读取失败写0mV | 不建议复制其错误处理；[所谓charging只判断外部电源](https://github.com/ivanthengwr/sticks3_mic/blob/da37513b66f2141fcb8998f2bfc1af8b5058d0d0/main/m5pm1.cpp#L82)，不能识别充电完成 |
| [thc1006/zephyr-m5stack-sticks3 fe5b3101](https://github.com/thc1006/zephyr-m5stack-sticks3/blob/fe5b31011afd58ab33f2efb4112708588e7007a7/app/boards/m5stack_sticks3_esp32s3_procpu.overlay#L139) | 同款K150；M5PM1 VBAT→composite fuel gauge→11点曲线插值 | **软件结构最值得参考**；有真机接入日志，尚无全周期SOC误差证明 |

M5PM1 的 [`readVbat()`](https://github.com/m5stack/M5PM1/blob/be9a5456c007c333e7ac963f33bfde1ffa5d82ee/src/M5PM1.cpp#L3211) 提供mV，公开寄存器/API没有电池电流、库仑计或原生SOC。[作者issue #8](https://github.com/thc1006/zephyr-m5stack-sticks3/issues/8)也明确这一限制。电压估计SOC、剩余续航比例、实际容量mAh必须分开；配置`250000µAh`是设计容量，并非测得容量。

## Zephyr 默认表为何不能照搬

[官方v4.4.0曲线](https://github.com/zephyrproject-rtos/zephyr/blob/v4.4.0/include/zephyr/dt-bindings/battery/battery.h)注明来源 **Panasonic NCR18650BF**，不是StickS3原装电池。0%、10%……100%共11点，mV分别为：

`2502, 3146, 3372, 3449, 3532, 3602, 3680, 3764, 3842, 3936, 4032`

[Composite驱动](https://github.com/zephyrproject-rtos/zephyr/blob/v4.4.0/drivers/fuel_gauge/composite/fuel_gauge_composite.c)在没有原生SOC时[分段线性插值](https://github.com/zephyrproject-rtos/zephyr/blob/v4.4.0/include/zephyr/drivers/sensor/battery.h)。可以移植查表结构，无须把整个项目改成Zephyr；表应替换为本机标定数据。

[作者限制说明](https://github.com/thc1006/zephyr-m5stack-sticks3/blob/fe5b31011afd58ab33f2efb4112708588e7007a7/docs/02_SDD.md#L149)承认没有负载补偿、理想条件需要静置开路读数、板级曲线仍待实现。公开[HW-017日志](https://github.com/thc1006/zephyr-m5stack-sticks3/blob/fe5b31011afd58ab33f2efb4112708588e7007a7/evidence/20260605-hw017-battery-soc.log)约14秒，主要验证USB供电时4164–4176mV=100%；[验证记录](https://github.com/thc1006/zephyr-m5stack-sticks3/blob/fe5b31011afd58ab33f2efb4112708588e7007a7/docs/03_TDD.md#L501)没有容量仪真值对照。

由公式推导，3880mV在当前线性表约72%，在默认Zephyr表约84%。数值不同不能证明更准；换错表可能继续高估。4032mV以上一直100%也是该默认表的明确限制。

## 开发者讨论与其他候选

[Zephyr PR #109961](https://github.com/zephyrproject-rtos/zephyr/pull/109961)引入的是PaperColor及通用M5PM1驱动。StickS3作者报告I2C睡眠配置可能使首次事务只唤醒芯片、事务丢失，上游初始化因此重新设置sleep timeout。值得借鉴其读取可靠性经验；本项目已测I2C_CFG=0、读取失败计数0，不能将它定为本次断电根因。issue关闭也不等于完成SOC精度标定。

另核查了三项：

- [whitefirer/sticks3-toolkit 65963956](https://github.com/whitefirer/sticks3-toolkit/blob/6596395656126ec67f533b40129a05c31df571f1/firmware/main/pmic.c)：同款诊断，定义VBAT常量但没有电量算法。
- [M5StopWatch-UserDemo 6b4aa125](https://github.com/m5stack/M5StopWatch-UserDemo/blob/6b4aa125288b6fe9dca661f10159f6e1e5ee785c/main/hal/hal_pmic.cpp#L19)：其他设备，3300–4200mV线性，每秒读取、旧7新1的EMA；PMG2充电引脚也不同。
- [slochewie/M5StickS3-MQTT-Counter eafce70f](https://github.com/slochewie/M5StickS3-MQTT-Counter/blob/eafce70f2e32d109eb1d355e9ecc728b06385433/m5StickS3-mqtt-counter.yaml#L436)：同款ESPHome，实际3200–4200mV单一直线，未见标定来源。

M5Stack论坛检索了StickS3电量、battery percentage/voltage与power。找到的[启动讨论](https://community.m5stack.com/topic/8057/stick-s3-troubleshooting-help/6)与标定无关；本次范围内未找到同款容量仪对照及误差统计，不能用StickC/Plus2的其他PMIC方案替代。

## 补充：作者标定实践与专用电量计

以下由主代理补充，查阅日期同为 2026-10-13。它们用于比较可行路线，未在这台 StickS3 上安装或做精度验收。

### 同 M5PM1 的标定案例：ESPHome-m5stackPaperColor

[h0verin 的开源项目](https://github.com/h0verin/ESPHome-m5stackPaperColor)采用 1250mAh 电芯和定时唤醒工况。作者称根据 2026-06 的 43.93 小时完整放电记录拟合分段曲线；其百分比主要以该工况下的剩余时间比例定义。[项目说明](https://github.com/h0verin/ESPHome-m5stackPaperColor#readme)

[papercolor.yaml 的实际实现](https://github.com/h0verin/ESPHome-m5stackPaperColor/blob/main/papercolor.yaml#L415)包含五次电压采样取中值、分段插值，并把软件关机点映射成 0%。源码注释引用 history-24.csv；本轮未取得该原始记录或电流积分基准。它可借鉴的是“记录完整放电、拟合、独立周期验证”的方法。1250mAh 电芯和间歇工况的表不能当成 StickS3 250mAh 电芯的标定表；采样代码还需补齐逐次 I2C 错误检查，不能整段复制。参见同文件 L425–483。

### StickS3 作者实践：EucBuddy

[EucBuddy 作者手册](https://eucbuddy.com/instructions/)称设备百分比按真实放电曲线调优，并提供约每秒一行的设备电池曲线日志及 CSV 下载（手册的 Blackbox — battery map）。这支持本机记录和调优的工程流程。所查页面未公开具体曲线、误差基准或可复用电量源码，安装还涉及许可证；本轮不把它列为已核实的开源高精度实现。参考手册 Section 6 和 Section 11。

### MAX17048 + Adafruit：模型式 SOC，硬件接入相对简单

[Adafruit_MAX1704X 开源库](https://github.com/adafruit/Adafruit_MAX1704X)直接读取专用芯片的 SOC 寄存器，[cellPercent() 源码](https://github.com/adafruit/Adafruit_MAX1704X/blob/main/Adafruit_MAX1704X.cpp#L148)没有在主机上再套一条线性电压公式。实际估算来自 MAX17048 的 ModelGauge。[ADI 产品说明](https://www.analog.com/en/products/max17048.html)

该芯片不需要电流采样电阻，因此“有专用电量计”并不一定意味着库仑计。需要把它接到真实电池端和 I2C；它不直接测放电电流或实际 mAh。厂家手册指出自定义电芯模型表现更好，主机还应提供电池温度补偿；默认模型适合部分电池，不能保证任意 StickS3 电芯即插即准。CRATE 是百分比变化率，不能冒充电流。[MAX17048 数据手册，ModelGauge、温度补偿、CRATE](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX17048-MAX17049.pdf#page=7)

### BQ27441 + SparkFun：电流测量和容量学习路线

[SparkFun_BQ27441_Arduino_Library](https://github.com/sparkfun/SparkFun_BQ27441_Arduino_Library)的[基本示例](https://github.com/sparkfun/SparkFun_BQ27441_Arduino_Library/blob/master/examples/BQ27441_Basic/BQ27441_Basic.ino)展示 SOC、电压、电流、剩余及满充容量的读取。算法在芯片内，Arduino 库负责通信和配置。

BQ27441 使用电流采样电阻与 Impedance Track，能报告 mAh 并适应容量/内阻变化；厂家列出的设计容量范围含 250mAh。需要接入电池电流路径，单接 Grove/I2C 不能获得内置电池电流。[TI 产品说明](https://www.ti.com/product/BQ27441-G1)

要先匹配电芯化学体系、充电电压和设计参数。TI 快速指南要求设置设计容量、设计能量、终止电压和终充速率；仅把示例中的容量改为 250 不能完成准确性验证。默认参数、模型匹配和学习状态都要检查，再以实际电流积分及动态负载试验验收。[TI 快速指南](https://www.ti.com/lit/pdf/SLUUAP7)，[库配置接口](https://github.com/sparkfun/SparkFun_BQ27441_Arduino_Library/blob/master/src/SparkFunBQ27441.h#L83)

## 对当前 ThinCube 的推荐

这是结合公开源码与本机证据的工程判断：

1. 现有硬件继续使用 v0.4.15 的采样错误处理、供电切换重算和独立低压提醒。精度改进应放在电芯/工况标定上；不用未经标定的 18650 曲线替换当前公式。
2. 借鉴 Zephyr 的分段插值框架和作者项目的完整放电记录流程，建立本机自己的曲线。低负载静置与传音/Wi-Fi 的端电压不同，需分别记录并验证。仅以固定工况剩余时间生成的曲线，是该工况下的可用续航估计；真实 SOC 精度的参考值应来自电池侧电流积分。
3. 保留平滑，但约束滞后；低压告警独立于百分比。容量标定要排除十分钟空闲关机，并记录实际背光、活动计时、工作负载与关机原因，避免把配置亮度或断连当作实际状态。
4. 若产品目标需要更可信的 SOC 和容量/老化信息，评估带电流测量的专用电量计路线；若主要需要 SOC、希望简化电流路径改造，可评估 MAX17048。两者都需硬件适配和本机验证，不预先承诺百分比误差。

本轮没有找到在原装 StickS3 上公开完整基准数据、足以保证高精度的开箱即用纯软件方案。开源插值和滤波能复用，电芯参数与真实容量基准需要补测。
