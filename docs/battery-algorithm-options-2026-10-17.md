# StickS3 电量估算：官方与开源方案复核

查阅日期：2026-10-17，Asia/Shanghai。针对 ThinCube 的“关机充电后显示 100%，开机仍插 USB 却很快回落到 90%”现象，核查官方范例、底层实现及开源替代方案。本轮只做研究和文档记录。

后续决定：用户接受官方线性估算的局限，选择官方公式并保留既有滤波、低压提示和OTA保护，已在 [v0.4.30](battery-official-v0.4.30.md) 实施烧录。下文保留研究时的比较及备选建议，不表示启用了状态机或标定。

## 结论

在本次核查范围内，没有找到 M5Stack 官方或同款 StickS3 已公开完整容量对照、可直接替换并证明精度更高的估算算法。官方 StickS3 范例仍调用 M5Unified 的线性电压估计；UiFlow2 的同名接口也是 M5Unified 包装。换成官方公式会改变显示值，不能证明真实剩余容量更准确。

社区有更适合管理充电显示的实现：InfiniTime 将电压换算和充电状态管理结合。其思路可参考，但“外电存在且不充电就认为满电”和插电时百分比只增等规则，不适合直接套用于本机反复充停的情况。社区细节、固定提交与许可证见 [补充源码核查](battery-algorithm-community-options-2026-10-17.md)。

建议优先研究“运行状态管理 + 本机电芯与负载下的标定曲线”。这条路线可以改善显示一致性，但只凭 VBAT、VIN 和 CHG_STAT，不能承诺获得专用电量计那样的容量估计。准确率需要独立的容量或放电周期验证。

## 官方实现：没有更高级的现成 StickS3 算法

### StickS3 Arduino 范例与 M5Unified

[官方 StickS3 Battery 范例](https://docs.m5stack.com/en/arduino/m5sticks3/battery)每两秒调用 `getBatteryLevel()`、`getBatteryVoltage()` 和 `isCharging()` 并显示，范例自身不实现容量估算。

继续追踪到 [M5Unified Power_Class.inl](https://github.com/m5stack/M5Unified/blob/master/src/utility/Power_Class.inl)：ESP32-S3 的 M5PM1 分支读取电压，再执行：

```text
level = clamp((mV - 3300) × 100 / (4150 - 3350), 0, 100)
      = clamp((mV - 3300) / 8, 0, 100)
```

正数转换成整数时截断，实际在 4100mV 就达到 100%。公式中的 4150、3350 不应被误读为实际换算的满空端点。查阅到的远端默认分支文件 blob SHA 为 `cdec4ac38470f32edfaec3506d5324acc93e4f7d`；这不是 commit SHA。本工程固定依赖 `8b63555e7bd9ac71978c55ad868fd1d1d3b2ef3e` 中的电压换算公式相同。

| 相同电压输入 | 官方线性公式 | 本工程参考曲线，未计显示滞回 |
| --- | ---: | ---: |
| 4190mV | 100% | 100% |
| 4090mV | 98% | 90% |
| 4040mV | 92% | 84% |

这是公式对比，不是精度测试。两种算法都没有实际容量真值；官方的高百分比也不能确认本机已经充满。

M5Unified 的 StickS3 `isCharging()` 另行读取 PM1 GPIO0 的低有效 CHG_STAT，未用于上述容量计算。该信号为非充电时，不能单独区分满充、待机、保护或异常停充。

### UiFlow2

[官方 MicroPython 包装源码，固定提交 3d7741a](https://github.com/m5stack/uiflow-micropython/blob/3d7741a57f8c1f864e896f487cd0e7d65f44d8b4/m5stack/cmodules/m5unified/m5unified_power.c)通过 `MAKE_METHOD_0(power, getBatteryLevel)` 暴露同一底层接口；这里未找到独立的更准确 SOC 模型。不能把换用 Python API 当作算法升级。

### 其他 M5PM1 官方范例：StopWatch

[M5StopWatch 官方 HAL，固定提交 83de653](https://github.com/m5stack/M5StopWatch-UserDemo/blob/83de653c0be4e51e82079bee2386b90fb0538a25/main/hal/hal_pmic.cpp)使用 3300–4200mV 线性换算、每秒采样、旧值 7/8 与新值 1/8 的 EMA。读取失败时不更新电量。充电状态在独立函数中取得，未修正百分比。

这是另一型号、不同充电引脚的范例；其滤波机制本工程已有相同级别的实现，不能解决端电压与静置开路电压的差别。

## 社区可借鉴的两类改进

### 电源状态管理

InfiniTime 的源码核查和条件限制见 [社区方案记录](battery-algorithm-community-options-2026-10-17.md)。有价值的是把充电显示、满电状态与电压估值分开处理，防止把充电状态切换时的电压变化全部当作真实容量变化。

在本机上移植时必须保留“充电未知/停止原因未知”的状态，不能直接令 `USB && !charging` 等于满电。本机已记录到约 4.0V、外电在线且 CHG_STAT 为非充电；套用该条件会直接制造错误的满电判断。也不能仅因为 USB 在线就永久禁止百分比下降。

### 用实际电芯与工况标定曲线

[ESPHome-m5stackPaperColor 的实际代码](https://github.com/h0verin/ESPHome-m5stackPaperColor/blob/main/papercolor.yaml#L454)使用同一 M5PM1 的 VBAT、五次中值及分段曲线。作者注释称其表来自 1250mAh 电芯、五分钟休眠周期下约 43.93 小时的放电记录；区分了静置满电电压与充电维持电压，并把自身软件关机点作为可用电量 0%。

本轮核验了源码与作者说明，没有独立复核其容量真值或误差。这是标定方法的参考，1250mAh 电芯、休眠负载和关机阈值都不能直接复制到 StickS3 的 250mAh 电芯。按固定负载的剩余运行时间拟合，也不等同于电流积分测得的剩余 mAh。

Zephyr 的分段表/插值结构已经是本工程当前曲线的参考来源，重新引入默认表本身不构成改进。关键在于本机数据、运行条件与状态模型。现有 `battery_curve_generated.h` 中 per-device 标定仍为禁用状态。

## 真正模型式估算通常需要额外硬件

[MAX17048 官方说明](https://www.analog.com/en/products/max17048.html)使用芯片内的 ModelGauge 跟踪 SOC，无须电流采样电阻，并考虑负载和温度影响；温度补偿由主机配合。它说明“模型式估算”不一定等于库仑计。

[Adafruit_MAX1704X 开源库](https://github.com/adafruit/Adafruit_MAX1704X)提供的是芯片驱动，SOC 由 MAX17048 计算；不是把库安装到当前 M5PM1 上就能获得 ModelGauge。需要接入真实电池端与 I2C，并验证模型适配。本轮没有进行硬件修改、精度或容量验证。

## 建议的本机改进边界

1. 维持电压原始值、低压判断与显示估计的独立性。任何显示优化都应继续暴露持续电压下降，不能用满电锁存遮盖真实低压。
2. 区分外电、充电有效、停止充电后的稳定阶段、纯电池运行，以及传感读取未知。稳定期应由本机记录验证；不能只加一个任意等待时间就声称准确。
3. 满充需要多条件及持续时间验证，不能只靠一帧高电压或 CHG_STAT=false。本机 100% 只持续十余秒的样本不能作为已确认满充基准。
4. 电芯标定需要完整记录和独立周期验证。此前用户取消了物理标定，本次搜索不等于重新授权自动启动放电标定。
5. 评价时分别记录显示跳变、原始电压、真实可用容量/运行时间。减少跳变与提高容量估计准确性是两项不同验收标准。

本报告提出研究后的实现方向，尚未修改固件估算策略。前次同款源码调查可参考 [2026-10-13 记录](sticks3-community-battery-research-2026-10-13.md)。
