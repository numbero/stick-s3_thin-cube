# README 预览素材

这些图片随仓库保存，供 GitHub README 和项目介绍引用。PNG 用于直接展示；同名 SVG 是可编辑、自包含的排版源文件，包含标题、描述和按原图像素重建的屏幕路径，无需外部字体或图像下载。中文文字使用阅读环境的可用字体，PNG 已固定导出观感。

| 文件 | 内容与来源 |
|---|---|
| `device-overview.png` / `.svg` | 为 README 整理的功能概览。竖屏来自 `artifacts/clawd-stories-v0.4.13/device-frames/normal.png`，横屏来自 `artifacts/clawd-scroll-v0.4.35/initial-screen.png`；均为此前保存的设备画布。外壳参考已确认简洁配对稿的尺寸与材质表达。 |
| `pairing-preview.png` / `.svg` | 简洁配对页的竖屏蓝牙、横屏 Wi-Fi 预览。屏幕来自 `artifacts/pairing-simple-v0.4.23/` 下对应的 `*-layout.png`，按设备字体像素重建，布局已在 v0.4.23 采用。`012345` 为公开示例码。交互源稿见 [`demo/pairing-simple.html`](../../../demo/pairing-simple.html)。 |
| `ui-design-v2.png` | 原样保留此前的 `artifacts/design/thincube-ui-v2.png`，作为早期高保真视觉方案。图中对讲模式 B:MUTE、部分状态栏与横屏提示为历史文案，不代表当前行为。 |

外壳均为设计示意，非实物照片；固件画布与配对像素重建也分别标明来源。概览仅复用历史画面，不宣称是当前固件的逐像素截图。现行操作以[根目录 README](../../../README.md#设备怎么用)及源码为准。

`artifacts/` 本身不纳入 Git；展示所需内容已复制或内嵌到本目录，远端阅读不依赖本机构建产物。修改 SVG 后可用支持 SVG 的图形工具重新导出同尺寸 PNG。
