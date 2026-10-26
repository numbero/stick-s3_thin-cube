# 自动构建与版本下载

首版发布 StickS3 正式固件和 Apple Silicon Mac 上位机。两端保留独立版本；`release.json` 将组件版本关联到一次发布。

## 工作流

- **CI**：PR、推送 `main`、手动运行时，验证版本，运行 C++ / Python / 手机页面测试，编译固件，再运行 Mac Swift 测试并编译上位机。不读取签名 Secrets，不发布 Release。
- **Release**：推送 `vX.Y.Z` 标签时验证版本与主干来源，构建固件、签名并打包上位机，汇总校验后自动发布。
- **Release 手动运行**：执行相同的签名构建并生成 `release-downloads` Actions artifact，保留 7 天，**不会创建或发布 Release**。用于首次验收签名配置。

固件在 `ubuntu-24.04` 构建；Mac 在 `macos-15` arm64 构建，目标 macOS 13。GitHub 官方 actions 均固定提交 SHA。PlatformIO Core 为 `6.1.19`，Python 为 `3.12`，Node 为 `22`，固件平台和库沿用 `platformio.ini` 固定值。构建清单记录实际 PlatformIO / Swift / Xcode 版本。

## 首次签名配置

沿用本机现有固定 Apple 签名身份；首版不提供 Apple 公证。需要一次性将**该证书和对应私钥**的加密 `.p12` 导出配置到 GitHub。不要重新生成证书，也不要导出整个钥匙串。

1. 在“钥匙串访问 → 我的证书”中，根据 `~/Library/Application Support/ThinCube/Signing/identity.sha1` 找到 ThinCube 使用的证书，确认其下有对应私钥。
2. 仅选中此身份，导出为加密 `.p12`，设置非空密码，文件留在源码目录之外。
3. 确认 `gh auth status` 已登录可管理本仓库的账户，然后运行：

   ```sh
   python3 scripts/setup-github-signing.py --p12 /absolute/path/ThinCube-signing.p12
   ```

脚本从终端隐式读取导出密码，校验证书与本机固定指纹一致，然后配置以下内容；不会导出钥匙串或将私钥写入源码：

| 类型 | 名称 | 内容 |
| --- | --- | --- |
| Actions Secret | `MACOS_CERTIFICATE_P12_BASE64` | 所选加密 `.p12` 的 Base64 |
| Actions Secret | `MACOS_CERTIFICATE_PASSWORD` | `.p12` 导出密码 |
| Actions variable | `MACOS_SIGNING_IDENTITY_SHA1` | 固定证书的 40 位 SHA-1，公开身份值 |

也可在 GitHub 仓库 Settings → Secrets and variables → Actions 中手动设置。Base64 不是加密，不要贴到 issue、日志或提交中。配置脚本只更新上述两个 Secrets 和一个 variable。

工作流在临时钥匙串导入该身份，并继续调用现有 `sign-macos.sh` 验证；缺少身份直接失败。临时导出文件、钥匙串和 pin 目录会清理，不会随 artifact 上传。

## 首次验证与正式发布

工作流合入主干后：

```sh
# 无签名的完整回归和编译
gh workflow run CI --ref main

# 已配置证书后，先生成签名成品用于下载验收，不发布
gh workflow run Release --ref main
```

在 Actions 下载 `release-downloads` 并解压，核对 `SHA256SUMS.txt`。Mac ZIP 解压后，将应用安装到 `/Applications/ThinCube.app`，按系统提示授权；BlackHole 2ch 仍需独立安装。本地源码构建继续使用 `bash scripts/build-macos.sh`，默认签名后替换系统应用。

首次验收应检查：Mac 启动、保留配对、钥匙串访问、辅助功能、语音与设备 OTA。USB ZIP 另做一次首刷验收，不能将其当作 OTA 应用固件。本流程不自动连接或烧录任何设备。

正式发布时，先更新 `release.json.version`，核对组件版本与源码一致，提交到主干，再打标签。例如当前首版清单为 `v0.4.79` / Mac `0.4.39` build `60`：

```sh
git push origin main
git tag -a v0.4.79 -m "ThinCube v0.4.79"
git push origin refs/tags/v0.4.79
```

后续版本使用新的标签。若只改 Mac，可以保留固件版本，同时增加发布版本、Mac 版本和 build；不要为匹配整套 Release 而给 OTA `.bin` 改名。标签必须等于 `v` + `release.json.version`，对应提交必须在 `main` 历史内。

## 下载内容

| 文件 | 用途 |
| --- | --- |
| `thincube-<固件版本>.bin` | 已有兼容 A/B 分区的设备，手机热点 / Mac Wi-Fi OTA |
| `firmware-manifest.json` | OTA 设备、协议、内嵌版本、大小及 SHA-256 |
| `thincube-usb-<固件版本>.zip` | 首次 USB 安装 / 恢复，含四个镜像、偏移、校验及独立 `flash.py` |
| `ThinCube-<Mac版本>-macos-arm64.zip` | 固定 Apple 身份签名的 Mac 应用；未公证 |
| `firmware-build.json` / `macos-build.json` | 组件构建提交、版本、工具链与文件哈希 |
| `release-manifest.json` / `SHA256SUMS.txt` | 整套发布关联与最终下载校验 |

USB 包通过 PlatformIO metadata 读取同次构建的 bootloader、partition table、`boot_app0.bin` 和应用镜像，不需要下载源码或编译器；烧录工具为 `esptool==4.11.0`，使用方式见包内 README。独立镜像避免 raw 合并文件填充间隙覆盖 NVS，但恢复操作会重置 OTA 选择，常规更新优先使用 OTA。

## 失败与重跑

只有固件、Mac、真实固件兼容校验和最终哈希全部通过，才开始创建草稿。所有附件上传后会从 GitHub 下载并再次校验，成功后才公开。中途失败可重跑同一提交，恢复本流程创建的草稿；其他来源草稿和已公开版本均拒绝覆盖。同一标签不能用于另一个提交。

如果公开版本需要修复，增加发布版本并推新标签。旧固件仍受设备的降级限制；Release 保留旧文件不表示设备 OTA 允许降级。

公开仓库的标准 GitHub-hosted runner 运行时间免费。签名证书续期或迁移 Developer ID 时，应同步处理固定指纹、DR 和系统授权连续性。未来正式面向他人分发可再加入 Hardened Runtime、secure timestamp、公证与装订；当前版本没有宣称完成这些步骤。
