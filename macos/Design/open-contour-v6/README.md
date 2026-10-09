# ThinCube · 开放轮廓配色与主题

以用户选定的 v5 方案 05「开放轮廓」为基础，扩展 8 组配色与主题。外框路径、屏幕范围、条键形状、机身比例、应用图标底板均沿用原版；蓝色横条键始终保留。

| 编号 | 主题 | 屏幕 | 特征 |
| --- | --- | --- | --- |
| A | 瓷白 | 实色 | 原版暖白与炭黑，最接近已选定的 05 |
| B | 石墨 | 实色 | 深灰与柔白，适合作为深色搭配 |
| C | 冰川 | 实色 | 冷灰蓝、深蓝外框与低饱和屏幕，视觉更轻 |
| D | 电蓝 | 实色 | 高饱和品牌蓝、柔白图形、浅蓝条键 |
| E | 苔雾 | 实色 | 灰绿与深苔色，安静的自然色调 |
| F | 砂岩 | 实色 | 暖砂与棕灰，柔和的工业色调 |
| G | 蓝图 | 空心 | 浅色线稿，进一步增加屏幕内的留白 |
| H | 夜航 | 空心 | 午夜蓝、冰蓝线条，轻盈的深色主题 |

建议优先比较 C「冰川」与 B「石墨」。A 保留原版参照，G/H 展示屏幕处理方式的另一种选择。所有方案保持纯色、正面、无透视、无渐变、无阴影。

`index.html` 是可离线打开的比较画廊，点击主题可放大、查看配色值与 64px/32px 效果。`themes.json` 保存可复用的色板与屏幕模式。

每个 `exports/编号-名称/` 目录含 `AppIcon.svg`、`AppIcon.pdf`、1024px `AppIcon.png`、完整 `AppIcon.iconset/` 与原生 `AppIcon.icns`。iconset 包含 16、32、128、256、512pt 的 1× 和 2× 版本。

对比图底部显示 64px / 32px 实际尺寸和四个色板圆点（背景、外框、屏幕、条键）。

用户已选择 **A「瓷白」** 作为当前应用图标。A 的完整资源已应用到 `macos/Resources/` 和打包应用，其他主题保留为备选；已确认的菜单图标保持原样。

## 项目中重新导出

```sh
python3 scripts/export-open-contour-themes.py
swift -module-cache-path macos/.build/clang-cache scripts/render-open-contour-themes.swift macos/Design/open-contour-v6 artifacts
```

上述步骤导出 SVG/PDF/PNG、iconset、画廊和对比图。ICNS 通过 macOS 的 `iconutil -c icns` 对各 iconset 转换。生成器读取原版 05 的 SVG，仅修改色值；G/H 的屏幕线条向内收缩，以保持屏幕外边界不变。

`scripts/build-icons.sh` 将已确认的 A 导出文件原样复制到应用资源目录，不重新生成菜单资源。运行 `scripts/build-macos.sh` 打包、签名，并自动替换 `/Applications/ThinCube.app`；若应用原先正在运行，会从该位置重启。仅生成分发产物可传入 `--no-install`。
