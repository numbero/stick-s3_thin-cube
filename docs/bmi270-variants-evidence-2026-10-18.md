# BMI270 官方配置功能核对补充

核对日期：2026-10-18。只读资料核对，没有修改产品代码或操作设备。此前研究固定提交为 `41129fcfe39c583ee5462d79195741945d51c1fe`；本次 base/Legacy 沿用该提交的本机源码、Maximum FIFO 已直接核对该提交，README/Context/DSD/OIS 因固定链接无法取回而核对 Bosch 官方 `master`，不得将后几项误记为已完成固定提交核验。

## 关键功能矩阵

这里的“无”指该配置没有提供相应片上算法，并不意味着 MCU 不能读取六轴数据后自行实现。

| 功能 | base | Context | Legacy | Maximum FIFO | DSD |
| --- | --- | --- | --- | --- | --- |
| 加速度、角速度原始数据 | 有 | 有 | 有 | 有 | 有 |
| any-motion / no-motion | 有 | 无 | 有 | 无 | 有 |
| significant-motion | 有 | 无 | 有 | 无 | 无 |
| 计步器、步进事件 | 有，腕戴优化 | 有，腕戴优化 | 有，手机用途 | 无 | 无 |
| 活动分类 | 静止/走/跑 | 有，带 FIFO 上下文帧 | 静止/走/跑 | 无 | 无 |
| 腕部手势、抬腕唤醒 | 有 | 无 | 无 | 无 | 无 |
| 单敲、双敲、三敲 | 无 | 无 | 有 | 无 | 无 |
| 横竖屏方向、平放检测 | 无 | 无 | 有 | 无 | 无 |
| high-G、low-G/自由落体 | 无 | 无 | 有 | 无 | 无 |
| 开关门状态、门角度 | 无 | 无 | 无 | 无 | 有 |
| FIFO 容量 | 通用数据手册为 2 KiB | 2048 B | 2 KiB | 6 KiB | 2 KiB |

来源：[Bosch README（master）](https://github.com/boschsensortec/BMI270_SensorAPI/blob/master/README.md)、[base feature 表（固定提交）](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270.c#L487)、[Legacy feature 表（固定提交）](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270_legacy.c#L495)。

## 容易误判的点

1. **Context 不继承 base 的运动和腕部功能。** 实际 `bmi270_context_feat_in` 只有配置维护项、计步项与 activity recognition；中断表只有 step counter/detector。不是看到共用 `bmi2_defs.h` 有枚举，就认为所有配置都支持。[Context feature 表](https://github.com/boschsensortec/BMI270_SensorAPI/blob/master/bmi270_context.c#L487)
2. **Context 的活动细节超出 README 三类。** AN000 第 13 页列出 FIFO 帧中的 previous/current activity 和时间戳，类别含 Others、Still、Walk、Run、Bicycling、Invehicle。概览宜写“计步与上下文活动识别”，如果列全类别则注明 AN000；不能保证这些类别对手持 ThinCube 有足够准确率。同页明确 FIFO 为 2048 B。[AN000 第 13 页](https://www.bosch-sensortec.com/media/boschsensortec/downloads/application_notes_1/bst-bmi270-an000.pdf#page=13)
3. **Maximum FIFO 是删除片上高级功能换 FIFO 空间。** 固定提交的 feature input/output 表均为空；官方 AN001 第 6 页明确用于不需要上述功能、但重视 FIFO 的情形，并给出 6 KB。常规原始传感器、数据就绪和 FIFO 中断不应与高级算法事件混为一谈。[固定源码空表](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270_maximum_fifo.c#L69)、[AN001 第 6 页](https://www.bosch-sensortec.com/media/boschsensortec/downloads/application_notes_1/bst-bmi270-an001.pdf#page=6)
4. **DSD 是门锁专用配置，不是 base 加门检测。** 实际高级 feature 表仅有 door state detector、any-motion、no-motion；无 tap、significant-motion、计步和腕部算法。AN005 第 6 页也仅列门状态和 any/no-motion；第 16 页说明以 200 Hz 陀螺仪判断开/关及门角度，包含门安装轴映射与专用校准。第 12 页给出 2 KB FIFO。[DSD feature 表](https://github.com/boschsensortec/BMI270_SensorAPI/blob/master/bmi270_dsd.c#L489)、[AN005 第 6 页](https://www.bosch-sensortec.com/media/boschsensortec/downloads/application_notes_1/bst-bmi270-an005-03.pdf#page=6)、[第 12 页](https://www.bosch-sensortec.com/media/boschsensortec/downloads/application_notes_1/bst-bmi270-an005-03.pdf#page=12)、[第 16 页](https://www.bosch-sensortec.com/media/boschsensortec/downloads/application_notes_1/bst-bmi270-an005-03.pdf#page=16)
5. **OIS 不是又一种同级算法配置。** 官方 OIS README 要求集成 `bmi2_ois` 加“所需 variant 文件”；初始化/主机配置经 primary host 接口，OIS 配置和数据经独立 SPI OIS 接口。它面向光学防抖数据通路，不提供敲击识别。[OIS README](https://github.com/boschsensortec/BMI270_SensorAPI/blob/master/OIS_README.md)
6. **Legacy 的名称是用途兼容，不等于过时或只能旧芯片使用。** AN002 第 6 页说明这是 BMI270 上提供 BMI160 风格低功耗 feature 的配置，目标是兼容旧功能需求；第 13 页明确 FIFO 仍为 2 KB。[AN002 第 6 页](https://www.bosch-sensortec.com/media/boschsensortec/downloads/application_notes_1/bst-bmi270-an002-01.pdf#page=6)、[第 13 页](https://www.bosch-sensortec.com/media/boschsensortec/downloads/application_notes_1/bst-bmi270-an002-01.pdf#page=13)

对本项目的含义：Legacy 增加的是芯片上的敲击、方向、平放和高低 G 算法；读取六轴后由 MCU 做的倾斜/摇动逻辑仍可保留。代价是不能同时假定保留 base 的腕部算法。用户当前的动作若由业务软件自行识别，应区分“芯片功能变化”与“产品行为是否真的受影响”。
