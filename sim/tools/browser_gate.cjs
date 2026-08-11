/* A GATE THAT HAS TO RUN ON THE TARGET. Some statements about wasm32 are only true there — a
 * counter's width is one — and a gate that decides them natively decides nothing. This loads one
 * page in the same Chromium every measurement in this archive is taken in, and passes when the
 * program prints "GATE PASS" and exits 0.
 *
 *   node tools/browser_gate.cjs <path> [timeoutS]
 *
 * `sim/up.sh` has to be running; the page is served out of the live sim/web mount. */
const { chromium } = require('playwright');

const PATH_ = process.argv[2] || '/counters.html';
const TIMEOUT_S = Number(process.argv[3] || 1800);
const HOST = process.env.OUTSHINE_SIM || 'http://localhost:8080';

(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage();
  const lines = [];
  page.on('console', m => { const t = m.text(); lines.push(t); if (t.startsWith('GATE')) console.log(t); });
  page.on('pageerror', e => lines.push('PAGEERROR ' + e.message));
  console.log('browser=Chromium/' + browser.version() + ' page=' + HOST + PATH_);
  await page.goto(HOST + PATH_, { waitUntil: 'load' });
  const deadline = Date.now() + TIMEOUT_S * 1000;
  let verdict = null;
  while (Date.now() < deadline && verdict === null) {
    await page.waitForTimeout(2000);
    for (const l of lines) if (l === 'GATE PASS' || l === 'GATE FAIL') verdict = l;
    if (lines.some(l => l.startsWith('GATE FAIL '))) verdict = 'GATE FAIL';
  }
  await browser.close();
  if (verdict !== 'GATE PASS') {
    console.error(lines.slice(-30).join('\n'));
    console.error('browser_gate: ' + (verdict || 'no verdict before the deadline'));
    process.exit(1);
  }
})();
