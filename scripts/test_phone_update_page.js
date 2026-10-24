#!/usr/bin/env node
'use strict';

// Exercise the shipped inline script, including the offline implementation,
// rather than keeping a separate JavaScript copy in sync with firmware.
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const root = path.resolve(__dirname, '..');
const header = fs.readFileSync(path.join(root, 'firmware/src/phone_update_page.h'), 'utf8');
const script = header.match(/<script>\s*([\s\S]*?)<\/script>/)[1];
const helpers = script.match(/\/\/ BEGIN TESTABLE FIRMWARE HELPERS([\s\S]*?)\/\/ END TESTABLE FIRMWARE HELPERS/)[1];
const core = vm.createContext({Uint8Array,Uint32Array,DataView,Error});
vm.runInContext(helpers, core);
const hash = data => core.sha256(new Uint8Array(data));
const expectedHash = data => crypto.createHash('sha256').update(data).digest('hex');
function image(size = 1024, version = 'v0.4.65') {
  const bytes = Buffer.alloc(size);
  bytes[0] = 0xe9; bytes.writeUInt16LE(9, 12); bytes.writeUInt32LE(0xabcd5432, 32);
  bytes.write('ThinCube-StickS3-v1', 288, 'ascii'); bytes.write(version, 320, 'ascii');
  return bytes;
}

assert.equal(hash(Buffer.alloc(0)), 'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855');
assert.equal(hash(Buffer.from('abc')), 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad');
assert.equal(hash(Buffer.from('abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq')), '248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1');
assert.equal(hash(Buffer.alloc(1000000, 'a')), 'cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0');
for (const size of [1, 55, 56, 63, 64, 65, 119, 120, 127, 128, 129, 352, 65536, 2097152]) {
  const bytes = Buffer.alloc(size);
  for (let i = 0; i < size; i++) bytes[i] = (i * 197 + 37) & 255;
  assert.equal(hash(bytes), expectedHash(bytes), `SHA-256 at length ${size}`);
}
const valid = image();
assert.equal(core.isDowngrade('v0.4.70','v0.4.67'),true);
assert.equal(core.isDowngrade('v0.4.70','v0.4.67-legacy'),true);
assert.equal(core.isDowngrade('v1.0.0','v0.99.999'),true);
assert.equal(core.isDowngrade('v0.4.70','v0.4.70'),false);
assert.equal(core.isDowngrade('v0.4.70','v0.4.71'),false);
assert.equal(core.isDowngrade('v0.4.9','v0.4.10'),false);
assert.equal(core.isDowngrade('dev','v0.4.70'),false);
assert.equal(core.isDowngrade('v0.4.70','v0.4.4294967296'),false);
assert.throws(() => core.parseFirmware(image(1024,'v0.4.67'),'thincube-v0.4.70.bin'),/filename_version_mismatch/);
assert.equal(core.parseFirmware(valid,'thincube-v0.4.65.bin').ver,'v0.4.65');

assert.deepEqual({...core.parseFirmware(valid, 'ThinCube.BIN', 2097152)}, {ver:'v0.4.65',size:1024});
assert.equal(core.parseFirmware(image(352), 'app.bin').size, 352);
assert.throws(() => core.parseFirmware(valid, 'firmware.zip'), /file_extension/);
assert.throws(() => core.parseFirmware(Buffer.alloc(351), 'app.bin'), /image_too_small/);
assert.throws(() => core.parseFirmware(Buffer.alloc(2097153), 'app.bin'), /image_too_large/);
assert.throws(() => core.parseFirmware(valid, 'app.bin', 512), /image_too_large/);
for (const offset of [0, 12, 13, 32, 33, 34, 35, 288, 307, 319]) {
  const bad = Buffer.from(valid); bad[offset] ^= 1;
  assert.throws(() => core.parseFirmware(bad, 'app.bin'), /wrong_device_image/, `Identity byte ${offset}`);
}
for (const bytes of [image(352, ''), image(352, 'x'.repeat(32)), image(352, 'v1\n'), image(352, 'v1<script>')]) {
  assert.throws(() => core.parseFirmware(bytes, 'app.bin'), /invalid_version/);
}
// A bootloader, partition table, or flash dump must never be admitted as app.
for (const filename of ['firmware/.pio/build/m5stack-sticks3/bootloader.bin','firmware/.pio/build/m5stack-sticks3/partitions.bin']) {
  const absolute = path.join(root, filename);
  if (fs.existsSync(absolute)) assert.throws(() => core.parseFirmware(fs.readFileSync(absolute), 'app.bin'));
}
const realImage = path.join(root, 'firmware/.pio/build/m5stack-sticks3/firmware.bin');
if (fs.existsSync(realImage)) {
  const bytes = fs.readFileSync(realImage);
  const manifest = core.parseFirmware(bytes, 'firmware.bin');
  assert.equal(hash(bytes), expectedHash(bytes));
  console.log(`Verified real firmware: ${manifest.ver}, ${manifest.size} bytes`);
}
console.log('PASS: offline SHA-256 vectors, padding boundaries, full image, and image admission checks');

function browser(handler) {
  const elements = new Map(), requests = [];
  class Element {
    constructor(id) { this.id=id; this.textContent=''; this.className=''; this.hidden=false; this.disabled=false; this.value=''; this.files=[]; this.handlers={}; }
    addEventListener(name, fn) { this.handlers[name]=fn; }
    async fire(name) { return this.handlers[name]({preventDefault(){}}); }
  }
  const element = id => { if (!elements.has(id)) elements.set(id, new Element(id)); return elements.get(id); };
  class XHR {
    constructor() { this.headers={}; this.upload={}; this.status=0; this.responseText=''; this.aborted=false; }
    open(method,url) { this.method=method; this.url=url; }
    setRequestHeader(name,value) { this.headers[name]=value; }
    send(body) {
      this.body=body; requests.push(this);
      setTimeout(() => {
        if (this.aborted) return;
        const result = handler(this, elements, requests);
        if (result === null) return;
        if (result.networkError) { this.onerror(); return; }
        const deliver=() => {
          this.status = result.status || 200; this.responseText = JSON.stringify(result.data);
          this.onload();
        };
        if(result.delay)setTimeout(deliver,result.delay);else deliver();
      }, 0);
    }
    abort() { this.aborted=true; if(this.onabort)this.onabort(); }
  }
  class Reader {
    readAsArrayBuffer(file) { this.result=Uint8Array.from(file.bytes).buffer; setTimeout(() => this.onload(), 0); }
  }
  const context = vm.createContext({Uint8Array,Uint32Array,DataView,Error,XMLHttpRequest:XHR,FileReader:Reader,document:{getElementById:element},window:{addEventListener(){}},setTimeout:(fn,ms)=>setTimeout(fn,Math.min(ms,2)),Date});
  vm.runInContext(script,context);
  return {element, requests};
}
const info = {version:'v0.4.64',device:'ThinCube-StickS3-v1',level:90,maxSize:2097152,stage:'idle',err:''};
const sessionToken = 'a'.repeat(32), uploadToken = 'b'.repeat(32);
const tick = () => new Promise(resolve => setTimeout(resolve, 10));
async function setup(client) {
  await tick();
  client.element('code').value = '123456'; await client.element('sessionForm').fire('submit');
  client.element('file').files=[{name:'firmware.bin',size:valid.length,bytes:valid}]; await client.element('file').fire('change');
  assert.equal(client.element('start').disabled,false);
}
function baseResponse(request) {
  if(request.url==='/maintenance/info')return {data:info};
  if(request.url==='/maintenance/session') {
    assert.equal(request.method,'POST'); assert.equal(JSON.parse(request.body).code,'123456');
    assert.equal(request.headers['X-ThinCube-Maintenance'],undefined);
    return {data:{token:sessionToken}};
  }
  assert.equal(request.headers['X-ThinCube-Maintenance'],sessionToken,request.url+' auth');
  if(request.url==='/ota/begin') {
    assert.equal(request.method,'POST'); const manifest=JSON.parse(request.body);
    assert.equal(manifest.ver,'v0.4.65'); assert.equal(manifest.size,valid.length); assert.equal(manifest.sha256,expectedHash(valid));
    return {data:{ok:true,token:uploadToken}};
  }
  return undefined;
}
async function downgradeFlows() {
  const old = image(1024,'v0.4.67');
  const client = browser(request => {
    if(request.url==='/maintenance/info') return {data:{...info,version:'v0.4.70'}};
    return baseResponse(request);
  });
  await tick();
  client.element('code').value='123456'; await client.element('sessionForm').fire('submit');
  client.element('file').files=[{name:'thincube-v0.4.67.bin',size:old.length,bytes:old}];
  await client.element('file').fire('change');
  assert.equal(client.element('start').disabled,true);
  assert.match(client.element('message').textContent,/已阻止降级/);
  assert.equal(client.element('versionChange').textContent,'v0.4.70 → v0.4.67');
  assert.equal(client.requests.some(r=>r.url==='/ota/begin'||r.url==='/ota'),false);
  client.element('file').files=[{name:'thincube-v0.4.70.bin',size:old.length,bytes:old}];
  await client.element('file').fire('change');
  assert.equal(client.element('start').disabled,true);
  assert.match(client.element('message').textContent,/文件名中的版本与固件内部版本不一致/);
  const same=image(1024,'v0.4.70');
  client.element('file').files=[{name:'thincube-v0.4.70.bin',size:same.length,bytes:same}];
  await client.element('file').fire('change');
  assert.equal(client.element('start').disabled,false);
  assert.equal(client.element('versionChange').textContent,'v0.4.70 → v0.4.70');
  console.log('PASS: older image and misleading filename rejected, numeric version order and same-version reinstall admitted');
}
async function flows() {
  let polls = 0, sawProgressWithoutSuccess = false;
  const written = browser((request,elements) => {
    const base=baseResponse(request); if(base)return base;
    if(request.url==='/ota/status')return {data:{stage:++polls===1?'confirm':'ready',pct:0,err:''}};
    assert.equal(request.url,'/ota'); assert.equal(request.headers['X-ThinCube-OTA'],uploadToken);
    assert.equal(request.headers['Content-Type'],'application/octet-stream'); assert.equal(request.body.bytes,valid);
    request.upload.onprogress({lengthComputable:true,loaded:valid.length,total:valid.length});
    assert.equal(elements.get('message').className,''); assert.match(elements.get('progressText').textContent,/等待设备完成/);
    sawProgressWithoutSuccess=true;
    return {data:{ok:true}};
  });
  await setup(written); await written.element('start').fire('click');
  assert.equal(sawProgressWithoutSuccess,true); assert.equal(written.element('message').className,'pending');
  assert.equal(written.element('progressText').textContent,'固件已写入，正在重启自检');
  assert.match(written.element('message').textContent,/尚未确认升级完成/);
  assert.match(written.element('message').textContent,/60–90 秒自检/);
  assert.match(written.element('message').textContent,/勿手动重启、关机或插拔 USB/);
  assert.match(written.element('message').textContent,/本页无法跟踪自检结果/);
  assert.match(written.element('message').textContent,/请以设备绿色“UPDATE COMPLETE”和版本号为准/);
  assert.doesNotMatch(written.element('message').textContent,/升级成功|已升级完成/);
  assert.equal(written.element('start').disabled,true);
  assert.equal(written.element('start').textContent,'请查看设备自检结果');
  assert.equal(written.element('refresh').disabled,true);
  assert.equal(written.element('file').disabled,true);
  assert.equal(written.element('bar').value,100,'100% describes transfer, not boot health');
  const pendingMessage = written.element('message').textContent, writtenRequests = written.requests.length;
  await tick(); // The mock runs page timers at most 2 ms apart: elapsed time cannot imply VALID.
  assert.equal(written.element('message').className,'pending');
  assert.equal(written.element('message').textContent,pendingMessage);
  assert.equal(written.requests.length,writtenRequests,'no pretend health polling after the hotspot closes');

  let failedUpload = false;
  const failed = browser(request => {
    const base=baseResponse(request); if(base)return base;
    if(request.url==='/ota/status')return {data:{stage:failedUpload?'failed':'ready',pct:0,err:failedUpload?'sha256_mismatch':''}};
    failedUpload=true; return {status:400,data:{ok:false}};
  });
  await setup(failed); await failed.element('start').fire('click');
  assert.equal(failed.element('message').className,'error'); assert.match(failed.element('message').textContent,/传输校验不一致/);
  assert.equal(failed.element('start').disabled,false,'failed upload can be retried');

  let uploaded = false;
  const noAck = browser(request => {
    const base=baseResponse(request); if(base)return base;
    if(request.url==='/ota/status')return {data:{stage:uploaded?'done':'ready',pct:100,err:''}};
    uploaded=true; return {data:{ok:false}};
  });
  await setup(noAck); await noAck.element('start').fire('click');
  assert.equal(noAck.element('message').className,'error'); assert.match(noAck.element('message').textContent,/没有收到设备的写入校验成功确认/);
  assert.doesNotMatch(noAck.element('message').textContent,/固件.*已写入|升级成功/);

  let connectionDropped = false;
  const droppedAck = browser(request => {
    const base=baseResponse(request); if(base)return base;
    if(request.url==='/ota/status')return {data:{stage:connectionDropped?'done':'ready',pct:100,err:''}};
    connectionDropped=true; return {networkError:true};
  });
  await setup(droppedAck); await droppedAck.element('start').fire('click');
  assert.equal(droppedAck.element('message').className,'error'); assert.match(droppedAck.element('message').textContent,/没有收到设备的写入校验成功确认/);
  assert.doesNotMatch(droppedAck.element('message').textContent,/固件.*已写入|升级成功/);

  const unauthorized = browser(request => {
    if(request.url==='/ota/begin') return {status:401,data:{error:'maintenance_required'}};
    return baseResponse(request);
  });
  await setup(unauthorized); await unauthorized.element('start').fire('click');
  assert.equal(unauthorized.element('sessionForm').hidden,false); assert.equal(unauthorized.element('start').disabled,true);
  assert.match(unauthorized.element('message').textContent,/重新输入/);

  let pendingUpload;
  const cancelled = browser(request => {
    if(request.url==='/ota/cancel')return {data:{ok:true,stage:'failed'}};
    const base=baseResponse(request); if(base)return base;
    if(request.url==='/ota/status')return {data:{stage:'ready',pct:0,err:''}};
    pendingUpload=request; return null;
  });
  await setup(cancelled); const completion=cancelled.element('start').fire('click'); await tick();
  assert.ok(pendingUpload); await cancelled.element('cancel').fire('click'); await completion;
  assert.equal(pendingUpload.aborted,true); assert.ok(cancelled.requests.some(request=>request.url==='/ota/cancel'));
  assert.notEqual(cancelled.element('message').className,'pending'); assert.equal(cancelled.element('start').disabled,false);

  const cancelledConfirmation = browser(request => {
    if(request.url==='/ota/cancel')return {data:{ok:true,stage:'failed'},delay:30};
    const base=baseResponse(request); if(base)return base;
    if(request.url==='/ota/status')return {data:{stage:'confirm',pct:0,err:''}};
    throw new Error('No file may be sent before physical confirmation');
  });
  await setup(cancelledConfirmation); const waitForA=cancelledConfirmation.element('start').fire('click'); await tick();
  const delayedCancel=cancelledConfirmation.element('cancel').fire('click'); await tick();
  assert.match(cancelledConfirmation.element('message').textContent,/正在确认设备能否取消/);
  assert.equal(cancelledConfirmation.element('start').disabled,true,'cannot retry before cancellation response');
  await delayedCancel; await waitForA; await tick();
  assert.equal(cancelledConfirmation.requests.some(request=>request.url==='/ota'),false);
  assert.equal(cancelledConfirmation.element('start').disabled,false);
  assert.match(cancelledConfirmation.element('message').textContent,/设备已接受取消请求/);

  let verifyingUpload;
  const cannotCancel = browser(request => {
    if(request.url==='/ota/cancel')return {status:409,data:{error:'cannot_cancel'},delay:30};
    const base=baseResponse(request); if(base)return base;
    if(request.url==='/ota/status')return {data:{stage:'ready',pct:0,err:''}};
    verifyingUpload=request; return null;
  });
  await setup(cannotCancel); const verifying=cannotCancel.element('start').fire('click'); await tick();
  assert.ok(verifyingUpload); const failedCancel=cannotCancel.element('cancel').fire('click'); await tick();
  assert.equal(verifyingUpload.aborted,true);
  assert.equal(cannotCancel.element('start').disabled,true);
  assert.doesNotMatch(cannotCancel.element('message').textContent,/已取消|已接受取消|会安全中止/);
  await failedCancel; await verifying; await tick();
  assert.match(cannotCancel.element('message').textContent,/最终校验或重启阶段/);
  assert.match(cannotCancel.element('message').textContent,/请勿断电/);
  assert.doesNotMatch(cannotCancel.element('message').textContent,/已取消|已接受取消|会安全中止/);
  assert.equal(cannotCancel.element('message').className,'error');
  console.log('PASS: authenticated upload, write ACK stays pending boot health, no timed success or false ACK, exact errors, retry, session expiry, delayed cancellation, and VERIFY refusal');
}
downgradeFlows().then(flows).catch(error => { console.error(error); process.exitCode=1; });
