# BMI270 Legacy 驱动迁移：官方实现补充证据

日期：2026-10-18。供 [对讲机输入研究](walkie-input-imu-primary-research-2026-10-18.md) 与驱动迁移计划使用。核查固定 Bosch SensorAPI 提交 `41129fcfe39c583ee5462d79195741945d51c1fe`，没有操作设备、烧录固件或修改产品代码。临时下载、host 检查与交叉编译均在 `/private/tmp/thincube-imu-research`。

## 推荐的驱动边界

**建议保留 M5Unified 的显示、音频、PM1 与 I²C 管理，关闭其 IMU 自动初始化，由项目内 `ImuService` 独占 BMI270，并接入 Bosch Legacy API。** “独占”包括初始化、读数、feature 状态、校准、睡眠和恢复，而非同时保留两个会 reset/load/configure 同一芯片的驱动。此为工程方案，依据是初始化会 soft-reset、加载配置且重建驱动内部状态。[Legacy 初始化](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270_legacy.c#L1827)、[通用初始化](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi2.c#L1879)

“仅替换 M5Unified 的 8 KiB blob”可以作为短期对照实验，但仍要适配 tap 页配置、状态消费及 Legacy any-motion 唤醒地址；且直接改 `.pio/libdeps` 不能形成可复现依赖。若维持此路线，应做固定版本的局部派生/补丁，不能将其估算为改一个数组即完成。官方 feature 地址来自对应配置表；base 和 Legacy 的 feature 页布局及可用功能不一致。[base feature 表](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270.c#L487)、[Legacy feature 表](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270_legacy.c#L495)

## 最小 API 路径与接口成本

| 环节 | 官方接口或事实 | 迁移含义 |
| --- | --- | --- |
| 平台适配 | `bmi2_dev` 注入 `read(reg, buf, uint32_t len, intf_ptr)`、`write(reg, const buf, len, intf_ptr)`、微秒 `delay_us` | 可包装 `M5.In_I2C`，不必换整个板级库；M5 的 `true` 成功要转换成 Bosch 的 `0` 成功 |
| 初始化 | 清零 `bmi2_dev`，设置 I²C/回调/事务长度，调用 `bmi270_legacy_init` | 不能遗留指向 base blob 的 `config_file_ptr`；API 仅在指针为空时选择自己的 Legacy blob |
| 采样配置 | `bmi270_legacy_set_sensor_config` 设置 accel/gyro，保持量程与换算一致 | 首阶段明确设置参数并回读，不依赖复位默认值 |
| 双敲 | enable 列表含 `BMI2_ACCEL`、`BMI2_DOUBLE_TAP`；`config.type=BMI2_TAP` | 不必同时开启单击和三击；先双击可减轻产品仲裁成本 |
| tap 参数 | API 实际读写 `data_reg_en`、`tap_sens_thres`、`max_gest_dur`、`quite_time_after_gest`、`wait_for_timeout`、`axis_sel` | 先 `get` 再显式 `set`，并回读；结构中的 `tap_shock_dur`、`min_const_thres` **未被此固定版本的 tap getter/setter 使用**，不能当作已有可调能力 |
| 事件 | `bmi2_get_int_status` → tap 汇总位 → `bmi2_get_regs(0x20, ...)` | tap 汇总位为 `0x08`，类型 single/double/triple 分别为 `0x20/0x40/0x80` |

证据：[回调类型](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi2_defs.h#L1544)、[设备结构](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi2_defs.h#L2992)、[官方 tap 示例](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270_legacy_examples/tap/tap.c)、[tap 结构](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi2_defs.h#L2362)、[实际 getter/setter](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270_legacy.c#L2427)、[状态常量](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270_legacy.h#L94)。

事务长度建议显式取平台可支持的偶数，例如 32 或 64 字节；通用读接口的局部接收缓冲是 128 字节，不能将当前 M5 一次写 8192 字节的配置照搬成 SDK `read_write_len`。[通用读实现](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi2.c#L1959)、[分块加载](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi2.c#L5580)

## 轮询、200 Hz 与事件完整性

**在线检测可先轮询，不必先开发 PM1 IRQ 链。** 官方 tap 示例映射 tap feature 后，在循环内读状态和类型，并未等待 MCU 中断。不能据此保证“完全不配置任何中断映射也工作”；首版沿用示例的 feature 映射并禁用物理输出/PM1 路由的组合，需真机验证。[示例](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270_legacy_examples/tap/tap.c#L76)

AN002 的关键约束：`data_reg_en=1` 使用用户加速度数据，ODR 必须为 200 Hz；关闭时用内部 200 Hz 未滤波数据。低功耗滤波下 tap/high-G 要求至少 200 Hz，其他 feature 至少 50 Hz。非锁存模式下状态位保留至读取；锁存模式读对应状态寄存器会复位状态和引脚，若条件持续则重新置位。`wait_for_timeout=0` 可在同一手势窗口多次报告。[AN002，第 14、16、36、71 页](https://www.bosch-sensortec.com/media/boschsensortec/downloads/application_notes_1/bst-bmi270-an002-01.pdf#page=14)

`bmi2_get_int_status` 一次读取 **0x1C 与 0x1D 两个字节**；后者含 accel/gyro data-ready。如果接入后仍让 M5Unified 另行读取 0x1D，会出现两个消费者读清同一状态的问题。建议由 `ImuService` 一次消费、统一分发；局部补丁路线也可只读 0x1C 获取 feature 状态，但须留出清理/睡眠的一致性设计。[实际实现](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi2.c#L2441)

**推断与待验证：** 状态是位图、不是事件队列；轮询周期内多个同类事件可能合并，0x20 类型与清状态间也没有 API 提供原子快照。建议 10–20 ms 定期消费并记录最大间隔，不能承诺零漏报。离开对讲机、重新启用、唤醒时要清陈旧状态并重新布防，避免把上一次敲击重复发给上位机。具体轮询周期与布防时间是实验参数。

## 深睡 any-motion 的准确迁移面

Legacy any-motion 仍是 **4 字节配置**：page 1，offset `0x06`，即窗口寄存器 `0x36..0x39`；duration 与 XYZ select 在第一 word，threshold 与 enable 在第二 word，enable 为第二 word 的 bit 15（字节 `0x39` 的 bit 7）。持续时间仍按 50 Hz 样本表达，1 单位 20 ms。映射到 INT1/INT2 的 any-motion 位是 `0x40`，也在 `INT_STATUS_0` 的 bit 6。当前 base 起点是 offset `0x0C`，因此地址必须迁移。[Legacy 定义](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270_legacy.h#L64)、[enable 实现](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270_legacy.c#L2951)、[字段打包](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270_legacy.c#L3786)、[时长文档](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270_legacy.c#L1003)、[base 地址定义](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270.h#L76)。

**映射位澄清：** 固定 Bosch base 的 `BMI270_ANY_MOT_STATUS_MASK` 与 `BMI270_INT_ANY_MOT_MASK` 都是 `0x40`，与 Legacy 相同；项目 [motion_wake_registers.h](../firmware/src/motion_wake_registers.h) 也写 `out_conf=7` 并映射 INT1 `0x40`。base 手册允许通过 `out_conf` 配置路由，7 对应 bit 6；因此这里没有“base 为 0x20、Legacy 为 0x40”的迁移差异。要迁移的是 feature 地址、配置打包与恢复逻辑，并回归实机行为。[固定 base 宏](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270.h#L108)、[base ANYMO_2 路由表，第 92–93 页](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmi270-ds000.pdf#page=92)

**建议：** 深睡仅保留 any-motion，禁用 tap 后降低采样配置；PM1 引脚链路本身不用改线，但配置、清状态、失败撤销与恢复必须回归。不能仅因 Legacy 也支持 any-motion 就认定已兼容。

## 原始数据、坐标和旧标定的兼容陷阱

Bosch `bmi2_sec_init` 将 `dev.remap` 初始化为 identity。`bmi2_parse_sensor_data` 会对 gyro 做 Z→X cross-sensitivity 补偿，再依照 `dev.remap` 变换 gyro/accel；`bmi2_set_remap_axes` 同时改变 feature 映射与驱动的 remap 状态。[初始化](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi2.c#L1891)、[数据处理](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi2.c#L2852)、[映射设置](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi2.c#L4750)

当前 M5Unified BMI270 直接读原始数据，因此“直接换成 SDK sensor data，再沿用所有旧 NVS raw16.16 偏置”不能视为数值等价。[M5 原始读取](https://github.com/m5stack/M5Unified/blob/8b63555e7bd9ac71978c55ad868fd1d1d3b2ef3e/src/utility/imu/BMI270_Class.inl#L130)

首阶段建议 `ImuService` 经 `bmi2_get_regs` 读原始 accel/gyro，保留旧换算、传感器轴偏置减法、`X=+Y / Y=-X / Z=+Z` 的板级软件变换；Bosch API 统一管理配置与 feature。feature remap 保持 identity，双敲先选 sensor Z，板 Z 也为 sensor Z。若后续使用 SDK 的校正读数或 feature remap，应对标定版本做显式迁移，防止补偿或坐标变换两次。此为兼容方案，数值和姿态回归仍需完成。

## 固定版上游缺陷：已用实际源码验证

`set_config_page_primary` 的 `feat_config` 长度为 16；tap 起点为 0，`idx=7; idx++; idx=idx*2-start_addr` 得到 16，但复制循环为 `index <= idx`，会访问 `feat_config[16]`。[数组长度常量](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi2_defs.h#L285)、[实际数组](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270_legacy.c#L2639)、[循环](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270_legacy.c#L2694)

本次以**实际固定版 `bmi270_legacy.c` + `bmi2.c`**、假的 I²C 页寄存器、Clang C99 `-O0 -fsanitize=address,undefined` 调用实际 `set_tap_config`，ASan 报 `set_config_page_primary` 的 `stack-buffer-overflow`，读取大小 1。仅将这个循环 `<=` 改为 `<` 后，同一程序通过；逐字节比较 8 页，只有预期 tap page 5/6 的字段发生变化，其余字节保留。原始日志：`/private/tmp/thincube-imu-research/tap_boundary_original.log`。

这是 host 的边界错误复现与最小修复验证，**不是 ESP32 真机崩溃复现，也不证明 SDK 其余函数无缺陷**。正式 vendoring 要保留来源、固定提交、这一最小补丁与有意义的边界回归；迁移估算应预留依赖适配成本。

## 体积、RAM 与许可：可证实的范围

Legacy blob 实际提取长度为 **8192 字节**，SHA-256 为 `48b0b9c5a1423846a82473660ee2e8477eb118f6fc9f40f070b2a4d765310dff`。其为 `const uint8_t` 数组；替换同长度 base blob 本身不增加 blob 字节数，也不能将其直接算作 8 KiB 常驻 RAM。[数组](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi270_legacy.c#L55)

使用本机 ESP32-S3 工具链，在 tmp 目录以 `-Os -ffunction-sections -fdata-sections` 编译：

| 项目 | 本次静态结果 | 限制 |
| --- | ---: | --- |
| `bmi2.c` 对象 text | 18,832 B | 未链接、未裁剪 |
| Legacy 对象 text | 16,624 B | 含 8192 B blob；使用上述边界修复 |
| Legacy 对象 data | 28 B | feature interrupt map；另有 4 B common 指针符号 |
| ESP32-S3 ABI `sizeof(bmi2_dev)` | 112 B | 不含服务、队列和配置对象 |
| `sizeof(bmi2_tap_config)` | 16 B | 配置结构大小 |

这 **约 34.6 KiB 的未裁剪 text 不是最终 Flash 增量**：要考虑链接裁剪/LTO、旧路径是否被剔除、回滚是否同时保留 base blob。正式固件必须比对 `.map` 和实际栈高水位。通用读寄存器函数含 128 B 局部缓冲；本次源码检索未发现这两个 C 文件调用 malloc/calloc/realloc，但不据此推算端到端 RAM 或 CPU/功耗。[读缓冲实现](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi2.c#L1959)、[设备结构](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/bmi2_defs.h#L2992)

仓库 LICENSE 和源文件 SPDX 为 **BSD-3-Clause**：允许源码/二进制再分发与修改，但源码及二进制分发材料须按条款保留声明，不能借 Bosch 名义背书；正式引入时带上原 LICENSE 和修改记录。[官方 LICENSE](https://github.com/boschsensortec/BMI270_SensorAPI/blob/41129fcfe39c583ee5462d79195741945d51c1fe/LICENSE)

## 迁移前的实验门槛

1. 先证明 Legacy 初始化、配置读回、单一状态消费者与双敲诊断计数工作；不要直接发系统快捷键。
2. 比较迁移前后同姿态的原始值、应用值和旧标定；把 SDK cross-sensitivity 路径作为单独决策。
3. 验证轮询受音频/网络阻塞时的事件合并、类型读数、重复触发，以及关闭/重启/唤醒时陈旧状态清理。
4. 完成 Legacy any-motion 深睡唤醒、失败撤销、恢复后的采样与首次动作隔离，再进入快捷键集成。
5. 真机测识别率、误触率、延迟和额外电流。官方支持 tap 只证明算法存在，不能证明 ThinCube 外壳和使用方式达标。
