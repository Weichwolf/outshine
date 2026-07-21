// FlightBox headless Command Center CLI — a client of the running flightbox hub, over ws://.../msp.
//   cc watch                     stream telemetry to stdout (any number can watch in parallel)
//   cc fly <mission|aircraft>    fly a mission against the running aircraft (arm -> WP -> land)
// Connects to CC_URL (default ws://127.0.0.1:8080/msp).

import { CC, sleep } from '../src/client.js';
import { flyMission } from '../src/harness.js';
import { resolveMission } from '../src/mission.js';

const fmt = (v: number | undefined, d = 1) => (v === undefined ? '-' : v.toFixed(d));

async function main() {
  const cmd = process.argv[2] ?? 'watch';
  const cc = new CC();
  await cc.connect();

  if (cmd === 'watch') {
    for (;;) {
      await sleep(500);
      const t = cc.t;
      const link = t.updated && Date.now() - t.updated < 2000 ? 'LIVE' : 'no-signal';
      process.stdout.write(
        `[${link}] arm=${t.armed ?? '-'} nav=${t.navState ?? '-'} wp=${t.navWp ?? '-'} ` +
        `agl=${fmt(t.aglM)} pos=${fmt(t.lat, 5)},${fmt(t.lon, 5)} ` +
        `rpy=${fmt(t.roll)}/${fmt(t.pitch)}/${fmt(t.yaw)} gs=${fmt(t.gs)} fix=${t.fix ?? '-'}/${t.sats ?? '-'}\n`);
    }
  } else if (cmd === 'fly') {
    const spec = process.argv[3] ?? 'c172';
    const path = spec.endsWith('.json') ? spec : `missions/${spec}.json`;
    const m = resolveMission(path);
    process.stdout.write(`>> ${m.name || m.aircraft}: ${m.takeoff.icao}/${m.takeoff.runway} -> ${m.waypoints.length} WP -> ${m.land.icao}/${m.land.runway}\n`);
    const proc = m.raw.procedure ?? {};
    const secs = (m.raw.abort?.timeout_s ?? 600);
    let last = '';
    await flyMission(cc, m, secs * 1000, {
      angleHoldAltM: proc.angle_hold_alt ?? 0,
      climbPitch: proc.angle_climb_pitch ?? 1500,
      autoland: true,
      onTick: (t, phase) => {
        const line = `${phase} arm=${t.armed} nav=${t.navState} wp=${t.navWp} agl=${fmt(t.aglM)} gs=${fmt(t.gs)}`;
        if (line !== last) { process.stdout.write(line + '\n'); last = line; }
      },
    });
    process.stdout.write('>> flight window ended\n');
    cc.close();
  } else {
    process.stderr.write(`unknown command ${JSON.stringify(cmd)}; try: watch | fly <mission>\n`);
    process.exit(2);
  }
}

main().catch((e) => { process.stderr.write(String(e?.message ?? e) + '\n'); process.exit(1); });
