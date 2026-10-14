# 云端固件研发

当前云端机器为 Linux ARM64，hostname 为 `instance-20261001-1103`。

## 数据盘布局

所有 Git 仓库放在 `/mnt/data/dev/<仓库名>`，当前仓库为 `/mnt/data/dev/stick-s3_thin-cube`。

共用工具与下载缓存放在 `/mnt/data/dev-tools`，与 Git 仓库分开：

- `platformio-venv`：PlatformIO 6.1.19 的 Python 虚拟环境。
- `platformio-core`：默认 `PLATFORMIO_CORE_DIR`，保存固定平台、交叉编译器、Arduino 框架及下载缓存。
- `pip-cache`：Python 下载缓存。
- `bin/pio`：包装命令，默认使用数据盘缓存；`/usr/local/bin/pio` 和 `platformio` 指向该命令。显式设置的 `PLATFORMIO_CORE_DIR` / `PIP_CACHE_DIR` 优先。

本机通过 Ubuntu `build-essential`、`python3-venv`、`python3-pip` 提供原生 C++ 编译器与 Python 环境。固件构建使用仓库固定的 Espressif 平台和 M5Stack 依赖，不使用原生 ARM 编译器生成 ESP32 固件。

## 日常命令

```sh
cd /mnt/data/dev/stick-s3_thin-cube
git pull --ff-only
bash scripts/test-local.sh
pio run -d firmware
python3 scripts/package-firmware.py
```

固件和发布清单输出到 `artifacts/`，构建缓存位于 `firmware/.pio/`，均被 Git 忽略且实际位于数据盘。

GitHub SSH 配置已选择 `/home/ubuntu/.ssh/stick-s3_github`，公钥备注为本机 hostname，其他 GitHub 项目可复用同一认证。SSH 私钥不属于源码或构建缓存。

## 验证范围

Linux 上可运行全部固件 C++ / Python 测试并构建、打包固件。Swift 上位机使用 macOS 系统框架，需在 Mac 上验证、构建和安装；仓库 `AGENTS.md` 的系统应用安装要求继续适用。

当前云端会话未连接用户的 StickS3，也未建立用户局域网的 OTA 通道。生成固件后，需要在能连接设备的机器进行 USB / Wi-Fi OTA 更新与真机验收。
