const { chromium } = require('playwright');
(async () => {
  const browser = await chromium.launch({ args: ['--no-sandbox','--enable-unsafe-webgpu','--enable-features=Vulkan','--use-gl=swiftshader','--enable-webgl'] });
  const page = await browser.newPage();
  const lines = [];
  const errs = [];
  page.on('console', m => {
    const t = m.text();
    if (/\[agl\]/.test(t)) lines.push({t: Date.now(), text: t});
    if (/error|Error|ERROR|LOST|nan|NaN|Infinity/.test(t)) errs.push(t);
  });
  page.on('pageerror', e => errs.push('PAGEERROR: '+e.message));
  await page.goto('http://localhost:8080/', { waitUntil: 'domcontentloaded' });
  await page.waitForTimeout(400000);
  console.log('=== AGL LINES ('+lines.length+') ===');
  for (const l of lines) console.log(l.t, l.text);
  console.log('=== ERRS/WARN ('+errs.length+') ===');
  for (const e of errs.slice(0,60)) console.log(e);
  await browser.close();
})();
