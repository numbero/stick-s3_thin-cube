#!/usr/bin/env python3
"""Render the ten editable SVG concepts without changing installed app assets."""
import base64
import html
import json
from pathlib import Path
import shutil
import subprocess
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
DESIGN = ROOT / "macos/Design/ten-concepts-v5"
RSVG = shutil.which("rsvg-convert")
if not RSVG:
    raise SystemExit("rsvg-convert is required to export SVG concepts")

concepts = []
for group in ["01-04.json", "05-07.json", "08-10.json"]:
    concepts.extend(json.loads((DESIGN / group).read_text()))
concepts.sort(key=lambda item: int(item["id"]))
assert [int(item["id"]) for item in concepts] == list(range(1, 11)), "Expected ten unique concepts"

for concept in concepts:
    source = DESIGN / concept["svgFile"]
    xml = ET.parse(source).getroot()
    assert xml.attrib.get("viewBox") == "0 0 1024 1024", source
    assert "<text" not in source.read_text(), source
    directory = DESIGN / "exports" / source.stem
    iconset = directory / "AppIcon.iconset"
    iconset.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, directory / "AppIcon.svg")

    def render(size, target):
        subprocess.run([RSVG, "-w", str(size), "-h", str(size), "-o", str(target), str(source)], check=True)

    render(1024, directory / "AppIcon.png")
    subprocess.run([RSVG, "-f", "pdf", "-o", str(directory / "AppIcon.pdf"), str(source)], check=True)
    for points in [16, 32, 128, 256, 512]:
        render(points, iconset / f"icon_{points}x{points}.png")
        render(points * 2, iconset / f"icon_{points}x{points}@2x.png")
    concept["outputDir"] = str(directory.relative_to(DESIGN))
    concept["image"] = "data:image/svg+xml;base64," + base64.b64encode(source.read_bytes()).decode("ascii")

(DESIGN / "manifest.json").write_text(json.dumps(concepts, ensure_ascii=False, indent=2))

cards = "\n".join(
    f'<button class="concept" type="button" data-index="{index}" aria-label="查看 {html.escape(item["id"])} {html.escape(item["name"])}">'
    f'<img src="{item["image"]}" alt="{html.escape(item["name"])}"><span><b>{item["id"]}</b>{html.escape(item["name"])}</span></button>'
    for index, item in enumerate(concepts)
)
data = json.dumps(concepts, ensure_ascii=False).replace("</", "<\\/")
page = '''<!doctype html>
<html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ThinCube · 十个应用图标方向</title>
<style>
*{box-sizing:border-box}body{margin:0;background:#f8f9fb;color:#18202b;font-family:-apple-system,BlinkMacSystemFont,"PingFang SC",sans-serif}
main{max-width:1320px;margin:auto;padding:38px 40px 54px}header{display:flex;align-items:baseline;gap:20px;padding:0 18px 28px}h1{font-size:24px;letter-spacing:-.5px;margin:0;font-weight:650}header p{margin:0;font-size:13px;color:#747c87}
.grid{display:grid;grid-template-columns:repeat(5,minmax(0,1fr));gap:30px 18px}.concept{border:0;background:none;color:inherit;font:inherit;padding:0;cursor:pointer;text-align:left}.concept img{width:100%;display:block;transition:transform .18s}.concept:hover img{transform:translateY(-4px)}.concept span{display:flex;align-items:center;gap:12px;margin:4px 8% 0;font-size:15px;font-weight:500}.concept b{font-size:12px;letter-spacing:.8px;color:#8a919c;font-variant-numeric:tabular-nums;font-weight:500}.concept:focus-visible{outline:2px solid #3478f6;outline-offset:4px;border-radius:16px}
dialog{width:min(880px,calc(100vw - 32px));border:0;border-radius:22px;padding:24px;background:#f8f9fb;color:#18202b}dialog::backdrop{background:#0c1429a8;backdrop-filter:blur(6px)}.dialog-top{display:flex;align-items:center;justify-content:space-between;gap:12px}.dialog-top h2{font-size:19px;margin:0}.actions{display:flex;gap:6px}.actions button{border:0;background:#e8edf3;padding:9px 12px;border-radius:9px;font:inherit;font-size:13px;cursor:pointer}.focus{display:grid;grid-template-columns:minmax(0,1fr) 190px;gap:20px;align-items:center}.hero{width:100%;display:block}.proofs p{font-size:13px;line-height:1.7;color:#667180}.proofs .sizes{display:flex;align-items:end;gap:20px}.proofs figure{margin:0;text-align:center}.proofs figcaption{font-size:11px;color:#8a919c;margin-top:8px}.proofs a{display:inline-block;margin-top:16px;font-size:13px;color:#2563eb;text-decoration:none}
@media(max-width:1000px){.grid{grid-template-columns:repeat(4,minmax(0,1fr))}}@media(max-width:760px){main{padding:24px 18px}.grid{grid-template-columns:repeat(3,minmax(0,1fr));gap:24px 10px}header{padding:0 10px 20px;display:block}header p{margin-top:8px}.focus{grid-template-columns:1fr}.proofs{padding:0 24px 12px}.proofs .sizes{justify-content:center}}@media(max-width:480px){.grid{grid-template-columns:repeat(2,minmax(0,1fr))}.concept span{font-size:14px}dialog{padding:16px}.dialog-top h2{font-size:16px}.actions button{padding:8px}}
@media(prefers-reduced-motion:reduce){.concept img{transition:none}}
</style>
<main><header><h1>ThinCube</h1><p>应用图标 · 10 个正面方向 · 点选放大</p></header><div class="grid">__CARDS__</div></main>
<dialog id="preview"><div class="dialog-top"><h2 id="name"></h2><div class="actions"><button type="button" id="previous" aria-label="上一个方案">←</button><button type="button" id="next" aria-label="下一个方案">→</button><button type="button" id="close">关闭</button></div></div><div class="focus"><img class="hero" id="hero" alt=""><aside class="proofs"><p id="description"></p><div class="sizes"><figure><img id="proof64" width="64" height="64" alt="64像素效果"><figcaption>64 px</figcaption></figure><figure><img id="proof32" width="32" height="32" alt="32像素效果"><figcaption>32 px</figcaption></figure></div><a id="download" href="" download>下载矢量源文件</a></aside></div></dialog>
<script>
const concepts=__DATA__;const modal=document.getElementById('preview');let current=0;
function show(index){current=(index+concepts.length)%concepts.length;const c=concepts[current];document.getElementById('name').textContent=c.id+' · '+c.name;document.getElementById('description').textContent=c.description;for(const id of ['hero','proof64','proof32']){const img=document.getElementById(id);img.src=c.image;if(id==='hero')img.alt=c.name;}const link=document.getElementById('download');link.href=c.image;link.download='ThinCube-'+c.id+'-'+c.slug+'.svg';if(!modal.open)modal.showModal();}
document.querySelectorAll('[data-index]').forEach(button=>button.addEventListener('click',()=>show(Number(button.dataset.index))));document.getElementById('close').addEventListener('click',()=>modal.close());document.getElementById('previous').addEventListener('click',()=>show(current-1));document.getElementById('next').addEventListener('click',()=>show(current+1));modal.addEventListener('click',event=>{if(event.target===modal)modal.close()});document.addEventListener('keydown',event=>{if(!modal.open)return;if(event.key==='ArrowLeft')show(current-1);if(event.key==='ArrowRight')show(current+1)});
</script></html>'''.replace("__CARDS__", cards).replace("__DATA__", data)
(DESIGN / "index.html").write_text(page)
print(f"Exported {len(concepts)} concepts with standalone gallery, SVG/PDF/PNG and full iconsets")
