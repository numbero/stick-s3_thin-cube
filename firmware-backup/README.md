> 2026-10-11 清理说明：本目录当时的历史固件 `.bin` 和对应 `.sha256` 已删除。下方历史备份与恢复说明仅作记录，`restore.sh` 依赖的旧镜像已不存在。2026-10-16 新设备的原厂备份见下一节。
>
> 2026-10-18 清理说明：按用户要求清理项目内其余客户端固件、开发备份、诊断片段及 ELF/MAP 构建产物，仅保留下方新设备原厂镜像和同名前缀的校验、设备记录、分区表及日志。源码与源码压缩包、测试记录和 macOS 上位机保留。逐文件清理结果见 `../artifacts/firmware-inventory-20261018/cleanup-result.json`；项目外文件的处理状态以该记录为准。老设备最初固件尚未找回，后续单独调查。

# StickS3 原始固件备份

设备出厂固件的整片 Flash 备份，用于刷写自制固件后还原。

## 当前可用备份：2026-10-16 新出厂设备

通过 USB 从用户确认的全新出厂设备读取，未执行 Flash 擦除、写入或 eFuse 修改。完成后已通过 RTS 复位设备。

| 项 | 值 |
|---|---|
| 镜像 | `sticks3-factory-8MB-20261016-7ce8b1e2db38.bin` |
| 大小及范围 | 8,388,608 字节，从 `0x00000000` 到 `0x007fffff` |
| 芯片 | ESP32-S3-PICO-1 (LGA56) rev v0.2 |
| MAC | `7c:e8:b1:e2:db:38` |
| Flash | 8 MB XMC，Manufacturer `0x20` / Device `0x4017` |
| 安全状态 | Secure Boot 关闭，Flash Encryption 关闭 |
| 校验 | 两次独立整片读取逐字节一致；分区表解析及校验通过 |
| SHA-256 | `c9404bd91e0b2b0513bfe8b7dfe0a78b82b902033a2b3e426bee2149b539968e` |

同名前缀的 `.bin.sha256` 保存校验和，`.json` 保存设备及验证记录，`.partitions.csv` 保存分区表；`.read1.log`、`.read2.log` 和 `.security.log` 保存操作日志（读取日志已省略逐块进度行）。镜像包含 bootloader、分区表、原厂应用、NVS、`sys` 和 `vfs` 文件系统；不包含 eFuse 或 RAM。

后续清理必须保留此原厂镜像及上述同名前缀的全部配套文件。

在本目录验证文件：

```sh
shasum -a 256 -c sticks3-factory-8MB-20261016-7ce8b1e2db38.bin.sha256
```

将来需要恢复这台设备时，先验证校验和，再执行以下命令（会覆盖设备整片 Flash；本次备份没有执行）：

```sh
python3 -m esptool --chip esp32s3 --port /dev/cu.usbmodem1401 --baud 921600 \
  write-flash 0x0 sticks3-factory-8MB-20261016-7ce8b1e2db38.bin
```

上述命令适用于本次使用的 esptool v5.4.0，串口按实际连接调整。现有 `restore.sh` 仍指向已删除的 2026-09-23 历史镜像，不适用于这份新备份。`.bin` 按仓库规则仅保存在本机，不会被 Git 跟踪。

## 历史文件（已删除）

| 文件 | 说明 |
|---|---|
| `sticks3-original-8MB-20260923.bin` | 8MB 整片 Flash 镜像，偏移 0x0 |
| `sticks3-original-8MB-20260923.bin.sha256` | 校验和 |
| `restore.sh` | 写回脚本 |
| `pre-usb-v0.3-20261007-8MB.bin` | 本次 USB 升级之前的整片 8 MB 备份，含当时的 NVS |
| `pre-usb-v0.3-20261007-8MB.bin.sha256` | 本次升级前备份的 SHA-256 |

老设备 2026-09-23 最初镜像的已知 SHA-256 为 `f607fc650704f821caf6fec5b4a9651f542113ac0081458b62ba72b1ec8dfd3e`。截至 2026-10-18，已扫描可访问目录中未找到匹配镜像；该校验值保留供后续恢复核验使用。

## 设备信息（备份时记录）

| 项 | 值 |
|---|---|
| 芯片 | ESP32-S3-PICO-1 (LGA56) rev v0.2 |
| Flash | 8MB，XMC，Manufacturer 0x20 / Device 0x4017 |
| PSRAM | 8MB (AP_3v3) |
| MAC | 7c:e8:b1:e5:46:60 |
| 备份日期 | 2026-09-23 |

## 为什么是整片备份

镜像从 `0x0` 起整片读取，包含 bootloader、分区表、应用固件、以及 NVS。NVS 里可能存有出厂校准数据与配网信息，只备份应用分区的话还原不回去。所以还原时也是整片写回 `0x0`。

## 恢复

```sh
./restore.sh              # 默认 /dev/cu.usbmodem1401
./restore.sh /dev/cu.usbX # 指定串口
```

脚本会先要一次确认，然后整片写入并自动复位。

手动等价命令：

```sh
python3 -m esptool --port /dev/cu.usbmodem1401 --baud 921600 \
  write_flash 0x0 sticks3-original-8MB-20260923.bin
```

## 设备进入下载模式

长按机身侧边复位键约 2 秒，内部绿色 LED 闪烁时松开。ESP32-S3 的原生 USB 通常能自动复位进入下载模式，但若 esptool 连不上，就走这个手动步骤。

## 校验

```sh
shasum -a 256 -c sticks3-original-8MB-20260923.bin.sha256
```
