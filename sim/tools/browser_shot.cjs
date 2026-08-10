/* One frame and the console out of the deployed WASM app. `make wasm` + `sim/up.sh` first.
 *   node tools/browser_shot.cjs [out.png] [settleMs] [canvasWxH]
 *
 * canvasWxH is the WINDOW, not the picture: the scene declares what resolution is rendered, and the
 * canvas gets it scaled in aspect-preserving -- bars rather than distortion. */
const { chromium } = require('playwright');

const OUT = process.argv[2] || 'wasm-demo.png';
const SETTLE = Number(process.argv[3] || 45000);
const [CW, CH] = (process.argv[4] || '1280x720').split('x').map(Number);

(async () => {
  const browser = await chromium.launch({ args: ['--enable-unsafe-webgpu', '--use-angle=metal'] });
  const page = await browser.newPage({ viewport: { width: CW, height: CH } });
  const log = [];
  page.on('console', m => log.push(m.text()));
  page.on('pageerror', e => log.push('PAGEERROR ' + e.message));
  await page.goto('http://localhost:8080/', { waitUntil: 'load' });
  await page.waitForTimeout(SETTLE);
  await page.screenshot({ path: OUT });
  const seen = new Set();
  for (const l of log) {
    if (!/scene |stand |terrain |groundfield|error|ERROR|errs/i.test(l)) continue;
    const key = l.replace(/[0-9.eE+-]+/g, '#');
    if (seen.has(key)) continue;
    seen.add(key);
    console.log(l);
  }
  await browser.close();
})();
