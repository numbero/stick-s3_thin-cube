# ThinCube · 十个应用图标方案

十个独立构图方向，共同保留竖向设备、屏幕与蓝色横条键。采用纯色几何和正面构图，无透视、渐变或阴影。

打开 `index.html` 可点击放大，查看 64px / 32px 效果；左右箭头切换方案。页面自包含，可离线打开。

| 编号 | 名称 | 设计重点 |
| --- | --- | --- |
| 01 | 蓝桥 | 紧凑完整机身，以蓝色横键作视觉锚点 |
| 02 | 回线 | 外壳与屏幕连成一条回旋线，形成品牌标记 |
| 03 | 近景 | 放大并偏置正面机身，利用边缘裁切形成构图 |
| 04 | 环域 | 圆形场域衬托瘦长设备，强调几何比例 |
| 05 | 开放轮廓 | 省去底边，以开放外框和留白保持轻盈 |
| 06 | 模块拼构 | 屏幕与按键拆分为两个模块 |
| 07 | C 型留白 | 以 C 型负形暗示 Cube，进一步抽象设备 |
| 08 | 反相 | 明暗对分，机身和屏幕跨分界反相 |
| 09 | 字形 | 将 T / C 的字形特征融入设备正面 |
| 10 | 双环 | 屏幕环与按键环连接，轮廓更柔和 |

每个 `exports/编号-名称/` 目录均含：

- `AppIcon.svg`：1024×1024 可编辑矢量源文件。
- `AppIcon.pdf`：矢量 PDF。
- `AppIcon.png`：1024×1024 PNG，外部透明留边。
- `AppIcon.iconset/`：16、32、128、256、512pt，均含 2×。
- `AppIcon.icns`：macOS 原生应用图标。

比较图中的小图为 64px 和 32px 实际显示尺寸。02、05、08 分别代表线条、留白与明暗构成，是建议优先比较的三个方向。

本轮仅提供设计提案，没有替换当前应用资源。已确认的菜单图标保持原样。

## 在项目中重新导出

在项目根目录运行：

```sh
python3 scripts/export-icon-concepts.py
swift -module-cache-path macos/.build/clang-cache scripts/render-icon-contact-sheet.swift macos/Design/ten-concepts-v5 artifacts
```

以上导出 SVG/PDF/PNG、iconset、离线画廊和比较图；ICNS 使用 macOS 的 `iconutil -c icns` 对各 iconset 单独转换。源文件均为原生 SVG，不依赖生成式位图或字体。

实物识别元素参考 [StickS3 官方产品图](https://shop.m5stack.com/products/m5sticks3-esp32s3-mini-iot-dev-kit)，另存于项目的 `macos/Design/references/sticks3-official.webp`。
