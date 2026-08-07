/* THE STEERED CLIENT'S GROUND SPEED, measured and not declared. It holds a movement key for a fixed
 * time and reads the two standpoints the client itself filed either side of the hold — the same `L`
 * an owner presses, the same shots.jsonl a reader tails. Distance over the CLIENT's own clock, so a
 * slow harness cannot flatter or spoil the number.
 *
 *   node tools/browser_walk.cjs [holdS] [settleMs] [--run]
 *
 * `make wasm` + `sim/up.sh` first; shots/ is the volume fb-sim appends to. */
const { chromium } = require('playwright');
const fs = require('fs');

const LOG = 'shots/shots.jsonl';
const HOLD = Number(process.argv[2] || 20);
const SETTLE = Number(process.argv[3] || 60000);
const RUN = process.argv.includes('--run');

function entries() {
  if (!fs.existsSync(LOG)) return [];
  return fs.readFileSync(LOG, 'utf8').split('\n').filter(l => l.trim()).map(l => JSON.parse(l));
}

/* Haversine on the WGS84 mean radius: over a few kilometres the ellipsoid correction is far below
 * the metre this measurement resolves. */
function metres(a, b) {
  const R = 6371008.8, d2r = Math.PI / 180;
  const p1 = a.lat * d2r, p2 = b.lat * d2r;
  const dp = (b.lat - a.lat) * d2r, dl = (b.lon - a.lon) * d2r;
  const h = Math.sin(dp / 2) ** 2 + Math.cos(p1) * Math.cos(p2) * Math.sin(dl / 2) ** 2;
  return 2 * R * Math.asin(Math.sqrt(h));
}

(async () => {
  const before = entries().length;
  const browser = await chromium.launch({ args: ['--enable-unsafe-webgpu', '--use-angle=metal'] });
  const page = await browser.newPage({ viewport: { width: 1280, height: 720 } });
  const log = [];
  page.on('console', m => log.push(m.text()));
  page.on('pageerror', e => log.push('PAGEERROR ' + e.message));
  await page.goto('http://localhost:8080/', { waitUntil: 'load' });
  await page.waitForTimeout(SETTLE);

  /* THE SHOTS BRACKET THE HOLD WITH NOTHING BETWEEN THEM AND THE KEY EVENTS: every millisecond of
   * padding is standing still inside a window the speed is divided by. */
  await page.keyboard.press('KeyL');
  if (RUN) await page.keyboard.down('Shift');
  await page.keyboard.down('KeyW');
  await page.waitForTimeout(HOLD * 1000);
  await page.keyboard.up('KeyW');
  if (RUN) await page.keyboard.up('Shift');
  await page.keyboard.press('KeyL');
  await page.waitForTimeout(3000);
  await browser.close();

  const all = entries();
  const fresh = all.slice(before);
  if (fresh.length < 2) {
    console.log('only ' + fresh.length + ' new entries — the client filed no pair, nothing to measure');
    process.exit(1);
  }
  const a = fresh[0], b = fresh[fresh.length - 1];
  const d = metres(a.camera, b.camera);
  const dt = (b.clockMs - a.clockMs) / 1000.0;
  console.log('%s  hold %s s   client clock between the two shots %s s',
              RUN ? 'RUN (shift)' : 'WALK', HOLD.toFixed(1), dt.toFixed(3));
  console.log('  %s -> %s', a.name, b.name);
  /* The hold is the controlled quantity; the interval is what the client's own clock says elapsed,
     and it is never shorter, so distance/interval is the LOWER bound on the same speed. */
  console.log('  distance %s m   speed %s m/s (over the hold)   %s m/s (over the interval)',
              d.toFixed(2), (d / HOLD).toFixed(3), (d / dt).toFixed(3));

  /* WHAT THE SPEED COSTS. `progress` is the geometry target cut's residency: if it leaves 1 the
     streaming did not keep up and the cut is coarser than the camera asked for, which is the only
     numeric handle on a hole there is from outside the renderer. */
  const num = (l, k) => { const m = l.match(new RegExp('\\b' + k + '=(-?[0-9.e+]+)')); return m ? Number(m[1]) : null; };
  const late = log.filter(l => / late /.test(l)).map(l => num(l, 'deltaMs')).filter(v => v !== null);
  const frames = log.filter(l => / frame /.test(l));
  const prog = frames.map(l => num(l, 'progress')).filter(v => v !== null);
  const tris = frames.map(l => num(l, 'triangles')).filter(v => v !== null);
  const draws = frames.map(l => num(l, 'draws')).filter(v => v !== null);
  const q = (v, p) => v.length ? v.slice().sort((x, y) => x - y)[Math.min(v.length - 1, Math.floor(p * v.length))] : NaN;
  console.log('  late frames (delta > 25 ms): ' + late.length +
              '   p50 ' + q(late, .5).toFixed(1) + ' ms  p95 ' + q(late, .95).toFixed(1) +
              ' ms  max ' + (late.length ? Math.max(...late).toFixed(1) : 'n/a') + ' ms');
  if (prog.length) console.log('  progress  min ' + Math.min(...prog).toFixed(3) + '  samples ' + prog.length);
  if (tris.length) console.log('  triangles min ' + Math.min(...tris) + '  max ' + Math.max(...tris) +
                               '   draws min ' + Math.min(...draws) + '  max ' + Math.max(...draws));
  for (const l of log.filter(l => /PAGEERROR|shot_failed|_failed|unresolved/.test(l))) console.log('  ' + l);
})();
