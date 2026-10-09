# ThinCube 应用图标

## 当前应用图标：A「瓷白」（v6）

用户已选定 v5 的 05「开放轮廓」作为整体设计方向。基于原轮廓与几何比例，新增 A–H 共 8 组：瓷白、石墨、冰川、电蓝、苔雾、砂岩、蓝图、夜航。前六组采用实色屏幕，后两组为空心屏幕；所有版本保留蓝色横条键。

当前已选用 **A「瓷白」**：暖白底 `#EFEDE7`、炭黑轮廓与屏幕 `#252B32`、蓝色条键 `#0878FF`。选定资源来自 `macos/Design/open-contour-v6/exports/A-porcelain/`，已原样应用到 `macos/Resources/` 和打包应用。菜单图标保持原样。

对比图位于 `artifacts/ThinCube-v6-A-D.png`、`artifacts/ThinCube-v6-E-H.png` 和 `artifacts/ThinCube-v6-themes.png`。其余配色、色板和离线画廊保存在 `macos/Design/open-contour-v6/`。

## 历史方向探索：十个新方案（v5）

新一轮提供十个结构不同的正面方向：蓝桥、回线、近景、环域、开放轮廓、模块拼构、C 型留白、反相、字形、双环。比较图位于 `artifacts/ThinCube-v5-01-05.png` 和 `artifacts/ThinCube-v5-06-10.png`，离线放大画廊与说明位于 `macos/Design/ten-concepts-v5/`。各方案已导出 SVG、PDF、PNG、iconset、ICNS。

其中 05「开放轮廓」已选定，并在 v6 中扩展配色；其余方案作为历史探索保留。

## 历史配色探索：主视图 v4

v4 曾采用 **A · 深色负形**。正面构图将圆角机身、竖向屏幕和蓝色条状按键提炼为一个图形标识；屏幕与深墨色背景同色，形成负形。三种元素沿中轴对齐，使用统一的圆角节奏和留白。

所有图形均为原生纯色矢量。应用图标仅包含正面，无侧面、透视、渐变、阴影、纹理、文字或装饰图案。

## 三个方向

对比图：`artifacts/ThinCube-app-icons-v4-preview.png`，包含大图与 64px、32px 效果。

| 方案 | 目录 | 表现 |
| --- | --- | --- |
| A · 深色负形（历史默认） | `macos/Design/front-view-v4/A-negative/` | 深墨色底、浅色机身、负形屏幕和蓝条 |
| B · 蓝底色块 | `macos/Design/front-view-v4/B-cobalt/` | 蓝色底、深色机身、浅色屏幕和亮蓝条 |
| C · 浅色线框 | `macos/Design/front-view-v4/C-outline/` | 浅灰底、细轮廓、浅蓝屏幕和蓝条 |

每版均含 SVG、PDF、1024px PNG、十档 iconset 与 ICNS。完整可用资源包：`artifacts/ThinCube-app-icons-v4.zip`。

## 已确认的菜单栏图标

菜单栏继续使用 v3 圆角机身正视图，屏幕和横向条状按键镂空。本轮只更新应用图标，原菜单 PDF、SVG、18px/36px PNG 与浅深色预览均保留，文件 SHA-256 与 v3 一致。

## 当前资源与构建

- `macos/Resources/AppIcon.svg`、`AppIcon.pdf`：当前 v6 A「瓷白」矢量资源。
- `macos/Resources/AppIcon.png`：1024×1024 透明 PNG。
- `macos/Resources/AppIcon.iconset/`：16、32、128、256、512pt，均含 2×，直接从矢量绘制。
- `macos/Resources/AppIcon.icns`：Finder、Dock 与应用切换器。
- `macos/Resources/MenuBarIconTemplate.*`：保留的 v3 菜单资源。

原始几何源文件为 `macos/Design/ten-concepts-v5/05-open-contour.svg`，色板位于 `macos/Design/open-contour-v6/themes.json`。运行 `scripts/export-open-contour-themes.py` 可重新导出主题 SVG/PDF/PNG 与 iconset，再使用 macOS 的 `iconutil` 编码 ICNS。`scripts/generate-icons.swift` 仅保留用于复现历史 v4，不参与当前图标构建。

运行 `scripts/build-icons.sh` 将已确认的 A「瓷白」导出文件原样复制到资源目录，不调用旧生成器或修改菜单资源；运行 `scripts/build-macos.sh` 将 ICNS/PDF 打包至应用并签名，然后自动替换 `/Applications/ThinCube.app`。若应用原先正在运行，会从该固定位置重启。仅生成分发产物可传入 `--no-install`。

本版没有调用图像生成服务。StickS3 识别元素来自此前核对的 [官方实物图](https://shop.m5stack.com/products/m5sticks3-esp32s3-mini-iot-dev-kit)，参考图保存在 `macos/Design/references/sticks3-official.webp`。旧方案存于 `macos/Design/rejected-*/`，不参与构建。
