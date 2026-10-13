# StickS3 分段电量显示 v0.4.17

用户取消两轮实机标定，要求借鉴官方项目，实现后直接烧录；另要求显示百分比不带“估算”标记。没有使用未完成的充电记录拟合电池曲线。

继续通过固定版本 [M5Unified](https://github.com/m5stack/M5Unified/blob/8b63555e7bd9ac71978c55ad868fd1d1d3b2ef3e/src/utility/power/M5PM1_Class.inl) 的带成功状态接口读取 M5PM1 电压；百分比换算采用 [Zephyr 官方 LiPo 默认 OCV 表](https://github.com/zephyrproject-rtos/zephyr/blob/dc4e2c2f780ada5cfb0f3862395778842fbe9e63/include/zephyr/dt-bindings/battery/battery.h)，来源注释为 Analog Devices AN4189 Table 1。此表与前次调查的 Panasonic NCR18650BF 表不同。借鉴其分段映射方式，不迁移操作系统。

原始微伏值按最近整数转换为毫伏，0%、10%……100%的11点为：

`3306, 3687, 3741, 3775, 3793, 3821, 3884, 3945, 4008, 4086, 4177`

两个节点之间做线性插值，输出取整并限制0–100。例：3860mV 从原公式70%变成56%；4100mV 从100%变成92%，4177mV以上才到100%。这些是算法计算结果，不能视作本机精度证据。数据及来源声明保留在 `firmware/src/battery_curve_reference.h`；上游文件快照 SHA-256 为 `8d214ec544abc1fc88e97995b7003db639286bc1eb0cba880c7e85340a5eff4b`。

每秒读取、中值去除尖峰和电压 EMA 延续 v0.4.15。保留两个百分点显示滞回，取消额外“每5秒追赶1%”与低电突然跳变逻辑。参考表在充电、放电、Wi-Fi、BLE、音频和不同背光下持续使用，避免模式切换造成百分比模型跳变；插拔电源仍清空旧电压窗口并重新采样。读取失效或过期显示 `--%`，不会写成0%。持续低电压仍独立显示红色 LOW；硬件欠压保护不调整。OTA 同时取原公式、新曲线及原始/中值/滤波电压的保守值。

正常电量标签显示 `56%`，移除 `~`；Mac 帮助中同步删除该标记说明。日志仍记录 `model=lipo-ref-an4189`。实机标定启动入口在这一版禁用，模式不会自行恢复。

参考表是通用电芯的开路电压曲线，运行中的端电压还受负载、温度、充电与电池老化影响。没有无依据地增加电流补偿或推测容量；本次不测实际 mAh，不宣称本机百分比误差已经降低到某个范围。

已完成20个 C++ 测试程序、5项 Python 工具测试及 Swift75项检查，零失败；首次跳过的实际固件镜像识别在构建后指定 v0.4.17 补跑通过。新增参考曲线测试覆盖平台区/端点、全电压范围单调性、不同无线/音频/背光下模型连续性、低压响应、供电切换、读失效及 OTA 保守门限。

v0.4.17 构建成功，应用镜像1710848字节，SHA-256 `7e37317fd1ebaf83c10fe632430206213c43bf15a4cc74beed13ce7e15bfdd3b`。烧录前重新核对 USB 序列号 `7C:E8:B1:E5:46:60`、分区表及两个有效 OTA 记录（sequence 3、4），确认活动 app1 为0x210000、原镜像版本 v0.4.16，并备份该应用槽。只烧录 app1；esptool 写入哈希验证通过并执行 watchdog reset。没有改写 NVS、分区表或 OTA 记录。

Mac0.4.10（build31）已由 `scripts/build-macos.sh` 构建、签名、安装并从 `/Applications/ThinCube.app` 重启。设备正常运行确认：原配对自动恢复、BLE连接、`chip=6046e5b1e87c`、`model=lipo-ref-an4189`、标定 state=0、VBAT读取失败0、硬件低压配置仍2500mV。这里只确认安装与正常启动，没有做充放电、容量或百分比精度实测。

本机镜像、备份与检查记录在 `artifacts/battery-reference-v0.4.17/`。相关源文件：[参考表](../firmware/src/battery_curve_reference.h)、[曲线选择](../firmware/src/battery_curve_config.h)、[插值](../firmware/src/battery_curve.h)、[滤波与安全门限](../firmware/src/battery_gauge.h)。
