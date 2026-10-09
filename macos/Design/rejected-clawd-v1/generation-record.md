# ThinCube 图标

应用图标使用深炭色圆角底、陶土色 Clawd 和三枚琥珀色悬浮方块。菜单栏使用同一角色的单色剪影，双眼镂空，18pt 时对齐像素网格；系统通过模板图像自动适配浅色、深色和选中状态。

Clawd 保留直角身体、两侧触角、四条腿和两颗方眼，无嘴、腮红或手臂。菜单图标对原 66×52 比例作了小尺寸光学校正。

## 文件与构建

- `macos/Design/AppIcon-master.png`：内置 image_gen 生成的原始透明 PNG。
- `macos/Resources/AppIcon.png`：1024×1024 透明 PNG。
- `macos/Resources/AppIcon.iconset/`：16、32、128、256、512pt，均含 2×。
- `macos/Resources/AppIcon.icns`：Finder、Dock、应用切换器使用的应用图标。
- `macos/Resources/MenuBarIconTemplate.pdf`：18×18pt 矢量菜单栏图标。
- `macos/Resources/MenuBarIconTemplate.svg`：可编辑矢量源文件。
- `macos/Resources/MenuBarIconTemplate.png`、`MenuBarIconTemplate@2x.png`：18px、36px 透明 PNG。

运行 `scripts/build-icons.sh` 可从原始应用图与菜单几何重新导出资源。运行 `scripts/build-macos.sh` 将 ICNS 和 PDF 打包进 `artifacts/ThinCube.app` 并执行现有本地签名。无需生成服务或 API 密钥即可重建已经保存的资源。

## 生成记录

应用图标通过内置 `image_gen` 工具生成，非 CLI。参考图为 `artifacts/landscape-device-v0.3.8.png`，仅参考其中的角色身份和配色。之后仅进行标准图标尺寸导出与格式转换。菜单栏图标由原生 CoreGraphics 矢量几何绘制，SVG、PDF 与 PNG 共用同一像素网格。

应用图标的最终提示词：

```text
Use case: logo-brand
Asset type: one finished macOS application icon for ThinCube, square 1024x1024 PNG with transparent exterior, ready for iconset export.
Primary request: create a refined macOS app icon featuring the terracotta pixel creature Clawd from the reference image. Reference image role: character identity and colors only; ignore its terminal UI, screen text and detector.
Subject: a single front-facing geometric pixel creature, preserving the reference silhouette. Exact underlying proportions in a 66 by 52 unit space: main rectangular body x6 y0 width54 height39; left antenna x0 y13 width6 height13; right antenna x60 y13 width6 height13; four short feet at x6, x18, x42, x54, y39, width6 height13; two small square eye cutouts at x12 y13 and x48 y13, size6 by6.5. Entire creature is terracotta #D97757, eyes very dark #140E0A. All character edges straight, axis aligned and sharply rectangular. No mouth, no cheeks or blush, no arms or hands.
Scene/backdrop: one deep charcoal macOS rounded-square tile, approximately 840x840 centered in the 1024 canvas, generously rounded corners, exterior truly transparent. Elegant subtly lit charcoal surface, quiet warm edge highlight and restrained soft shadow inside the transparent bounds.
Composition/framing: centered large upright pixel character occupying about 64 percent of tile width, with calm generous padding. Keep the four feet separated and visible. A tiny restrained cluster of three amber #F8B000 square particles above and to the right of the creature, separated from its body, recalls its telekinesis. Creature remains the unmistakable central mark.
Style/medium: premium restrained macOS icon, crisp vector-like character over a softly dimensional dark tile, terracotta warm light, subtle material polish, excellent readability at 32px. Pixel geometry stays flat and exact, not a plush toy and not voxel extrusion.
Text: none.
Constraints: output a SINGLE isolated application icon only; no presentation sheet, no labels, no mockup, no surrounding desktop, no grid, no lettering, no watermark, no microphone/Wi-Fi badges. Preserve the unusual wide eye spacing and four-foot character. Transparent canvas outside the rounded-square tile.
```

