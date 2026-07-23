const { chromium } = require('playwright');
(async () => {
  const browser = await chromium.launch({ args: ['--no-sandbox','--enable-unsafe-webgpu','--enable-features=Vulkan'] });
  const page = await browser.newPage();
  let ready = false;
  page.on('console', m => { if (/device ready/.test(m.text())) ready = true; });
  await page.goto('http://localhost:8080/gpu.html', { waitUntil: 'domcontentloaded' });
  for (let i = 0; i < 100 && !ready; i++) await page.waitForTimeout(100);
  await page.waitForTimeout(150);   /* a couple of frames, before the ~frame-14 loss */
  await page.screenshot({ path: '/tmp/claude-1000/-home-cosmo-Git-flightbox/cc-shots/gpu_hdr_fast.png' });
  await browser.close();
})();
