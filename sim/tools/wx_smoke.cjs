/* The browser client's LIVE-WEATHER path, run in node against web/gpu.js with stubbed browser globals.
 * Not a build target (like tools/fb_tournament.py): run it after `make wasm`, from sim/.
 *
 *   node tools/wx_smoke.cjs                 # both cases
 *
 * WHAT IT PROVES, and why it is worth stubbing a browser for: the two things about live weather that
 * cannot be seen from a native build. (1) With /wx unreachable the app boots and FLIES — calm, one
 * warning, no hang and no abort (the /elev lesson: a 503 must never kill a session). (2) With /wx
 * answering, the blob is parsed and adopted at a FRAME BOUNDARY, which is a log line the sim loop
 * itself emits and therefore proof that frames kept running across the switch.
 *
 * The stubs are deliberately the minimum: window/location/navigator for the config the app reads,
 * a synchronous XMLHttpRequest that serves web/missions/ off disk (that is how the app fetches its
 * mission), a no-op Worker (the tile pool has nothing to talk to here) and a fetch() that either
 * rejects or hands back the committed fixture. Everything after the weather decision — WebGPU, tiles —
 * is expected to be unavailable in node and is not what this measures. */
'use strict';
const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');

const SIM = process.cwd();
const GPU = path.join(SIM, 'web', 'gpu.js');
const FIXTURE = path.join(SIM, 'assets', 'wx-2026-07-27T00Z.wxb');

/* Child mode: this file re-executes itself with WX_BLOB set/unset, because the app boots exactly once
 * per process and the two cases are two boots. */
if (process.env.FB_WX_SMOKE_CHILD) {
  const blob = process.env.WX_BLOB || '';
  global.window = { FB_TILES_URL: 'http://localhost:8081', FB_ORIGIN_LAT: 47.179846,
                    FB_ORIGIN_LON: 7.411427, FB_SIM_UTC: 0 };
  global.location = { search: '' };
  /* node has its own read-only `navigator`; only define one where it is missing. */
  if (typeof globalThis.navigator === 'undefined')
    Object.defineProperty(globalThis, 'navigator', { value: { hardwareConcurrency: 4 }, writable: true });
  global.Worker = function () { this.postMessage = () => {}; this.terminate = () => {}; };
  global.XMLHttpRequest = function () {
    this.status = 0; this.responseText = '';
    this.open = (m, u) => { this.url = u; };
    this.overrideMimeType = () => {};
    this.send = () => {
      const p = this.url.startsWith('/missions/') ? path.join(SIM, 'web', this.url) : null;
      if (p && fs.existsSync(p)) { this.responseText = fs.readFileSync(p, 'latin1'); this.status = 200; }
      else this.status = 404;
    };
  };
  global.fetch = () => {
    if (!blob) return Promise.reject(new Error('connection refused'));
    const b = fs.readFileSync(blob);
    return Promise.resolve({ ok: true, status: 200,
      arrayBuffer: () => Promise.resolve(b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength)) });
  };
  /* Long enough for a few frames past the switch, short enough that a hang is a failure and not a wait. */
  setTimeout(() => { console.log('SMOKE_DONE'); process.exit(0); }, 8000);
  try { require(GPU); } catch (e) { console.log('SMOKE_THREW ' + String(e).split('\n')[0]); }
  return;
}

if (!fs.existsSync(GPU)) { console.error('wx_smoke: no web/gpu.js — run `make -C sim wasm` first'); process.exit(1); }
if (!fs.existsSync(FIXTURE)) { console.error('wx_smoke: no ' + FIXTURE); process.exit(1); }

function run(label, env) {
  const r = spawnSync(process.execPath, [__filename], {
    cwd: SIM, encoding: 'utf8', timeout: 60000,
    env: Object.assign({}, process.env, { FB_WX_SMOKE_CHILD: '1', FB_LOAD_TIMEOUT_MS: '50' }, env),
  });
  const out = (r.stdout || '') + (r.stderr || '');
  console.log('--- ' + label);
  out.split('\n').filter((l) => / wx |SMOKE_|loading done/.test(l)).forEach((l) => console.log('    ' + l));
  return out;
}

let fails = 0;
function want(out, re, what) {
  const ok = re.test(out);
  if (!ok) fails++;
  console.log((ok ? '    OK   ' : '    FAIL ') + what);
}

const dead = run('/wx unreachable', { WX_BLOB: '' });
want(dead, /INFO wx source source=calm pending=http:\/\/localhost:8081\/wx/, 'boots calm and states the endpoint it is waiting for');
want(dead, /WARN wx live_unavailable .*flying=calm/, 'a dead endpoint is a warning');
want(dead, /INFO loading progress/, 'and the sim keeps running past it');
want(dead, /SMOKE_DONE/, 'no hang: the process reached its own deadline');
want(dead, /^(?!.*wx source source=live)/s, 'nothing was adopted');

const live = run('/wx serving the committed fixture', { WX_BLOB: FIXTURE });
want(live, /INFO wx live_ready bytes=8317984 run=2026-07-27T00:00:00Z .*gridStep=2 fields=20/, 'the blob is parsed and its GFS run named');
want(live, /INFO wx source source=live endpoint=\/wx/, 'and adopted at a frame boundary');
want(live, /SMOKE_DONE/, 'no hang');

console.log(fails === 0 ? 'wx_smoke: OK' : 'wx_smoke: ' + fails + ' FAILED');
process.exit(fails === 0 ? 0 : 1);
