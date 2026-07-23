const { chromium } = require('playwright');
(async () => {
  const browser = await chromium.launch({ executablePath: "/usr/bin/chromium", args: ['--no-sandbox','--enable-unsafe-webgpu','--enable-features=Vulkan'] });
  const page = await browser.newPage();
  const logs = [];
  page.on('console', m => logs.push(m.text()));
  await page.goto('http://localhost:8080/gpu.html', { waitUntil: 'domcontentloaded' });
  await page.waitForTimeout(12000);
  await page.screenshot({ path: '/tmp/claude-1000/-home-cosmo-Git-flightbox/cc-shots/gpu_new_pw.png' });
  console.log(logs.filter(l => /FBRenderer|LOST/.test(l)).join('\n'));
  await browser.close();
})();
