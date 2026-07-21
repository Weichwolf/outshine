#!/usr/bin/env node
/* Playwright probe of the LIVE WASM Command Center at localhost:8080 while a mission flies.
 * Loads the real page (WASM renders the telemetry), and in the SAME browser context opens a second
 * WebSocket to /ws to decode the binary telem_packet_t stream the WASM itself consumes — so we confirm
 * BOTH that the renderer runs (canvas + screenshots) AND that live telemetry advances (seq, position,
 * altitude, flight mode) through the mission. Emits a JSON verdict on stdout; PNGs to the out dir.
 *
 *   node test/cc_probe.js [seconds] [label] [url]
 *
 * telem_packet_t (common/protocol.h): <I magic> + 20 floats + <B state><B rssi><H seq> = 88 bytes.
 * floats: roll pitch yaw alt x y gs batt home_dist home_bearing glideslope_err cloud vis
 *         sun_el sun_az moon_el moon_az moon_phase vs airspeed
 */
const { chromium } = require('playwright');
const fs = require('fs');

const SECS  = parseFloat(process.argv[2] || '30');
const LABEL = process.argv[3] || 'probe';
const URL   = process.argv[4] || 'http://localhost:8080/';
const OUT   = `/tmp/claude-1000/-home-cosmo-Git-flightbox/cc-shots`;
const ST = ['DISARMED','ARMED','CLIMB','LOITER','MANUAL','RTH'];

(async () => {
  fs.mkdirSync(OUT, { recursive: true });
  // The bundled browser revision for this playwright build may be absent; a newer chromium is present.
  const CHROME = process.env.CC_CHROME || `${process.env.HOME}/.cache/ms-playwright/chromium-1228/chrome-linux64/chrome`;
  const browser = await chromium.launch({
    executablePath: fs.existsSync(CHROME) ? CHROME : undefined,
    args: ['--no-sandbox','--disable-gpu','--use-gl=angle','--use-angle=swiftshader'],
  });
  const page = await browser.newPage({ viewport: { width: 1280, height: 720 } });
  const errors = [];
  page.on('pageerror', e => errors.push(String(e)));
  page.on('console', m => { if (m.type()==='error') errors.push('console:'+m.text()); });

  await page.goto(URL, { waitUntil: 'domcontentloaded' });
  // Wait for the WASM runtime to actually start (Module.calledRun) — proves the CC booted.
  let booted = false;
  try { await page.waitForFunction(() => window.Module && window.Module.calledRun === true, { timeout: 20000 }); booted = true; } catch(e) {}

  // Tap /ws in page context and accumulate decoded telemetry for the whole window.
  await page.evaluate(() => {
    window.__telem = [];
    const proto = location.protocol === 'https:' ? 'wss' : 'ws';
    const ws = new WebSocket(`${proto}://${location.host}/ws`);
    ws.binaryType = 'arraybuffer';
    ws.onmessage = (ev) => {
      if (!(ev.data instanceof ArrayBuffer) || ev.data.byteLength !== 88) return;
      const dv = new DataView(ev.data);
      if (dv.getUint32(0, true) !== 0x314D4C54) return; // 'TLM1'
      const f = i => dv.getFloat32(4 + i*4, true);
      window.__telem.push({
        roll:f(0), pitch:f(1), yaw:f(2), alt:f(3), x:f(4), y:f(5), gs:f(6),
        vs:f(18), airspeed:f(19), state:dv.getUint8(84), rssi:dv.getUint8(85), seq:dv.getUint16(86, true)
      });
    };
    window.__ws = ws;
  });

  const shots = [];
  const nShots = Math.min(6, Math.max(3, Math.round(SECS/8)));
  for (let i = 0; i < nShots; i++) {
    await page.waitForTimeout((SECS*1000)/nShots);
    const p = `${OUT}/${LABEL}_${i}.png`;
    await page.screenshot({ path: p });
    shots.push(p);
  }

  const t = await page.evaluate(() => window.__telem);

  // Verdict
  const nan = t.filter(p => [p.roll,p.pitch,p.alt,p.x,p.y].some(v => !Number.isFinite(v))).length;
  const alt = t.map(p=>p.alt), xs = t.map(p=>p.x), ys = t.map(p=>p.y);
  const rng = a => a.length ? [Math.min(...a), Math.max(...a)] : [null,null];
  const dist = t.length ? Math.hypot(t[t.length-1].x - t[0].x, t[t.length-1].y - t[0].y) : 0;
  const seqAdv = t.length>1 ? t[t.length-1].seq !== t[0].seq : false;
  const verdict = {
    label: LABEL, booted, packets: t.length, seqAdvancing: seqAdv,
    states: [...new Set(t.map(p => ST[p.state] ?? p.state))],
    altRange: rng(alt).map(v=>v==null?null:+v.toFixed(1)),
    xRange: rng(xs).map(v=>v==null?null:+v.toFixed(0)), yRange: rng(ys).map(v=>v==null?null:+v.toFixed(0)),
    groundTrackMoved_m: +dist.toFixed(0), nanPackets: nan,
    pageErrors: errors.slice(0,5), shots,
  };
  console.log(JSON.stringify(verdict, null, 2));
  process.exit(0);   // bypass the system playwright-core's buggy async temp-dir cleanup on close
})().catch(e => { console.error('PROBE_ERR', String(e).slice(0,300)); process.exit(1); });
