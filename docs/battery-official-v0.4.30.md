# v0.4.30：M5 官方电量换算

日期：2026-10-17。用户接受官方线性电量估算的局限，要求保留现有滤波、低压提示和 OTA 保护，并编译烧录到 ThinCube-6046E5。

## 实现

显示百分比改为工程固定的 M5Unified 公式：

```text
percent = clamp((millivolts - 3300) / 8, 0, 100)
```

正数整数换算截断，4100mV 达到100%。该值仍是电压估计，不是容量实测或充电完成证明。遥测模型名为 `m5unified-linear`。

继续使用原有每秒采样、七点中值、EMA、预热、约2个百分点显示滞回、供电切换重置和五秒过期未知处理。电池供电下的低压确认/恢复条件没有改变。

原来的 LiPo 参考曲线继续作为独立 OTA 保守上限。`BatteryGauge::setOtaCurve()` 与显示曲线分开，OTA 仍取原始、中值和滤波电压在官方线性及原参考曲线中的最小估值。因此显示变高不提高 OTA 的电量许可值。

本次没有启用此前提出的状态机、满电锁存或物理标定。Mac 协议字段未变，现有上位机能读取新的模型名；未重建 macOS 应用。

## 测试与构建

- 30 个固件 C++ 测试程序、5 个 Python 测试通过。
- 对2500–4500mV的每个整数电压，验证显示符合官方公式，同时新旧策略的 OTA 电量和低压判断一致。
- 充电回落、供电切换、未知供电、无效读数、持续低压和过期场景继续覆盖；已有滤波及供电策略测试通过。
- 最终应用镜像通过 Mac 端 `AudioTests.testBuiltFirmwareIdentityWhenProvided` 身份兼容测试。
- 协作聊天基于同一最终镜像运行的完整83个Swift测试全部通过，证据为 `artifacts/clawd-light-v0.4.30/swift-tests.log`。
- release 编译成功；静态 RAM 106304 字节，Flash 使用1745501字节，应用镜像1745936字节。

最终镜像：`artifacts/battery-official-2026-10-17/release/thincube-v0.4.30.bin`。

SHA-256：`ccddf09751ba348105064cdc394c6e543fe97aff89c1016ad26a6c57bc97b6e5`。

## 与光点修复合并

按用户直接授权，与“合并功能并验证固件”聊天协调，保留其最终光点方向、灵敏度、回正和屏幕旋转锁定修复。该聊天先完成v0.4.29后释放串口，由本聊天统一构建并烧录v0.4.30。

为避免并行构建覆盖，使用隔离的 `artifacts/battery-official-2026-10-17/build-project` 构建。其源文件与最终共享工作区逐项SHA-256匹配，记录为 `final-source-hashes.json`；共享 `firmware/platformio.ini` 也已更新为v0.4.30。此目录的应用镜像是本次实际烧录产物，原共享 `.pio/build` 中先前构建不能替代它。

## 设备、备份和烧录

USB核对 MAC `7c:e8:b1:e5:46:60`，对应chip `6046e5b1e87c`。完整读取8MiB Flash，确认旧版v0.4.29，解析实际分区表和OTA数据CRC，确定活动槽为app0（0x10000）。

备份：`artifacts/battery-official-2026-10-17/pre-v0.4.30-flash-8MB.bin`。

备份SHA-256：`7356f7b0ee14ec35c4aae4b12027d4dbdac639947ee1d02afb6d4a9caa1525a7`。

只写app0应用镜像，esptool写入hash校验成功，随后watchdog复位。烧录后独立读取并比较分区表、NVS和OTA数据，三者均与完整备份中的对应字节完全一致。

启动日志确认 `=== clawd v0.4.30 ===`、`app0 @ 0x010000`、135×240 LCD；BLE自动加密认证成功。上位机遥测已收到 `m5unified-linear`：例如filteredMv=3989、percent=86、otaPercent=77、failures=0，符合“显示官方公式、OTA保留旧保守上限”的预期。

连续观察到启动70秒，仅一个启动头，无观察期间重启。67个有效BLE样本逐项核对：显示与官方公式的差异在原有1%滞回内，OTA估值与原参考曲线保守规则一致，VBAT读取失败为0。检查完成后已关闭USB监视器，将串口交给光点修复聊天继续验证。

运行日志及完整验证结果位于忽略目录 `artifacts/battery-official-2026-10-17/`。原始电压持续变化的电气原因不属于此次换算改动的已修复范围；光点的进一步物理操作验收由对应聊天继续完成。

背景：[官方与社区比较](battery-algorithm-options-2026-10-17.md)、[排查记录](battery-80-investigation-2026-10-17.md)。
