# StickS3 原始固件备份

设备出厂固件的整片 Flash 备份，用于刷写自制固件后还原。

## 文件

| 文件 | 说明 |
|---|---|
| `sticks3-original-8MB-20260923.bin` | 8MB 整片 Flash 镜像，偏移 0x0 |
| `sticks3-original-8MB-20260923.bin.sha256` | 校验和 |
| `restore.sh` | 写回脚本 |
| `pre-usb-v0.3-20261007-8MB.bin` | 本次 USB 升级之前的整片 8 MB 备份，含当时的 NVS |
| `pre-usb-v0.3-20261007-8MB.bin.sha256` | 本次升级前备份的 SHA-256 |

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
