/* ONE DECLARED RUN IN THE BROWSER. Same scene, same SceneRunner, same numbers -- only the
 * translation is another one.
 *
 *   node tools/browser_run.cjs <mod> <scene> [timeoutS] [canvasWxH]
 *
 * THE WINDOW SIZE IS THE OUTPUT MEDIUM'S, not the picture's: what resolution is rendered is the
 * scene's own declaration. The canvas gets the finished picture scaled into it aspect-preserving,
 * and that this does not move the result is exactly the statement two runs at different `canvasWxH`
 * check.
 *
 * `make wasm` and `sim/up.sh` have to be running. The products land in sim/runs/, log and telemetry
 * in sim/logs/ -- a run is fully reconstructible from the server log alone.
 *
 * EVERY MEASUREMENT PINS ITS TRANSLATION: the wasm hash (fb-sim computes it over the bytes it
 * serves) AND the Chromium version stand in the header line, so the browser version is part of the
 * measurement instead of an invisible variable.
 *
 * --enable-dawn-features=allow_unsafe_apis unlocks `timestamp-query`; without the flag the telemetry
 * line says `gpu=absent` rather than reporting zeros. */
const { chromium } = require('playwright');

const MOD = process.argv[2] || 'demo';
const SCENE = process.argv[3] || 'frame';
const TIMEOUT_S = Number(process.argv[4] || 900);
const HOST = process.env.OUTSHINE_SIM || 'http://localhost:8080';
const [CW, CH] = (process.argv[5] || '1280x720').split('x').map(Number);

(async () => {
  const browser = await chromium.launch({
    args: ['--enable-unsafe-webgpu', '--use-angle=metal',
           '--enable-dawn-features=allow_unsafe_apis'] });
  const page = await browser.newPage({ viewport: { width: CW, height: CH } });
  const lines = [];
  page.on('console', m => lines.push(m.text()));
  page.on('pageerror', e => lines.push('PAGEERROR ' + e.message));
  await page.addInitScript(({ mod, scene }) => {
    window.FB_MOD = mod;
    window.FB_SCENE = scene;
  }, { mod: MOD, scene: SCENE });
  await page.goto(HOST + '/', { waitUntil: 'load' });

  const build = await page.evaluate(() => window.FB_BUILD || 'unset');
  console.log(`# mod=${MOD} scene=${SCENE} canvas=${CW}x${CH} wasm=${build} chromium=${browser.version()}`);

  const t0 = Date.now();
  let rc = null;
  while (rc === null && (Date.now() - t0) / 1000 < TIMEOUT_S) {
    await page.waitForTimeout(1000);
    rc = await page.evaluate(() => (window.FB_RUN_DONE === undefined ? null : window.FB_RUN_DONE));
  }
  const secs = ((Date.now() - t0) / 1000).toFixed(1);
  for (const l of lines) {
    if (!/ (run|outshine|forest|render) /.test(l)) continue;
    if (/DEBUG/.test(l)) continue;
    console.log(l);
  }
  console.log(rc === null ? `# TIMEOUT after ${secs}s` : `# rc=${rc} after ${secs}s`);
  await browser.close();
  process.exit(rc === null ? 2 : rc);
})();
