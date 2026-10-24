#pragma once

// Served directly from flash. No Internet, secure-context APIs, or CDN required.
static const char phoneUpdatePage[] = R"THINCUBE(<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="color-scheme" content="dark">
<title>ThinCube · 手机升级</title>
<style>
:root{color-scheme:dark;font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;background:#131313;color:#eee9df;--dim:#b8b3a9;--amber:#ffbc67;--line:#3b352c}*{box-sizing:border-box}body{margin:0;padding:28px 20px calc(32px + env(safe-area-inset-bottom));line-height:1.6}main{max-width:540px;margin:auto}header{margin:8px 0 24px}.brand{font-size:13px;letter-spacing:.16em;color:var(--amber);font-weight:700}h1{font-size:30px;line-height:1.25;letter-spacing:-.04em;margin:8px 0 12px}p{margin:8px 0}.intro,.hint,small{color:var(--dim);font-size:14px}.notice{padding:14px 16px;border-left:3px solid var(--amber);background:#272119;border-radius:0 12px 12px 0;font-size:14px;margin:20px 0}.card{background:#1d1c1a;border:1px solid var(--line);border-radius:18px;padding:20px;margin:16px 0}.card h2{font-size:17px;margin:0 0 12px;display:flex;align-items:center;gap:10px}.number{font-size:12px;border:1px solid #665038;width:25px;height:25px;border-radius:50%;text-align:center;line-height:23px;color:var(--amber)}.row{display:flex;justify-content:space-between;align-items:center;gap:14px}.status-dot{display:inline-block;width:7px;height:7px;background:#aaa59a;border-radius:50%;margin-right:7px}.online .status-dot{background:#b3ce91}.device-state{font-size:13px;color:var(--dim);margin-top:5px}dl{margin:16px 0 0;display:grid;grid-template-columns:auto 1fr;gap:8px 18px;font-size:14px}dt{color:var(--dim)}dd{margin:0;text-align:right;overflow-wrap:anywhere}label{display:block;font-size:14px;margin:12px 0 6px}input,button{font:inherit}input[type=text]{display:block;width:100%;min-height:50px;border-radius:11px;border:1px solid #6d5b44;background:#131313;color:#fff;padding:10px 14px;font-size:22px;letter-spacing:.2em}input:focus,button:focus-visible{outline:3px solid var(--amber);outline-offset:3px}input[type=file]{display:block;width:100%;font-size:14px;color:var(--dim);overflow:hidden;padding:12px 0}input[type=file]::file-selector-button{color:#eee9df;border:1px solid #6d5b44;background:#302a22;border-radius:10px;padding:11px 14px;margin-right:12px}button{min-height:48px;cursor:pointer;border-radius:11px;border:1px solid transparent;font-size:15px;font-weight:600;padding:10px 16px;background:var(--amber);color:#26190b}button:disabled{opacity:.4;cursor:default}.secondary{background:transparent;color:#e8dac6;border-color:#66523a}.quiet{background:transparent;color:var(--amber);font-size:13px;padding:5px 7px;min-height:40px}.full{width:100%;margin-top:12px}.digest{font:11px ui-monospace,SFMono-Regular,monospace;letter-spacing:.015em;word-break:break-all}.file-detail{border-top:1px solid var(--line);padding-top:12px;margin-top:10px}.file-detail dl{margin:0}.version-change{font-size:17px;color:var(--amber);font-weight:700;overflow-wrap:anywhere}.file-detail .digest{margin-top:12px;color:var(--dim)}#progress{margin-top:18px}progress{display:block;width:100%;height:9px;border:0;border-radius:9px;overflow:hidden;background:#3b352c;accent-color:var(--amber)}progress::-webkit-progress-bar{background:#3b352c}progress::-webkit-progress-value{background:var(--amber)}#progressText{font-size:14px;color:var(--amber);margin-top:9px}#message{border-radius:13px;padding:14px 16px;background:#29241d;border:1px solid #695337;font-size:14px;white-space:pre-line;overflow-wrap:anywhere}#message.error{border-color:#90604d;color:#ffccba;background:#30211c}#message.pending{border-color:#b9823f;color:#ffcf91;background:#302416;font-weight:600}#message:empty{display:none}.footer{font-size:12px;color:#aaa398;text-align:center;margin-top:24px}.authorized{color:#c4d7aa}.sr-only{position:absolute;width:1px;height:1px;padding:0;margin:-1px;overflow:hidden;clip:rect(0,0,0,0);white-space:nowrap;border:0}[hidden]{display:none!important}@media(max-width:360px){body{padding:20px 14px}.card{padding:16px}h1{font-size:27px}}
</style>
</head>
<body><main>
<header><div class="brand">THINCUBE / LOCAL UPDATE</div><h1>用手机升级固件</h1><p class="intro">设备热点无需 Wi-Fi 密码。连接后仍需输入屏幕上的 6 位升级码，选择固件，并在设备上按 A 确认。</p></header>
<div class="notice">请先把 ThinCube 应用固件 <strong>.bin</strong> 下载到手机。写入期间保持热点连接、停留在本页，勿锁屏。重启后还需约 60–90 秒自检；请保持供电，勿手动重启、关机或插拔 USB，直到设备显示绿色“UPDATE COMPLETE”。热点没有互联网是正常现象。</div>
<p class="hint">请用 Safari 或 Chrome 打开 <strong>http://192.168.4.1/</strong>。若系统弹出的网络认证窗口无法选择文件，请切换到完整浏览器。</p>
<section class="card" aria-labelledby="deviceTitle"><div class="row"><h2 id="deviceTitle">设备连接</h2><button id="refresh" class="quiet" type="button">重新连接</button></div><div id="deviceState" class="device-state"><span class="status-dot" aria-hidden="true"></span><span id="connection">正在连接设备…</span></div><dl><dt>设备</dt><dd id="device">ThinCube StickS3</dd><dt>当前版本</dt><dd id="currentVersion">—</dd><dt>电量</dt><dd id="battery">—</dd></dl></section>
<section class="card" aria-labelledby="sessionTitle"><h2 id="sessionTitle"><span class="number" aria-hidden="true">1</span>验证本次升级</h2><form id="sessionForm"><label for="code">设备屏幕上的 6 位升级码</label><input id="code" name="code" type="text" inputmode="numeric" pattern="[0-9]{6}" maxlength="6" autocomplete="off" autocapitalize="off" spellcheck="false" placeholder="000000" required aria-describedby="sessionHint"><p id="sessionHint" class="hint">升级码仅用于本次热点会话，不会改变 Mac 配对。</p><button id="authorize" class="full secondary" type="submit">验证升级码</button></form><p id="sessionState" class="hint" role="status" hidden></p></section>
<section class="card" aria-labelledby="fileTitle"><h2 id="fileTitle"><span class="number" aria-hidden="true">2</span>选择应用固件</h2><label class="sr-only" for="file">选择 ThinCube .bin 固件文件</label><input id="file" type="file" accept=".bin,application/octet-stream" aria-describedby="fileHint"><p id="fileHint" class="hint">支持 ThinCube StickS3 应用镜像，最大 2 MiB。文件会先在手机本地检查。</p><div id="fileDetail" class="file-detail" hidden><p id="versionChange" class="version-change"></p><dl><dt>目标版本</dt><dd id="targetVersion"></dd><dt>文件大小</dt><dd id="fileSize"></dd></dl><div class="digest">SHA-256<br><span id="digest"></span></div></div></section>
<section class="card" aria-labelledby="updateTitle"><h2 id="updateTitle"><span class="number" aria-hidden="true">3</span>确认并升级</h2><p class="hint">电量需达到 50%。提交后，请核对设备屏幕上的目标版本，并在 30 秒内按 A。按 B 可取消。</p><button id="start" type="button" class="full" disabled>开始升级</button><div id="progress" hidden><progress id="bar" max="100" value="0" aria-label="手机发送进度"></progress><p id="progressText" role="status" aria-live="polite"></p><button id="cancel" type="button" class="secondary full">取消本次升级</button></div></section>
<p id="message" role="status" aria-live="polite" aria-atomic="true"></p>
<p class="footer">所有操作均在手机和设备之间完成。<br>写入后设备会重启自检，并恢复原连接模式。<br>热点断开后，本页无法跟踪自检结果，请查看设备屏幕。</p>
</main>
<script>
'use strict';
// BEGIN TESTABLE FIRMWARE HELPERS
function parseFirmware(bytes, name, maximum) {
  if (!/\.bin$/i.test(name)) throw new Error('file_extension');
  if (bytes.length < 352) throw new Error('image_too_small');
  if (bytes.length > Math.min(maximum || 2097152, 2097152)) throw new Error('image_too_large');
  if (bytes[0] !== 0xe9 || (bytes[12] | bytes[13] << 8) !== 9) throw new Error('wrong_device_image');
  // esp_app_desc_t starts after the 24-byte image and 8-byte segment headers.
  if (bytes[32] !== 0x32 || bytes[33] !== 0x54 || bytes[34] !== 0xcd || bytes[35] !== 0xab) throw new Error('wrong_device_image');
  var identity = 'ThinCube-StickS3-v1';
  for (var i = 0; i < 32; i++) {
    if (bytes[288 + i] !== (i < identity.length ? identity.charCodeAt(i) : 0)) throw new Error('wrong_device_image');
  }
  var version = '', ended = false;
  for (var v = 320; v < 352; v++) {
    if (bytes[v] === 0) { ended = true; break; }
    if (!/[A-Za-z0-9.+_-]/.test(String.fromCharCode(bytes[v]))) throw new Error('invalid_version');
    version += String.fromCharCode(bytes[v]);
  }
  if (!ended || !version) throw new Error('invalid_version');
  // Official package names must agree with the descriptor inside the image.
  // Renaming an old .bin must not make it look like the requested release.
  var named = /^thincube-(v?\d+\.\d+\.\d+(?:[-+][A-Za-z0-9.+_-]+)?)\.bin$/i.exec(name);
  if (named && named[1] !== version) throw new Error('filename_version_mismatch');
  return {ver: version, size: bytes.length};
}

function isDowngrade(current, target) {
  function core(value) {
    var match = /^v?(\d+)\.(\d+)\.(\d+)(?:$|[-+])/.exec(value || '');
    if (!match) return null;
    var parts = match.slice(1,4).map(Number);
    return parts.every(function(n) { return n <= 4294967295; }) ? parts : null;
  }
  var running = core(current), next = core(target);
  if (!running || !next) return false;
  for (var i=0;i<3;i++) if (running[i] !== next[i]) return next[i] < running[i];
  return false;
}

// SHA-256 is implemented locally because HTTP at 192.168.4.1 is not a secure
// context on mobile browsers; crypto.subtle is consequently not available.
function sha256(bytes) {
  var k = [0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2];
  var h = [0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19];
  var padded = new Uint8Array(Math.ceil((bytes.length + 9) / 64) * 64);
  padded.set(bytes); padded[bytes.length] = 0x80;
  var view = new DataView(padded.buffer), bits = bytes.length * 8;
  view.setUint32(padded.length - 8, Math.floor(bits / 4294967296), false);
  view.setUint32(padded.length - 4, bits >>> 0, false);
  var w = new Uint32Array(64);
  function rotate(x,n) { return (x >>> n) | (x << (32-n)); }
  for (var offset = 0; offset < padded.length; offset += 64) {
    for (var i = 0; i < 16; i++) w[i] = view.getUint32(offset+i*4, false);
    for (var j = 16; j < 64; j++) {
      var x = w[j-15], y = w[j-2];
      var s0 = rotate(x,7)^rotate(x,18)^(x>>>3), s1 = rotate(y,17)^rotate(y,19)^(y>>>10);
      w[j] = (w[j-16]+s0+w[j-7]+s1) >>> 0;
    }
    var a=h[0], b=h[1], c=h[2], d=h[3], e=h[4], f=h[5], g=h[6], z=h[7];
    for (var n = 0; n < 64; n++) {
      var t1 = (z+(rotate(e,6)^rotate(e,11)^rotate(e,25))+((e&f)^(~e&g))+k[n]+w[n]) >>> 0;
      var t2 = ((rotate(a,2)^rotate(a,13)^rotate(a,22))+((a&b)^(a&c)^(b&c))) >>> 0;
      z=g; g=f; f=e; e=(d+t1)>>>0; d=c; c=b; b=a; a=(t1+t2)>>>0;
    }
    h[0]=(h[0]+a)>>>0; h[1]=(h[1]+b)>>>0; h[2]=(h[2]+c)>>>0; h[3]=(h[3]+d)>>>0;
    h[4]=(h[4]+e)>>>0; h[5]=(h[5]+f)>>>0; h[6]=(h[6]+g)>>>0; h[7]=(h[7]+z)>>>0;
  }
  return h.map(function(x) { return ('00000000'+x.toString(16)).slice(-8); }).join('');
}
// END TESTABLE FIRMWARE HELPERS

(function() {
  var ids = ['refresh','deviceState','connection','device','currentVersion','battery','sessionForm','code','authorize','sessionState','file','fileDetail','targetVersion','fileSize','versionChange','digest','start','progress','bar','progressText','cancel','message'];
  var ui = {}; ids.forEach(function(id) { ui[id] = document.getElementById(id); });
  var state = {info:null,token:'',file:null,manifest:null,busy:false,reading:false,authorizing:false,cancelling:false,cancelTask:null,xhr:null,cancelled:false,phase:'idle',run:0};
  var errors = {
    firmware_downgrade:'所选固件比设备当前版本更旧，已阻止降级。请重新选择最新版本；如需恢复旧版请使用 USB。', filename_version_mismatch:'文件名中的版本与固件内部版本不一致，请重新获取原始固件文件。', file_extension:'请选择下载到手机的 .bin 应用固件文件。', image_too_small:'文件过小或下载不完整，请重新下载 ThinCube 应用固件。', image_too_large:'文件超过设备支持的大小，请使用最大 2 MiB 的应用固件。', wrong_device_image:'这不是适用于 ThinCube StickS3 的应用固件，请检查下载文件。', invalid_version:'固件版本标识无效，请重新下载官方 ThinCube 固件。', file_read_failed:'无法读取文件。请先将固件下载到手机本地，再重新选择。', invalid_code:'升级码不正确，请输入设备当前显示的 6 位数字。', bad_code:'升级码不正确，请输入设备当前显示的 6 位数字。', code:'升级码不正确，请输入设备当前显示的 6 位数字。', rate_limited:'升级码尝试次数过多，请稍等后重试。', session_expired:'本次升级授权已过期，请重新输入设备屏幕上的升级码。', maintenance_required:'本次升级授权无效，请重新输入设备屏幕上的升级码。', maintenance_session_required:'本次升级授权无效，请重新输入设备屏幕上的升级码。', invalid_session:'本次升级授权无效，请重新输入设备屏幕上的升级码。', pairing_required:'本次升级授权无效，请重新输入设备屏幕上的升级码。', maintenance_inactive:'设备已退出手机升级模式，请重新进入该模式并连接热点。', not_in_maintenance:'设备已退出手机升级模式，请重新进入该模式并连接热点。', busy:'设备正在处理另一次升级，请先在设备上按 B 取消，稍后重试。', battery_must_be_known_and_at_least_50:'电量不足 50% 或电量尚未读出。请给设备充电，稍后重试。', no_compatible_slot:'设备没有足够大的备用升级分区，请通过 USB 升级。', boot_health_pending:'设备仍在进行上次升级的启动检查，请等待约 90 秒后再试。', battery_calibration_active:'设备正在校准电量，请先完成或退出校准。', sleep_pending:'设备正在准备休眠，请唤醒后重新进入手机升级模式。', transport_switch_pending:'设备正在切换连接，请稍后重试。', invalid_manifest:'升级信息无效，请重新选择固件。', bad_request:'设备未能识别请求，请重新连接并重试。', token:'本次写入凭证已失效，请重新发起升级并按 A 确认。', confirm_on_device:'本次升级尚未得到设备确认，请重新开始，并在设备上按 A。', timeout:'设备等待确认或上传超时。请重新开始，并在 30 秒内按 A 确认。', confirm_timeout:'设备等待确认超时，请重新开始并在 30 秒内按 A。', cancelled:'本次升级已取消，可以重新选择文件并重试。', cancelled_or_timeout:'设备已中止本次升级，请重新开始。', upload_timeout:'上传超时，设备已中止本次写入。请保持热点连接后重试。', receive_timeout:'设备接收超时，已中止本次写入。请保持热点连接后重试。', connection_lost:'与设备的连接中断。请检查手机仍连接 ThinCube 热点，再重新连接。', request_timeout:'设备响应超时。请保持热点连接，查看设备屏幕后重试。', exact_length_required:'固件文件长度与升级信息不一致，请重新选择文件。', sha256_mismatch:'固件传输校验不一致，设备未启用此镜像，请重新选择文件并重试。', erase_failed:'备用分区擦除失败，请重新启动设备后重试。', write_failed:'固件写入失败，设备未启用此镜像，请重新启动后重试。', image_verification_failed:'设备镜像校验失败，未启用此固件，请重新下载后重试。', version_mismatch:'固件版本与升级信息不一致，请重新选择文件。', set_boot_failed:'固件写入后无法切换启动分区，请重新启动设备后重试。', cannot_cancel:'设备已进入最终校验或重启阶段，请勿断电，等待设备完成。', invalid_response:'未收到有效的设备确认，请检查设备屏幕并重新连接。', upload_not_confirmed:'手机没有收到设备的写入校验成功确认。请先查看设备屏幕和当前版本，再决定是否重试。'
  };
  errors.too_many_attempts = '升级码连续输错 5 次，请等待 30 秒后重试。';
  errors.not_ready = '设备正在退出或尚未准备好升级热点，请查看设备屏幕后重新连接。';
  errors.json_required = '设备未能识别升级请求，请刷新本页后重试。';
  function message(text,kind) { ui.message.textContent = text || ''; ui.message.className = kind || ''; }
  function explanation(error) { var code = error && error.message || 'invalid_response'; return errors[code] || ('设备返回错误：'+code+'。请查看设备屏幕后重试。'); }
  function updateControls() {
    var written = state.phase === 'written';
    ui.start.disabled = written || state.busy || state.cancelling || state.reading || state.authorizing || !state.token || !state.manifest || !state.info;
    ui.start.textContent = written ? '请查看设备自检结果' : state.busy ? '升级进行中…' : '开始升级';
    ui.file.disabled = written || state.busy || state.cancelling; ui.code.disabled = written || state.busy || state.cancelling || state.authorizing;
    ui.authorize.disabled = written || state.busy || state.cancelling || state.authorizing; ui.refresh.disabled = written || state.busy || state.cancelling;
  }
  function forgetSession() {
    state.token = ''; ui.sessionForm.hidden = false; ui.sessionState.hidden = true; ui.code.value = ''; updateControls();
  }
  function jsonRequest(path,method,body,authorized,timeout) {
    return new Promise(function(resolve,reject) {
      var xhr = new XMLHttpRequest(); xhr.open(method || 'GET',path,true); xhr.timeout = timeout || 6000;
      if (authorized && state.token) xhr.setRequestHeader('X-ThinCube-Maintenance',state.token);
      if (body !== undefined && body !== null) xhr.setRequestHeader('Content-Type','application/json');
      xhr.onload = function() {
        var data; try { data = JSON.parse(xhr.responseText); } catch (_) { reject(new Error('invalid_response')); return; }
        if (xhr.status < 200 || xhr.status >= 300 || data.ok === false || data.error) {
          var error = new Error(data.error || data.err || 'invalid_response'); error.status = xhr.status;
          if (authorized && xhr.status === 401) forgetSession(); reject(error); return;
        }
        resolve(data);
      };
      xhr.onerror = function() { reject(new Error('connection_lost')); };
      xhr.ontimeout = function() { reject(new Error('request_timeout')); };
      xhr.send(body !== undefined && body !== null ? JSON.stringify(body) : null);
    });
  }
  function sleep(ms) { return new Promise(function(resolve) { setTimeout(resolve,ms); }); }
  function readFile(file) {
    return new Promise(function(resolve,reject) {
      var reader = new FileReader(); reader.onload = function() { resolve(new Uint8Array(reader.result)); };
      reader.onerror = function() { reject(new Error('file_read_failed')); }; reader.readAsArrayBuffer(file);
    });
  }
  async function refresh() {
    ui.refresh.disabled = true;
    try {
      var info = await jsonRequest('/maintenance/info','GET',null,false);
      if (info.device !== 'ThinCube-StickS3-v1' || typeof info.version !== 'string') throw new Error('wrong_device_image');
      state.info = info; ui.connection.textContent = '已连接升级热点'; ui.deviceState.className = 'device-state online';
      ui.device.textContent = 'ThinCube StickS3'; ui.currentVersion.textContent = info.version;
      ui.battery.textContent = typeof info.level === 'number' && info.level >= 0 ? info.level+'%' : '正在读取';
      if (state.file && state.manifest && state.file.size > Math.min(info.maxSize || 2097152,2097152)) {
        state.manifest = null; ui.fileDetail.hidden = true; throw new Error('image_too_large');
      }
      if (state.manifest && isDowngrade(info.version,state.manifest.ver)) { state.manifest = null; message(errors.firmware_downgrade,'error'); }
      else if (typeof info.level === 'number' && info.level >= 0 && info.level < 50) message(errors.battery_must_be_known_and_at_least_50,'error');
      else if (!state.busy) message('请核对设备屏幕上的升级码，并选择固件。');
    } catch (error) {
      state.info = null; ui.connection.textContent = '设备连接不可用'; ui.deviceState.className = 'device-state'; message(explanation(error),'error');
    } finally { updateControls(); }
  }
  ui.refresh.addEventListener('click',refresh);
  ui.sessionForm.addEventListener('submit',async function(event) {
    event.preventDefault(); if (state.busy || state.authorizing) return;
    if (!/^[0-9]{6}$/.test(ui.code.value)) { message(errors.invalid_code,'error'); return; }
    state.authorizing = true; updateControls();
    try {
      var result = await jsonRequest('/maintenance/session','POST',{code:ui.code.value},false);
      if (!/^[a-f0-9]{32}$/i.test(result.token || '')) throw new Error('invalid_response');
      state.token = result.token; ui.code.value = ''; ui.sessionForm.hidden = true;
      ui.sessionState.textContent = '本次升级已授权。退出热点模式后自动失效。'; ui.sessionState.className = 'hint authorized'; ui.sessionState.hidden = false;
      message(state.manifest ? '固件已检查，可以开始升级。' : '升级码已验证，请选择固件。');
    } catch (error) { forgetSession(); message(explanation(error),'error'); }
    finally { state.authorizing = false; updateControls(); }
  });
  var fileRead = 0;
  ui.file.addEventListener('change',async function() {
    var currentRead = ++fileRead, file = ui.file.files[0]; state.file = null; state.manifest = null; ui.fileDetail.hidden = true;
    if (!file) { state.reading = false; updateControls(); return; }
    state.reading = true; updateControls(); message('正在手机上检查固件和计算 SHA-256…');
    try {
      // Reject oversized files before allocating or asking a cloud file provider
      // to download them. The full image is limited to one 2 MiB app partition.
      if (file.size < 352) throw new Error('image_too_small');
      if (file.size > Math.min(state.info && state.info.maxSize || 2097152,2097152)) throw new Error('image_too_large');
      if (!/\.bin$/i.test(file.name)) throw new Error('file_extension');
      var bytes = await readFile(file); if (currentRead !== fileRead) return;
      var manifest = parseFirmware(bytes,file.name,state.info && state.info.maxSize);
      await sleep(20); if (currentRead !== fileRead) return;
      manifest.sha256 = sha256(bytes);
      ui.versionChange.textContent = (state.info ? state.info.version : '—')+' → '+manifest.ver;
      ui.targetVersion.textContent = manifest.ver; ui.fileSize.textContent = (manifest.size/1024).toFixed(1)+' KiB'; ui.digest.textContent = manifest.sha256; ui.fileDetail.hidden = false;
      if (state.info && isDowngrade(state.info.version,manifest.ver)) throw new Error('firmware_downgrade');
      state.file = file; state.manifest = manifest;
      message('固件型号和文件校验已完成。'+(state.token ? '现在可以开始升级。' : '请先验证设备屏幕上的升级码。'));
    } catch (error) { if (currentRead === fileRead) message(explanation(error),'error'); }
    finally { if (currentRead === fileRead) { state.reading = false; updateControls(); } }
  });
  function upload(file,token) {
    return new Promise(function(resolve,reject) {
      var xhr = new XMLHttpRequest(); state.xhr = xhr;
      xhr.open('POST','/ota',true); xhr.timeout = 135000;
      xhr.setRequestHeader('Content-Type','application/octet-stream');
      xhr.setRequestHeader('X-ThinCube-Maintenance',state.token); xhr.setRequestHeader('X-ThinCube-OTA',token);
      xhr.upload.onprogress = function(event) {
        if (state.cancelled) return;
        var percent = event.lengthComputable ? Math.min(100,Math.floor(event.loaded*100/event.total)) : 0;
        ui.bar.value = percent; ui.progressText.textContent = percent < 100 ? '手机发送进度 '+percent+'% · 设备同步写入' : '手机已发送完毕，等待设备完成写入与校验…';
      };
      xhr.onload = function() {
        state.xhr = null;
        var result; try { result = JSON.parse(xhr.responseText); } catch (_) { reject(new Error('upload_not_confirmed')); return; }
        if (xhr.status === 200 && result.ok === true) { resolve(); return; }
        if (xhr.status === 401) forgetSession();
        reject(new Error(result.error || result.err || 'upload_not_confirmed'));
      };
      xhr.onerror = function() { state.xhr = null; reject(new Error('upload_not_confirmed')); };
      xhr.ontimeout = function() { state.xhr = null; reject(new Error('upload_timeout')); };
      xhr.onabort = function() { state.xhr = null; reject(new Error('cancelled')); };
      xhr.send(file);
    });
  }
  ui.start.addEventListener('click',async function() {
    if (ui.start.disabled) return;
    var run = ++state.run, file = state.file, manifest = state.manifest;
    state.busy = true; state.cancelled = false; state.cancelTask = null; state.phase = 'begin'; updateControls();
    ui.progress.hidden = false; ui.bar.value = 0; ui.cancel.disabled = true;
    ui.progressText.textContent = '正在向设备提交升级信息…'; message('请保持热点连接，并查看设备屏幕。');
    try {
      if (state.info && isDowngrade(state.info.version,manifest.ver)) throw new Error('firmware_downgrade');
      var result = await jsonRequest('/ota/begin','POST',manifest,true);
      if (!/^[a-f0-9]{32}$/i.test(result.token || '')) throw new Error('invalid_response');
      state.phase = 'confirm'; ui.cancel.disabled = false;
      var until = Date.now()+35000;
      while (true) {
        if (state.cancelled || run !== state.run) throw new Error('cancelled');
        ui.progressText.textContent = '等待设备确认 · 核对 '+manifest.ver+' 后按 A';
        var status = await jsonRequest('/ota/status','GET',null,true);
        if (status.stage === 'failed') throw new Error(status.err || 'cancelled');
        if (status.stage === 'ready') break;
        if (status.stage !== 'confirm') throw new Error('confirm_on_device');
        if (Date.now() >= until) throw new Error('timeout');
        await sleep(700);
      }
      if (state.cancelled || run !== state.run) throw new Error('cancelled');
      state.phase = 'upload'; ui.progressText.textContent = '设备已确认，开始发送固件…';
      await upload(file,result.token);
      // HTTP 200 with {ok:true} acknowledges hash/image verification and
      // boot-slot selection only. Boot health has not yet been confirmed.
      state.phase = 'written'; ui.cancel.disabled = true; ui.bar.value = 100;
      ui.progressText.textContent = '固件已写入，正在重启自检';
      message('固件 '+manifest.ver+' 已写入并通过文件校验，尚未确认升级完成。\n请保持供电，等待约 60–90 秒自检；勿手动重启、关机或插拔 USB。\n热点即将断开，本页无法跟踪自检结果，也不会自动宣告成功。请以设备绿色“UPDATE COMPLETE”和版本号为准，或在 Mac 重新连接后核对版本及启动检查结果。','pending');
      state.token = ''; state.manifest = null; state.info = null;
    } catch (error) {
      var detail = error;
      if (state.phase === 'upload' && error.message !== 'cancelled') {
        try { var failure = await jsonRequest('/ota/status','GET',null,true); if (failure.stage === 'failed' && failure.err) detail = new Error(failure.err); } catch (_) {}
      }
      if (run === state.run) {
        if (state.cancelled) {
          // Aborting a phone request does not prove a flash commit stopped.
          // Wait for the cancellation response so this catch cannot overwrite
          // cannot_cancel with a misleading "cancelled" acknowledgement.
          var outcome = await state.cancelTask;
          if (outcome && outcome.accepted) {
            ui.progressText.textContent = '设备已接受取消请求';
            message('设备已接受取消请求。请查看设备屏幕，按 B 返回后可重试。');
          } else if (outcome && outcome.code === 'cannot_cancel') {
            ui.progressText.textContent = '设备已进入最终校验或重启阶段';
            message(errors.cannot_cancel,'error');
          } else {
            ui.progressText.textContent = '取消状态待设备确认';
            message('手机已停止发送，尚未收到设备的取消确认。请勿断电；若设备已进入最终校验，取消可能无法执行。请查看设备屏幕，按 B 取消或等待操作结束。','error');
          }
        } else {
          ui.progressText.textContent = '本次升级未获得写入确认';
          message(explanation(detail),'error');
        }
      }
    } finally {
      if (run === state.run) { state.busy = false; ui.cancel.disabled = true; updateControls(); }
    }
  });
  ui.cancel.addEventListener('click',async function() {
    if (!state.busy || ui.cancel.disabled || state.phase === 'written') return;
    ui.cancel.disabled = true; state.cancelled = true; state.cancelling = true; updateControls();
    var settle, outcome;
    state.cancelTask = new Promise(function(resolve) { settle = resolve; });
    if (state.xhr) state.xhr.abort();
    ui.progressText.textContent = '正在请求设备取消；进入最终校验后无法中止';
    message('正在确认设备能否取消本次升级，请勿断电。');
    try {
      await jsonRequest('/ota/cancel','POST',{},true);
      outcome = {accepted:true};
    } catch (error) {
      outcome = {code:error.message};
    } finally { settle(outcome); state.cancelling = false; updateControls(); }
  });
  window.addEventListener('beforeunload',function(event) { if (state.busy) { event.preventDefault(); event.returnValue = ''; } });
  refresh();
})();
</script>
</body></html>)THINCUBE";
