# ThinCube 扁平图标

当前方案 v2 以 StickS3 本体为主题：瘦长的长方体机身、上方屏幕、下方蓝色方按键。ThinCube 名称对应设备的外观，不再以 Clawd 为应用标识。外观参考：[M5Stack StickS3 官方资料](https://docs.m5stack.com/zh_CN/core/StickS3)。

应用图标使用浅灰圆角底、石墨灰正面和窄侧面，用纯色几何面表达机身的薄长体积。无渐变、投影、高光、纹理、文字或角色图案。菜单栏使用同一设备的正面轮廓，屏幕与按键透明镂空，由 macOS 自动着色。

| 元素 | 颜色 |
| --- | --- |
| 圆角底 | `#EEF2F7` |
| 正面机身 | `#354154` |
| 窄侧面 | `#1B2433` |
| 顶面 | `#4B596B` |
| 屏幕 | `#101824` |
| 按键 | `#278BFF` |

## 文件

- `macos/Resources/AppIcon.svg`、`AppIcon.pdf`：可编辑矢量版本。
- `macos/Resources/AppIcon.png`：1024×1024 透明 PNG。
- `macos/Resources/AppIcon.iconset/`：16、32、128、256、512pt，均含 2×，直接从矢量绘制导出。
- `macos/Resources/AppIcon.icns`：Finder、Dock 和应用切换器图标。
- `macos/Resources/MenuBarIconTemplate.svg`、`MenuBarIconTemplate.pdf`：18×18pt 单色矢量菜单栏图标。
- `macos/Resources/MenuBarIconTemplate.png`、`MenuBarIconTemplate@2x.png`：18px、36px 透明 PNG。
- `macos/Resources/MenuBarIcon-preview.png`：浅色、深色底预览。

## 生成与接入

本版为原生 CoreGraphics 几何绘制，未调用图像生成服务，所有颜色均为纯色填充。几何源文件是 `scripts/generate-icons.swift`，并同步输出 SVG。

运行 `scripts/build-icons.sh` 重新导出全部资源和 ICNS；运行 `scripts/build-macos.sh` 打包图标至 `artifacts/ThinCube.app/Contents/Resources`，然后执行现有本地签名。受限执行环境中，`iconutil` 编码可能需要在沙箱外运行，普通终端构建不受影响。

旧的 Clawd 材质方案已弃用，其原始生成图片与提示词保留在 `macos/Design/rejected-clawd-v1/`，不参与构建。
