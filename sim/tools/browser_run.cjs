/* EIN DEKLARIERTER LAUF IM BROWSER. Dieselbe Szene, dasselbe SceneRunner, dieselben Zahlen --
 * nur die Uebersetzung ist eine andere.
 *
 *   node tools/browser_run.cjs <mod> <scene> [timeoutS]
 *
 * `make wasm` und `sim/up.sh` muessen laufen. Die Produkte landen in sim/runs/, Log und Telemetrie
 * in sim/logs/ -- ein Lauf ist vollstaendig aus dem Serverlog nachvollziehbar.
 *
 * JEDE MESSUNG PINNT IHRE UEBERSETZUNG: wasm-Hash (fb-sim rechnet ihn ueber die Bytes, die es
 * ausliefert) UND Chromium-Version stehen in der Kopfzeile. Die Browserversion ist damit Teil der
 * Messung statt einer unsichtbaren Veraenderlichen.
 *
 * --enable-dawn-features=allow_unsafe_apis schaltet `timestamp-query` frei; ohne das Flag sagt die
 * Telemetriezeile `gpu=absent` statt Nullen zu melden. */
const { chromium } = require('playwright');

const MOD = process.argv[2] || 'demo';
const SCENE = process.argv[3] || 'frame';
const TIMEOUT_S = Number(process.argv[4] || 900);
const HOST = process.env.OUTSHINE_SIM || 'http://localhost:8080';

(async () => {
  const browser = await chromium.launch({
    args: ['--enable-unsafe-webgpu', '--use-angle=metal',
           '--enable-dawn-features=allow_unsafe_apis'] });
  const page = await browser.newPage({ viewport: { width: 1280, height: 720 } });
  const lines = [];
  page.on('console', m => lines.push(m.text()));
  page.on('pageerror', e => lines.push('PAGEERROR ' + e.message));
  await page.addInitScript(({ mod, scene }) => {
    window.FB_MOD = mod;
    window.FB_SCENE = scene;
  }, { mod: MOD, scene: SCENE });
  await page.goto(HOST + '/', { waitUntil: 'load' });

  const build = await page.evaluate(() => window.FB_BUILD || 'unset');
  console.log(`# mod=${MOD} scene=${SCENE} wasm=${build} chromium=${browser.version()}`);

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
