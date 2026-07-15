/* Measure the real command center in a real browser. Called by bench.sh.
 *
 * Shares shot.js's rules and for the same reasons: pinned chrome, --use-angle=swiftshader, and
 * NEVER --virtual-time-budget -- it fast-forwards the page's clock while the tile fetches run in
 * real seconds, so it takes streaming time away and then measures an empty world. A benchmark of
 * an empty world is worse than no benchmark: it is fast, reproducible and meaningless.
 *
 * WHAT IS MEASURED, AND WHAT WAS THROWN OUT AFTER MEASURING IT
 *
 * The obvious metric -- the gap between frames -- is NOT a performance metric here, and this file
 * deliberately records it only as a diagnostic. Measured on this box, at rest:
 *   frame interval p50 = 131 ms (7.5 fps), while the frame callback costs 0.5 ms and gl.finish()
 *   costs 0.00 ms (7 ms total across 12 s).
 * So ~130 of every 131 ms is the browser waiting, not the renderer working: headless chromium
 * paces an idle page at its own cadence and there is no GPU backlog behind it. The proof that this
 * number must not be read as "frame time": injecting 25 ms/frame of CPU work made the frame rate
 * go UP, 7.7 -> 14.0 fps, because a busy page gets ticked more often. A metric that improves when
 * you add work cannot be used to detect work getting cheaper.
 *
 * What survives, and what a renderer refactor should be judged on:
 *   1. FRAME CALLBACK DURATION -- wall time inside emscripten's main-loop callback. cc.c runs
 *      emscripten_set_main_loop(frame,0,1); fps=0 drives the loop from requestAnimationFrame, and
 *      a probe confirmed exactly ONE rAF consumer on the page (MainLoop_scheduler_rAF). So
 *      wrapping rAF in an init script sees every renderer frame and nothing else, with no hook
 *      inside command_center/. This is the CPU work an SoA/SIMD refactor would actually move.
 *   2. CDP Performance.getMetrics ScriptDuration/TaskDuration -- independent CPU accounting for
 *      the same work. Kept because it is independent: it is cross-checked against the sum of (1)
 *      in bench_report.py, and two metrics disagreeing is how we find out one of them is lying.
 *
 *   node bench.js OUT.json OUT.png osm|photo W H WARMUP_S MEASURE_S [SYNTH_LOAD_MS]
 *
 * SYNTH_LOAD_MS burns that many ms per frame in a rAF callback of our own -- the self-test.
 * If the CPU numbers do not move when it is set, this harness is printing decorative constants.
 * bench.sh runs it as a gate.
 */
const { chromium } = require('playwright');

const [outJson, outPng, ground, w, h, warmupS, measureS, synthMs] = process.argv.slice(2);
const warmup = parseInt(warmupS, 10) * 1000;
const measure = parseInt(measureS, 10) * 1000;
const synth = parseFloat(synthMs || '0');

/* Never hardwired to :8080. A baseline taken against "whatever is on the default port" is the
 * exact failure run-tests.sh already had once (it published :8080, the port a live stack held, and
 * measured the LIVE aircraft while printing its own model names). It happened again here: a stack
 * rebuilt from someone else's working tree took over :8080 mid-run, so a "baseline of master" was
 * quietly measuring an unrelated refactor. bench_stack.sh pins a commit onto its own port and
 * passes it in; the wasm hash below records WHICH artifact actually answered. */
const BASE = process.env.FB_BENCH_URL || 'http://localhost:8080';

/* Same teardown swallow as shot.js: the distro playwright throws "rimraf: callback function
 * required" out of close(), asynchronously, after every result is already collected. */
let finished = false, exitCode = 0;
process.on('uncaughtException', (e) => {
  if (finished) process.exit(exitCode);
  console.error('  bench.js: ' + e.message);
  process.exit(1);
});

/* The line world3d.h prints on every streamer transition. Ready = it drew something and waits for
 * nothing. Parsed for the counters too, so the benchmark and the readiness gate read one source. */
const QUAD = /quadtree: (\d+) chunks drawn \(budget (\d+)\), (\d+) wanted split, (\d+) waiting, (\d+) pending, (\d+) over budget \| levels ([\d/]+) \| baked (\d+), cached (\d+), evicted (\d+)/;

(async () => {
  const browser = await chromium.launch({
    executablePath: process.env.FB_CHROME,
    args: ['--no-sandbox', '--disable-gpu-sandbox',
           '--use-angle=swiftshader', '--enable-unsafe-swiftshader'],
  });
  const page = await browser.newPage({ viewport: { width: +w, height: +h } });

  /* Must run before the WASM loads, or emscripten grabs the unwrapped rAF and we measure nothing.
   * Recording stays off until reset() so the streaming frames -- which bake tiles and upload
   * textures, a different population entirely -- never enter the steady-state sample. */
  await page.addInitScript((synthMs) => {
    const raf = window.requestAnimationFrame.bind(window);
    const fb = { on: false, iv: [], dur: [], last: 0, gl: {}, glPerFrame: {} };
    window.__fb = fb;

    /* WHAT WORK IS THE RENDERER DOING, not just how long did it take.
     *
     * This is the sharper half of the harness and the reason it exists. The bug that started this
     * whole effort -- a texture LOD doing 2x malloc + stbi_load + glGenerateMipmap per chunk PER
     * FRAME, 1 fps -- is invisible to a time metric whose noise floor is tens of percent, and it
     * would have been buried anyway under SwiftShader's software raster. As a COUNT it is not
     * subtle: 256 generateMipmap calls in a frame that should issue zero. A counter with a hard
     * expected value beats a time with 80 % noise for answering "did this step break something".
     *
     * Counted from OUTSIDE command_center/ by wrapping the context that getContext hands back --
     * emscripten stores it as GLctx and calls GLctx.texImage2D(...) by name at call time, so the
     * wrapper sees every call the renderer makes, with no hook in the renderer. getContext is
     * patched on the prototype because the canvas does not exist yet when this init script runs.
     *
     * The steady-state expectation is a MEDIAN of zero, not an always-zero: the aircraft really
     * does cross tile boundaries mid-window and legitimately bakes then. Thrash shows up in p50,
     * a boundary crossing only in the max -- which is exactly the distinction a per-frame count
     * makes and a per-run total does not.
     *
     * generateMipmap is counted because it is the one call ONLY the tile path makes (world3d.h),
     * which is what lets bench_report.py tell a re-bake apart from the EVS codec's one legitimate
     * video upload per frame (cc.c fb_codec_upload, texImage2D with no mipmap). Counting
     * texImage2D alone conflates the two and alarms forever in EVS. */
    const COUNTED = ['texImage2D', 'texSubImage2D', 'compressedTexImage2D', 'generateMipmap',
                     'createTexture', 'deleteTexture', 'bufferData', 'bufferSubData',
                     'createBuffer', 'deleteBuffer', 'drawElements', 'drawArrays',
                     'compileShader', 'linkProgram'];
    const getCtx = HTMLCanvasElement.prototype.getContext;
    HTMLCanvasElement.prototype.getContext = function (type, ...rest) {
      const ctx = getCtx.call(this, type, ...rest);
      if (ctx && /webgl/.test(type) && !ctx.__fbWrapped) {
        ctx.__fbWrapped = true;
        for (const m of COUNTED) {
          if (typeof ctx[m] !== 'function') continue;
          const orig = ctx[m].bind(ctx);
          fb.gl[m] = 0;
          ctx[m] = function (...a) { fb.gl[m]++; return orig(...a); };
        }
      }
      return ctx;
    };

    window.__fb.reset = () => {
      fb.iv = []; fb.dur = []; fb.last = 0;
      fb.glPerFrame = {};
      for (const k of Object.keys(fb.gl)) fb.glPerFrame[k] = [];
      fb.on = true;
    };
    window.requestAnimationFrame = function (cb) {
      return raf(function (ts) {
        const t0 = performance.now();
        /* Measured against the previous callback's ENTRY, not against the rAF timestamp argument:
         * ts is the frame's nominal vsync time, not when our work started. Diagnostic only -- see
         * the header on why this is not a performance number. */
        if (fb.on && fb.last) fb.iv.push(t0 - fb.last);
        fb.last = t0;
        const before = fb.on ? Object.assign({}, fb.gl) : null;
        try { return cb(ts); }
        finally {
          if (fb.on) {
            fb.dur.push(performance.now() - t0);
            /* Per-frame deltas: the renderer's GL work attributable to THIS frame. */
            for (const k of Object.keys(fb.gl)) {
              (fb.glPerFrame[k] = fb.glPerFrame[k] || []).push(fb.gl[k] - (before[k] || 0));
            }
          }
        }
      });
    };
    /* Self-test load: a second, independent rAF consumer burning CPU on the main thread. It does
     * not touch the renderer -- it competes with it for the thread, as extra work would. */
    if (synthMs > 0) {
      const burn = () => {
        const end = performance.now() + synthMs;
        while (performance.now() < end) { /* deliberate: block the main thread */ }
        window.requestAnimationFrame(burn);
      };
      window.requestAnimationFrame(burn);
    }
  }, synth);

  let lastQuad = null, ready = false;
  page.on('console', (m) => {
    const g = QUAD.exec(m.text());
    if (!g) return;
    lastQuad = g;
    if (+g[1] > 0 && +g[5] === 0) ready = true;
  });

  const cdp = await page.context().newCDPSession(page);
  await cdp.send('Performance.enable');

  await page.goto(`${BASE}/?ground=${ground}`);

  const t0 = Date.now();
  while (!ready && Date.now() - t0 < warmup) await page.waitForTimeout(500);
  const streamedS = (Date.now() - t0) / 1000;

  /* Steady state starts here. Everything before this line is load, not frame cost. */
  await page.evaluate(() => window.__fb.reset());
  const m0 = await cdp.send('Performance.getMetrics');
  const wall0 = Date.now();

  await page.waitForTimeout(measure);

  const m1 = await cdp.send('Performance.getMetrics');
  const wallS = (Date.now() - wall0) / 1000;
  const s = await page.evaluate(() => ({ iv: window.__fb.iv, dur: window.__fb.dur,
                                         glPerFrame: window.__fb.glPerFrame }));

  await page.screenshot({ path: outPng });

  const met = (m, k) => (m.metrics.find((x) => x.name === k) || { value: 0 }).value;
  const d = (k) => met(m1, k) - met(m0, k);

  /* Fingerprint the thing we just measured, from the server that served it -- not from the git
   * working tree, which is a different question and was the one answered wrongly before. */
  const wasmSha = await page.evaluate(async (base) => {
    const b = await (await fetch(base + '/cc.wasm')).arrayBuffer();
    const d = await crypto.subtle.digest('SHA-256', b);
    return [...new Uint8Array(d)].map((x) => x.toString(16).padStart(2, '0')).join('');
  }, BASE);

  const out = {
    ground, width: +w, height: +h,
    url: BASE, wasm_sha256: wasmSha,
    ready, streamed_s: +streamedS.toFixed(1),
    measure_wall_s: +wallS.toFixed(2),
    synth_load_ms: synth,
    frames: s.dur.length,
    /* Raw and unaggregated: bench_report.py owns the statistics, so the percentile definition
     * lives in one place and can be re-checked against the sample it came from. */
    frame_cb_ms: s.dur,
    interval_ms: s.iv,
    /* Per-frame GL call counts. The regression net: noise-free, and they name the work rather
     * than timing it. See the addInitScript header. */
    gl_per_frame: s.glPerFrame,
    cdp: {
      TaskDuration: +d('TaskDuration').toFixed(3),
      ScriptDuration: +d('ScriptDuration').toFixed(3),
      LayoutDuration: +d('LayoutDuration').toFixed(3),
      RecalcStyleDuration: +d('RecalcStyleDuration').toFixed(3),
    },
    counters: lastQuad ? {
      drawn: +lastQuad[1], budget: +lastQuad[2], wanted_split: +lastQuad[3],
      waiting: +lastQuad[4], pending: +lastQuad[5], over_budget: +lastQuad[6],
      levels: lastQuad[7], baked: +lastQuad[8], cached: +lastQuad[9], evicted: +lastQuad[10],
    } : null,
  };
  require('fs').writeFileSync(outJson, JSON.stringify(out));

  if (!ready) console.log(`  NOT READY after ${streamedS}s — measured an unfinished world`);
  else console.log(`  streamed in ${streamedS}s, ${out.frames} frames in ${wallS}s`);

  finished = true; exitCode = ready ? 0 : 2;
  await browser.close();
  process.exit(exitCode);
})().catch((e) => { console.error('  bench.js: ' + e.message); process.exit(1); });
