# ThinCube 应用图标：主视图 v4

三版采用同一套纯正面几何：圆角机身、竖屏和横向蓝色条键。主体、屏幕和按键沿垂直中轴对齐，统一圆角节奏和留白。纯色矢量绘制。

- **A-negative · 深色负形**：深墨色底与浅色机身形成负形屏幕，蓝条作为唯一高饱和点。
- **B-cobalt · 蓝底色块**：蓝色底、深色机身、浅色屏幕，蓝条使用较亮的蓝色区分。
- **C-outline · 浅色线框**：浅灰底与细轮廓，屏幕为浅蓝色面，蓝条保留强调。

每个目录都含可编辑 SVG、矢量 PDF、1024px PNG、十档 iconset、ICNS 与同一张比较图。比较图还显示 64px 和 32px 缩略效果。

重新导出某版：

```sh
swift -module-cache-path macos/.build/clang-cache scripts/generate-icons.swift macos/Design/front-view-v4/B-cobalt --app-only --style cobalt
iconutil -c icns macos/Design/front-view-v4/B-cobalt/AppIcon.iconset -o macos/Design/front-view-v4/B-cobalt/AppIcon.icns
```

`--app-only` 保留已确认的菜单栏资源，不重写其 PDF、PNG 或 SVG。
