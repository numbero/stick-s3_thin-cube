# 本机上位机构建与安装

重新构建 macOS 上位机后，必须替换 `/Applications/ThinCube.app`，不能只更新 `artifacts/ThinCube.app`。本机联调、启动与系统授权固定使用 `/Applications/ThinCube.app`。

优先运行 `scripts/build-macos.sh`：构建和签名成功后自动安装；若 ThinCube 正在运行，会先退出，替换后从系统应用位置重启。若已单独完成构建，运行 `bash scripts/install-macos.sh` 完成安装。只有明确要求仅生成分发产物时才使用 `--no-install`。

# GitHub Actions CI 与版本发布

正式分发统一使用 GitHub Actions 构建、GitHub Releases 下载，复用现有工作流和打包脚本。详细操作见 `docs/github-actions-release.md`。

## 版本与主干

- `release.json` 是整套发行的版本清单；发行版本、固件版本和 Mac 版本独立，不要求数值相同。
- 修改固件版本时，同步正式环境 `firmware/platformio.ini` 的 `CLAWD_FW_VER` 与清单的 `firmware`；修改 Mac 版本时，同步 `macos/Info.plist` 的版本、build 与清单的 `macos`、`macos_build`。
- 每次正式发行使用新的 `release.json.version`，标签必须为对应的 `vX.Y.Z`。发布前运行 `python3 scripts/release.py validate`，并完成与变更相关的回归检查；本地完整回归入口为 `bash scripts/test-local.sh`。
- 待发布改动必须先提交并推送到 `main`，再为选定的主干提交创建、推送版本标签。云端只构建该提交，不包含本地未提交修改；不得混入其他进行中的工作。
- 版本标签固定对应一个提交，不得移动标签复用版本；已公开的 Release 不得覆盖附件，修复使用新发行版本。

## 工作流约定

- `.github/workflows/ci.yml`：PR、推送 `main` 或手动运行时，执行版本校验、回归检查及两端编译；不读取签名 Secrets，不发布 Release。
- `.github/workflows/build.yml`：复用固件与 Mac 构建。正式固件仅使用 `m5stack-sticks3` 环境；不得把 base 回滚或 diagnostic 镜像作为正式固件。Mac 测试必须设置 `THINCUBE_TEST_FIRMWARE`、`THINCUBE_TEST_VERSION`，实际校验同次云端构建的固件，不能跳过后声称兼容验证通过。
- `.github/workflows/release.yml`：版本标签推送时，额外检查标签提交属于 `main` 历史，构建、签名、汇总并自动发布。手动运行 Release 只生成供验收的签名下载包，不公开发布；中间 Actions artifacts 保留 7 天，长期下载入口为 Releases。
- 工具链与官方 actions 的固定版本、提交 SHA 以工作流为准；更新后必须验证 CI。Mac 显式选择 Xcode，不能依赖 runner 默认版本，否则可能缺少现有代码需要的 SDK。

## 签名与产物

- Mac 必须复用 ThinCube 固定的 Apple 签名身份及现有验证规则；缺少或失效时直接失败，不得降级为 ad hoc、自签名或擅自轮换证书。
- 签名材料只通过 `MACOS_CERTIFICATE_P12_BASE64`、`MACOS_CERTIFICATE_PASSWORD` 两个 Actions Secrets 提供；预期指纹使用 `MACOS_SIGNING_IDENTITY_SHA1` variable。CI 导入临时钥匙串并清理，私钥、密码不得写入源码、日志或下载附件。
- CI 仅生成分发产物，调用 `bash scripts/build-macos.sh --no-install`；本机重建、联调仍遵循上方 `/Applications/ThinCube.app` 安装要求。
- 当前 Mac 分发包为 Apple Silicon / macOS 13+，固定 Apple 身份签名但未公证；发布说明必须准确反映架构、系统要求和公证状态。
- 每个 Release 必须包含 OTA `.bin`、完整 USB 首刷 ZIP、Mac 应用 ZIP、组件构建清单、`firmware-manifest.json`、`release-manifest.json` 和 `SHA256SUMS.txt`。所有组件必须来自同一提交，并校验源码版本、固件内嵌版本、应用版本及文件哈希。
- OTA 文件保留 `thincube-<固件版本>.bin` 命名，不得改为不同的整套发行版本。USB 包必须包含 bootloader、分区表、`boot_app0.bin` 和应用镜像及正确偏移，不能作为 OTA 固件使用；不得发布设备 flash dump、NVS 或配对数据。

## 发布完成与失败恢复

所有构建和校验成功后，由 `scripts/publish-release.py` 创建或恢复同一提交的草稿，上传完整附件，从 GitHub 下载回读并校验，再公开 Release。草稿查找须保留认证列表查询、刷新缓存和有界重试，处理创建后的可见性延迟；不得因暂时查不到草稿而放宽提交、版本或签名检查。

失败时保持草稿，可重跑同一提交恢复；不得替换其他来源草稿或已公开版本。报告发布完成前，必须确认工作流结果、正式 Release 可访问、附件齐全且下载校验通过，不能仅凭编译成功或 Actions artifact 上传成功宣告完成。CI 不代替 USB 首刷、OTA、连接、语音及系统授权的真机验收。
