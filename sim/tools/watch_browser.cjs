/* The browser proof harness: a real Chromium on the deployed WASM app (`make wasm` + `sim/up.sh`),
 * console captured, stills and a video out. Not a build target; run it from sim/.
 *
 *   OUT=build/shots MOD=f22 MISSION=c01m04-silkworm-jungle DURATION=340 VIDEO=1 \
 *     SHOT='director CUT' node tools/watch_browser.cjs
 *
 * WHY IT EXISTS IN THIS FORM. A mark in WALL seconds cannot be aimed at anything: the mission clock
 * starts after an indeterminate tile load, so a 1.6 s fireball was caught in one run of five. The
 * client prints BOTH clocks on every line (`t=<mission second> …`, and the harness stamps arrival),
 * so `SIMMARKS=203` fires on the first line at or past mission second 203 whatever the load did — and
 * `SHOT=<regex>` needs no clock at all, because a `director CUT` line IS the interesting instant.
 *
 * Env: OUT MOD MISSION EXTRA(&view=director) DURATION MARKS SIMMARKS SHOT SHOTDELAY SHOTMAX KEYS
 *      VIDEO=1 UTC=<unix seconds, pins the sky through web/config.js> */
const { chromium } = require('playwright');
const fs = require('fs');
const path = require('path');

const OUT = process.env.OUT || 'build/shots';
const MOD = process.env.MOD || 'f22';
const MISSION = process.env.MISSION || 'c01m04-silkworm-jungle';
const MARKS = (process.env.MARKS || '').split(',').filter(Boolean).map(Number);
const SIMMARKS = (process.env.SIMMARKS || '').split(',').filter(Boolean).map(Number);
const SHOT = process.env.SHOT ? new RegExp(process.env.SHOT) : null;
const SHOTDELAY = Number(process.env.SHOTDELAY || 2500);
const SHOTMAX = Number(process.env.SHOTMAX || 4);
const EXTRA = process.env.EXTRA || '&view=director';
const VIDEO = process.env.VIDEO === '1';
const DURATION = Number(process.env.DURATION || 0);

(async () => {
  fs.mkdirSync(OUT, { recursive: true });
  const browser = await chromium.launch({
    channel: 'chromium',
    headless: true,
    args: ['--enable-unsafe-webgpu', '--enable-features=Vulkan,WebGPU',
           '--use-angle=metal', '--ignore-gpu-blocklist', '--enable-gpu'],
  });
  const ctx = await browser.newContext(Object.assign(
    { viewport: { width: 1280, height: 720 }, deviceScaleFactor: 1 },
    VIDEO ? { recordVideo: { dir: path.join(OUT, 'video'), size: { width: 1280, height: 720 } } } : {}));
  const page = await ctx.newPage();
  /* web/config.js pins FB_SIM_UTC = 0 and loads AFTER any init script, so the sky is only pinnable by
   * rewriting that one line on the wire. */
  if (process.env.UTC) await page.route('**/config.js', async (route) => {
    const r = await route.fetch();
    const body = (await r.text()).replace(/window\.FB_SIM_UTC\s*=\s*[^;]*;/,
                                          `window.FB_SIM_UTC = ${Number(process.env.UTC)};`);
    await route.fulfill({ response: r, body });
  });
  const log = [];
  const t0 = Date.now();
  let simT = -1;                 // last mission second the client printed
  const simSeen = [];            // [simSec, wallMs] pairs, for the drift table
  let shots = 0;
  let busy = false;
  const pending = [];

  const grab = async (tag) => {
    const f = path.join(OUT, `${MISSION}-${tag}.png`);
    try { await page.screenshot({ path: f }); log.push(`>> shot ${tag} wall=${((Date.now()-t0)/1000).toFixed(1)}s sim=${simT.toFixed(1)}s ${f}`); }
    catch (e) { log.push(`>> shot ${tag} FAILED ${e.message}`); }
  };
  const pump = async () => {
    if (busy) return;
    busy = true;
    while (pending.length) await grab(pending.shift());
    busy = false;
  };

  page.on('console', (m) => {
    const txt = m.text();
    const wall = Date.now() - t0;
    log.push(`[${wall}] ${txt}`);
    const mt = /^t=([0-9.]+) /.exec(txt);
    if (mt) {
      const s = Number(mt[1]);
      if (s > simT) { simT = s; simSeen.push([s, wall]); }
      for (let i = SIMMARKS.length - 1; i >= 0; i--)
        if (SIMMARKS[i] >= 0 && s >= SIMMARKS[i]) { pending.push(`s${SIMMARKS[i]}`); SIMMARKS[i] = -1; }
    }
    if (SHOT && SHOT.test(txt) && shots < SHOTMAX) {
      const n = ++shots;
      setTimeout(() => { pending.push(`ev${n}`); pump(); }, SHOTDELAY);
    }
    if (pending.length) pump();
  });
  page.on('pageerror', (e) => log.push(`[pageerror] ${e.message}`));

  const url = `http://localhost:8080/?mod=${MOD}&mission=${MISSION}${EXTRA}`;
  log.push(`>> ${url}`);
  await page.goto(url, { waitUntil: 'domcontentloaded' });

  const KEYS = (process.env.KEYS || '').split(',').filter(Boolean)
    .map((s) => { const i = s.indexOf(':'); return { t: Number(s.slice(0, i)), k: s.slice(i + 1) }; });
  for (const kp of KEYS)
    setTimeout(() => page.keyboard.press(kp.k).then(() => log.push(`>> key ${kp.k} @${kp.t}s`)).catch(()=>{}), kp.t * 1000);

  for (const m of MARKS) {
    const wait = m * 1000 - (Date.now() - t0);
    if (wait > 0) await page.waitForTimeout(wait);
    pending.push(`t${m}`);
    await pump();
  }
  const end = DURATION * 1000 - (Date.now() - t0);
  if (end > 0) await page.waitForTimeout(end);
  while (pending.length || busy) await page.waitForTimeout(200);

  /* THE TWO CLOCKS, as a table: what wall second each mission second landed on. */
  const drift = simSeen.filter((_, i) => i % 25 === 0)
    .map(([s, w]) => `sim=${s.toFixed(1)} wall=${(w / 1000).toFixed(1)}`);
  log.push('>> clocks ' + drift.join(' | '));
  fs.writeFileSync(path.join(OUT, `${MISSION}.log`), log.join('\n') + '\n');
  /* THIS page's own recording, asked of the page — reading the directory renamed every OTHER run's
   * video onto this one's name and destroyed two of them. */
  const vid = VIDEO ? page.video() : null;
  const src = vid ? await vid.path() : null;
  await ctx.close();
  if (src) {
    const dst = path.join(OUT, 'video', `${MISSION}.webm`);
    fs.renameSync(src, dst);
    log.push(`>> video ${dst}`);
    console.log(`>> video ${dst}`);
  }
  await browser.close();
  console.log(log.filter((l) => />>|director|mission RESULT|pageerror/.test(l)).slice(-60).join('\n'));
})();
