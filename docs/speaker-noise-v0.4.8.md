# StickS3 开机持续杂音排查与修复

现场排查始于 2026-10-10，设备为 ThinCube-6046E5，原固件 v0.4.7。用户报告电池与 USB 供电时都有持续细小杂音，USB 时更大，开机即存在。

## 已确认的原因与修复

本项目固定的 M5Unified 提交 `8b63555e7bd9ac71978c55ad868fd1d1d3b2ef3e` 在 `config_t` 中将 `output_power` 默认设为 `true`，初始化时调用 `Power.setExtOutput(cfg.output_power)`。项目此前直接使用默认配置，导致未使用的 EXT_5V 升压输出一直开启。

实机 M5PM1 的 PWR_CFG（0x06）起初为 `0x1f`；只关闭 EXT_5V 对应 bit 3 后为 `0x17`。屏幕、IMU、麦克风、无线通信和充电保留，用户确认杂音“明显变小，但仍有”。

v0.4.8 在 `M5.begin(cfg)` 前明确设置 `cfg.output_power=false`，并在启动日志输出 `ext5v_out` 的读回值。其他运行逻辑保持原设置。未来使用 Grove、Hat EXT_5V 或红外供电时，需按外设需求显式启用 EXT_5V。

## 隔离试验

下列试验均在 USB 连接、EXT_5V 升压已关闭的条件下进行，听感由用户现场反馈。

| 运行条件 | 用户听感 |
| --- | --- |
| 正常主循环，仅关闭 EXT_5V 升压 | 明显变小，但仍有 |
| 暂停主循环，保留麦克风任务、无线后台任务和供电 | 完全消失 |
| 只恢复约 30 fps 的屏幕绘制与传输 | 完全安静 |
| 只恢复 IMU 读取和姿态计算 | 完全安静 |
| 只恢复每秒一次电量、USB 电压和充电状态读取 | 完全安静 |
| 只恢复 M5.update，包括按键和电源按键轮询 | 完全安静 |
| 只恢复 deviceTick，循环等待 1 ms，上位机控制恢复 | 很轻的持续滋滋声，需凑近听 |
| 暂停 deviceTick，保留 1 ms 循环与麦克风数据消费 | 长间隔偶尔滋滋一下，随后安静 |
| 退出上位机，继续 deviceTick、1 ms 循环、麦克风和 Wi-Fi STA | 完全安静 |

上位机 Wi-Fi 控制心跳间隔为 100 ms。上述对照将剩余持续杂音定位到活跃控制通信所触发的干扰；具体是射频耦合、供电纹波还是器件自身振动，尚未用示波器或声源测量区分，不能据此认定某个模拟器件损坏。

用户明确接受这部分近距离才能听到的轻微杂音，要求仅完成关闭 5V 升压的修复。因此未更改 Wi-Fi 省电、发射功率或心跳参数；新增的 Wi-Fi 参数诊断镜像仅构建，未烧录。所有临时诊断入口均从正式源码移除。

## 扬声器关闭状态

排查时扬声器任务未运行，PM1 GPIO3 为 GPIO 功能、推挽输出，输出锁存与输入读回均为低。项目已有 `M5.Speaker.end()`。

固定版本的 M5Unified 将 GPIO3 功能配置误写为清除 0x16 的 bit 3；M5PM1 数据手册中 GPIO3 功能实际对应 bits 7:6。此设备实测功能已为 GPIO，未发现这个错误正在导致本次杂音，正式修复没有修改依赖缓存。

## 安装与验证记录

2026-10-11 已完成正式 v0.4.8 构建、打包及 USB app1 烧录，写入 hash 验证通过。启动读回 `ext5v_out 0`，屏幕与 IMU 更新恢复；90 秒串口观察中末次心跳为 85 秒，麦克风 ready=1、采集队列 dropped=0、health=1。系统安装的 ThinCube 上位机恢复连接并显示 v0.4.8，原两项姿态校准仍有效；启动超过一分钟后仍在线。

正式固件的构建、镜像校验、烧录和启动验证结果见 `artifacts/speaker-noise-20261010/validation.json`；构建日志为 `release-build.log`，烧录日志为 `release-flash.log`，正常运行日志为 `release-device.log`。

烧录前备份了完整 8 MB Flash，保存为 `artifacts/speaker-noise-20261010/before/device-flash.bin`，SHA-256 为 `f8ea1b9bdc9fe780aad8de26c261b7d81ee30836fedb8df269f78dba4fbab237`。诊断只写入原活动应用分区 app1（0x210000），没有写入 NVS、分区表、OTA 选择记录或 app0。源码快照、原始 v0.4.7 镜像和现场日志一并保留。

本次修复的听感验收依据现场关闭升压后的反馈；最终固件保留相同配置。不另行宣称完成电池供电下的最终复测、长时间传音测试或残余杂音消除。

## 官方资料

- [StickS3 产品文档](https://docs.m5stack.com/zh_CN/core/StickS3)：管脚、EXT_5V 供电，以及首发批次开机可能轻微异响的说明。该说明不能证明本设备的具体声源或批次。
- [M5PM1 数据手册](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/M5PM1_Datasheet_EN.pdf)：PWR_CFG bit 3 与 GPIO 功能字段。
- [StickS3 原理图](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150_Stick_S3_PRJ_V0.6_20251111_2025_11_17_16_10_24.pdf)：功放 AW8737 的 VDD 接 VBUS_L0；L3B 断电不能直接视为切断功放电源。
