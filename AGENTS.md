# 本机上位机构建与安装

重新构建 macOS 上位机后，必须替换 `/Applications/ThinCube.app`，不能只更新 `artifacts/ThinCube.app`。本机联调、启动与系统授权固定使用 `/Applications/ThinCube.app`。

优先运行 `scripts/build-macos.sh`：构建和签名成功后自动安装；若 ThinCube 正在运行，会先退出，替换后从系统应用位置重启。若已单独完成构建，运行 `bash scripts/install-macos.sh` 完成安装。只有明确要求仅生成分发产物时才使用 `--no-install`。
