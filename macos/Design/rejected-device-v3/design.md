# ThinCube 图标 v3

本版依据 [M5Stack StickS3 官方实物主图](https://shop.m5stack.com/cdn/shop/files/1_f50b328d-f753-4bef-92ec-6f0e8a69e5c0_1200x1200.webp?v=1769049123) 重绘。产品资料：[StickS3 官方页面](https://shop.m5stack.com/products/m5sticks3-esp32s3-mini-iot-dev-kit)。

## 外观与风格

扁平化指图形表现方式。设备保留圆角长方体外壳、侧面厚度和底部轮廓，采用与实物主图接近的斜向视角。正面为上方竖向屏幕、下方横向细长蓝色胶囊按键；机身正面比例为 1:2，屏幕比例接近 135:240。

全部图形使用纯色矢量面。没有渐变、纹理、高光或投影。浅色屏幕、黑灰机身与蓝条构成主体，背景为浅冰蓝圆角底。

菜单栏采用圆角机身正视图，18pt 像素网格下将屏幕与横向条状按键镂空。小尺寸下作了光学校正，系统通过模板图像自行着色。

| 元素 | 颜色 |
| --- | --- |
| 图标底 | `#DCEBFC` |
| 正面机身 | `#353C46` |
| 侧面与厚度 | `#1E242D` |
| 底部接口 | `#0D1118` |
| 屏幕 | `#F8F7F1` |
| 条状按键 | `#1769FF` |

## 资源与构建

- `macos/Resources/AppIcon.svg`、`AppIcon.pdf`：矢量版。
- `macos/Resources/AppIcon.png`：1024×1024 透明 PNG。
- `macos/Resources/AppIcon.iconset/`：16、32、128、256、512pt，均含 2×，从矢量直接绘制。
- `macos/Resources/AppIcon.icns`：Finder、Dock 与应用切换器资源。
- `macos/Resources/MenuBarIconTemplate.svg`、`MenuBarIconTemplate.pdf`：18×18pt 模板图标。
- `macos/Resources/MenuBarIconTemplate.png`、`MenuBarIconTemplate@2x.png`：18px、36px 透明 PNG。
- `macos/Resources/MenuBarIcon-preview.png`：浅深色预览。
- `macos/Design/references/sticks3-official.webp`：实际检查过的官网主图，仅用于外观参考。

几何源文件为 `scripts/generate-icons.swift`。运行 `scripts/build-icons.sh` 导出资源与 ICNS；运行 `scripts/build-macos.sh` 将 ICNS/PDF 打包至应用并签名。受限执行环境的 ICNS 编码器可能需要沙箱外执行。

本次使用内置 `image_gen` 作构图探索，提示词保留在 `macos/Design/v3-imagegen-prompt.txt`。生成图未作为最终应用资源；最终交付是按真实外观重绘的原生纯色 SVG、PDF 与 PNG，无图像生成服务依赖。

旧方案分别保存在 `macos/Design/rejected-clawd-v1/` 与 `macos/Design/rejected-device-v2/`，不参与构建。
