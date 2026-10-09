#!/usr/bin/env python3
"""Extend concept 05 with eight palettes while preserving its geometry."""
import base64
import html
import json
from pathlib import Path
import shutil
import subprocess
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
DESIGN = ROOT / "macos/Design/open-contour-v6"
SOURCE = ROOT / "macos/Design/ten-concepts-v5/05-open-contour.svg"
RSVG = shutil.which("rsvg-convert")
if not RSVG:
    raise SystemExit("rsvg-convert is required")

themes = json.loads((DESIGN / "themes.json").read_text())
assert [item["id"] for item in themes] == list("ABCDEFGH")
namespace = "http://www.w3.org/2000/svg"
ET.register_namespace("", namespace)

for theme in themes:
    tree = ET.parse(SOURCE)
    svg = tree.getroot()
    title, background, contour, screen, button = list(svg)
    title.text = f'ThinCube — Open Contour / {theme["name"]}'
    background.set("fill", theme["background"])
    contour.set("stroke", theme["contour"])
    screen.set("fill", theme["screen"])
    if theme["screenMode"] == "outline":
        # Inset the stroke so the visible screen bounds stay at x390/y253/244/378.
        screen.attrib.update(x="399", y="262", width="226", height="360", rx="13",
                             fill="none", stroke=theme["screen"], **{"stroke-width": "18"})
    button.set("fill", theme["button"])
    directory = DESIGN / "exports" / f'{theme["id"]}-{theme["slug"]}'
    iconset = directory / "AppIcon.iconset"
    iconset.mkdir(parents=True, exist_ok=True)
    source = directory / "AppIcon.svg"
    ET.indent(tree, space="  ")
    tree.write(source, encoding="unicode", xml_declaration=False)

    def render(size, output):
        subprocess.run([RSVG, "-w", str(size), "-h", str(size), "-o", str(output), str(source)], check=True)

    render(1024, directory / "AppIcon.png")
    subprocess.run([RSVG, "-f", "pdf", "-o", str(directory / "AppIcon.pdf"), str(source)], check=True)
    for points in [16, 32, 128, 256, 512]:
        render(points, iconset / f"icon_{points}x{points}.png")
        render(points * 2, iconset / f"icon_{points}x{points}@2x.png")
    theme["outputDir"] = str(directory.relative_to(DESIGN))
    theme["image"] = "data:image/svg+xml;base64," + base64.b64encode(source.read_bytes()).decode("ascii")

(DESIGN / "manifest.json").write_text(json.dumps(themes, ensure_ascii=False, indent=2))
cards = "\n".join(
    f'<button class="card" type="button" data-index="{index}" aria-label="查看 {theme["id"]} {html.escape(theme["name"])}">'
    f'<img src="{theme["image"]}" alt="{html.escape(theme["name"])}"><h2><span>{theme["id"]}</span>{html.escape(theme["name"])}</h2>'
    f'<p>{html.escape(theme["description"])}</p></button>'
    for index, theme in enumerate(themes)
)
data = json.dumps(themes, ensure_ascii=False).replace("</", "<\\/")
page = '''<!doctype html>
<html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ThinCube · 开放轮廓配色与主题</title>
<style>
*{box-sizing:border-box}body{margin:0;background:#f9fafb;color:#202932;font-family:-apple-system,BlinkMacSystemFont,"PingFang SC",sans-serif}main{max-width:1280px;margin:auto;padding:36px 32px 48px}header{padding:0 20px 24px}h1{margin:0;font-size:25px;letter-spacing:-.5px}header p{color:#7b848e;font-size:13px;margin:9px 0 0}.grid{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:28px 18px}.card{padding:0;border:0;background:none;text-align:left;font:inherit;color:inherit;cursor:pointer}.card>img{width:100%;display:block;transition:transform .16s}.card:hover>img{transform:translateY(-3px)}.card h2{display:flex;gap:12px;align-items:baseline;font-size:18px;margin:6px 8% 8px}.card h2 span{font-size:12px;color:#87909a}.card p{font-size:12px;line-height:1.7;color:#74808a;margin:0 8%}.card:focus-visible{outline:2px solid #0878ff;outline-offset:5px;border-radius:12px}
dialog{padding:24px;border:0;border-radius:22px;width:min(920px,calc(100vw - 32px));color:inherit;background:#f9fafb}dialog::backdrop{background:#111c30b3;backdrop-filter:blur(6px)}.top{display:flex;justify-content:space-between;align-items:center}.top h2{font-size:20px;margin:0}.actions{display:flex;gap:6px}.actions button{font:inherit;font-size:13px;border:0;border-radius:8px;padding:8px 12px;background:#e7ebf1;cursor:pointer}.detail{display:grid;grid-template-columns:minmax(0,1fr) 210px;gap:20px;align-items:center}.hero{width:100%;display:block}aside p{font-size:13px;line-height:1.8;color:#66717d}.sizes{display:flex;align-items:end;gap:24px;margin:24px 0}.sizes figure{margin:0;text-align:center}.sizes figcaption{font-size:11px;color:#8b939d;margin-top:6px}.swatches{display:flex;gap:8px}.swatch{width:23px;height:23px;border-radius:50%;border:1px solid #00000012}.palette{font-size:11px;line-height:1.8;color:#7b848f;font-variant-numeric:tabular-nums;margin-top:8px}a{color:#2564dd;text-decoration:none;font-size:13px}
@media(max-width:800px){main{padding:24px 14px}.grid{grid-template-columns:repeat(2,minmax(0,1fr));gap:24px 10px}.detail{grid-template-columns:1fr}.detail aside{padding:0 16px 16px}.hero{max-width:480px;margin:auto}.sizes{margin:16px 0}.card p{font-size:11px}}@media(prefers-reduced-motion:reduce){.card>img{transition:none}}
</style>
<main><header><h1>ThinCube · 开放轮廓</h1><p>05 方向延展 · 8 组配色与主题 · 点选放大</p></header><div class="grid">__CARDS__</div></main>
<dialog id="preview"><div class="top"><h2 id="name"></h2><div class="actions"><button id="previous" aria-label="上一个主题">←</button><button id="next" aria-label="下一个主题">→</button><button id="close">关闭</button></div></div><div class="detail"><img class="hero" id="hero" alt=""><aside><p id="description"></p><div class="swatches" id="swatches"></div><div class="palette" id="palette"></div><div class="sizes"><figure><img id="proof64" width="64" height="64" alt="64像素效果"><figcaption>64 px</figcaption></figure><figure><img id="proof32" width="32" height="32" alt="32像素效果"><figcaption>32 px</figcaption></figure></div><a id="download" download>下载 SVG</a></aside></div></dialog>
<script>
const themes=__DATA__;const modal=document.getElementById('preview');let current=0;
function show(index){current=(index+themes.length)%themes.length;const t=themes[current];document.getElementById('name').textContent=t.id+' · '+t.name;document.getElementById('description').textContent=t.description;for(const id of ['hero','proof64','proof32']){document.getElementById(id).src=t.image;}document.getElementById('hero').alt=t.name;const colors=[t.background,t.contour,t.screen,t.button];document.getElementById('swatches').replaceChildren(...colors.map(color=>{const dot=document.createElement('span');dot.className='swatch';dot.style.background=color;dot.title=color;return dot}));document.getElementById('palette').textContent=colors.join(' / ');const link=document.getElementById('download');link.href=t.image;link.download='ThinCube-'+t.id+'-'+t.slug+'.svg';if(!modal.open)modal.showModal();}
document.querySelectorAll('[data-index]').forEach(button=>button.addEventListener('click',()=>show(Number(button.dataset.index))));document.getElementById('close').addEventListener('click',()=>modal.close());document.getElementById('previous').addEventListener('click',()=>show(current-1));document.getElementById('next').addEventListener('click',()=>show(current+1));modal.addEventListener('click',event=>{if(event.target===modal)modal.close()});document.addEventListener('keydown',event=>{if(!modal.open)return;if(event.key==='ArrowLeft')show(current-1);if(event.key==='ArrowRight')show(current+1)});
</script></html>'''.replace("__CARDS__", cards).replace("__DATA__", data)
(DESIGN / "index.html").write_text(page)
print(f"Exported {len(themes)} open-contour themes")
