// FlightBox headless Command Center CLI — a client of the running flightbox hub.
//   cc watch                     stream telemetry to stdout (any number can watch in parallel)
//   cc arm                       arm and hold in ANGLE (hand-over to a human/other CC)
// Connects to the hub's MSP-proxy port (CC_HOST/CC_PORT, default 127.0.0.1:5766).

import { CC, sleep } from '../src/client.js';

async function main() {
  const cmd = process.argv[2] ?? 'watch';
  const cc = new CC();
  await cc.connect();

  if (cmd === 'watch') {
    for (;;) {
      await sleep(500);
      const t = cc.t;
      const age = t.updated ? Date.now() - t.updated : Infinity;
      const link = age < 2000 ? 'LIVE' : 'no-signal';
      process.stdout.write(
        `[${link}] arm=${t.armed ?? '-'} nav=${t.navState ?? '-'} wp=${t.navWp ?? '-'} ` +
        `agl=${fmt(t.aglM)} pos=${fmt(t.lat, 5)},${fmt(t.lon, 5)} ` +
        `rpy=${fmt(t.roll)}/${fmt(t.pitch)}/${fmt(t.yaw)} gs=${fmt(t.gs)} fix=${t.fix ?? '-'}/${t.sats ?? '-'}\n`);
    }
  } else if (cmd === 'arm') {
    // steady RC at ~30 Hz: ARM (AUX1) + ANGLE, neutral sticks. iNav needs a continuous RC stream.
    const t0 = Date.now();
    for (;;) {
      const ts = (Date.now() - t0) / 1000;
      const armEdge = ts % 3 >= 1.5 ? 2000 : 1000;                 // pulse ARM until it takes
      cc.rc([1500, 1500, 1000, 1500, cc.t.armed ? 2000 : armEdge, 2000, 1000, 1000]);
      await sleep(33);
    }
  } else {
    process.stderr.write(`unknown command ${JSON.stringify(cmd)}; try: watch | arm\n`);
    process.exit(2);
  }
}

const fmt = (v: number | undefined, d = 1) => (v === undefined ? '-' : v.toFixed(d));
main().catch((e) => { process.stderr.write(String(e?.message ?? e) + '\n'); process.exit(1); });
