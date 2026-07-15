/* Drive the real command center in a real browser and shoot it. Called by shot.sh.
 *
 * Why playwright and not `chrome --headless --screenshot`: that path needs a way to wait for the
 * tiles, and the only knob it has is --virtual-time-budget. That flag fast-forwards the PAGE's
 * clock -- 90 s of page time passes in about 1 s of wall clock -- while the tile fetches are real
 * round trips to fb-tiles in real seconds. So it does not give the streaming time, it TAKES it
 * away, and then shoots. The old comment here claimed the exact opposite ("gives the tile
 * streaming time to actually land. Wall-clock sleeps would race") and the result was two
 * screenshots of an empty sky that got read as a broken renderer. The renderer was fine.
 *
 * And we do not sleep either: we wait for the CONDITION the streamer itself publishes -- chunks
 * drawn, nothing pending. A sleep is a guess about the network; this is an observation of it, so
 * it is both faster on a warm cache and correct on a cold one.
 *
 *   node shot.js OUT.png osm|photo WIDTH HEIGHT TIMEOUT_SECONDS
 */
const { chromium } = require('playwright');

const [out, ground, w, h, secs] = process.argv.slice(2);
const timeout = parseInt(secs, 10) * 1000;

/* The distro playwright (/usr/share/nodejs/playwright-core) throws from its own teardown --
 * "rimraf: callback function required", a bundled-rimraf API mismatch inside close(), raised
 * asynchronously so no try/catch around our await can see it. It fires AFTER the shot is on disk
 * and the console lines are collected, i.e. after every result we came for. Without this handler
 * a perfectly good screenshot exits 1, and a check that reports red on success gets ignored
 * exactly as fast as one that reports green on failure.
 * Only swallowed once we are finished; a crash before that is still a real failure. */
let finished = false, exitCode = 0;
process.on('uncaughtException', (e) => {
  if (finished) process.exit(exitCode);
  console.error('  shot.js: ' + e.message);
  process.exit(1);
});

/* "quadtree: 128 chunks drawn (budget 128), ... 0 pending, ..." -- the streamer prints this on
 * every transition. Ready = it drew something and is waiting for nothing. */
const READY = /quadtree: (\d+) chunks drawn .*?, (\d+) pending/;

(async () => {
  const browser = await chromium.launch({
    /* Pinned, never playwright's own default: the installed playwright wants a chromium revision
     * it did not install (1.38 asks for chromium-1080, the box has 1228) and would tell us to run
     * `npx playwright install` -- a browser download to run a check that already has a browser.
     * The path is the one CLAUDE.md documents; FB_CHROME overrides it. */
    executablePath: process.env.FB_CHROME,
    /* Software GL: there is no GPU here. --use-angle=swiftshader, NOT --use-gl=swiftshader --
     * this chrome answers the latter with gl=none,angle=none, renders a blank white canvas, and
     * still exits 0 with a screenshot: a green-looking check of an empty page. */
    args: ['--no-sandbox', '--disable-gpu-sandbox',
           '--use-angle=swiftshader', '--enable-unsafe-swiftshader'],
  });
  const page = await browser.newPage({ viewport: { width: +w, height: +h } });

  let last = null, ready = false;
  page.on('console', (m) => {
    const t = m.text();
    const g = READY.exec(t);
    if (!g) return;
    last = t;
    if (+g[1] > 0 && +g[2] === 0) ready = true;
  });

  await page.goto(`http://localhost:8080/?ground=${ground}`);

  const t0 = Date.now();
  while (!ready && Date.now() - t0 < timeout) await page.waitForTimeout(500);

  await page.screenshot({ path: out });

  const secsWaited = ((Date.now() - t0) / 1000).toFixed(1);
  if (last) console.log(`  streamer: ${last.replace(/^\[world3d\] /, '')}`);
  /* Say so rather than shooting a half-streamed world and calling it a result. pngstat still runs
   * on it -- two independent opinions about the same pixels are the point. */
  if (ready) console.log(`  streamed in ${secsWaited}s`);
  else       console.log(`  NOT READY after ${secsWaited}s — the shot is of an unfinished world`);

  /* Everything we came for is on disk; from here the distro teardown bug is noise. */
  finished = true; exitCode = ready ? 0 : 2;
  await browser.close();
  process.exit(exitCode);
})().catch((e) => { console.error('  shot.js: ' + e.message); process.exit(1); });
