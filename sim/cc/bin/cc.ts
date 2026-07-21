// FlightBox headless Command Center CLI — a client of the running flightbox hub, over ws://.../msp.
//   cc watch                     stream telemetry to stdout (any number can watch in parallel)
//   cc fly <mission|aircraft>    fly a mission against the running aircraft (arm -> WP -> land)
// Connects to CC_URL (default ws://127.0.0.1:8080/msp).

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { CC, sleep } from '../src/client.js';
import { flyMission } from '../src/harness.js';
import { resolveMission } from '../src/mission.js';
import { inavParams } from '../src/ringtrack.js';


const fmt = (v: number | undefined, d = 1) => (v === undefined ? '-' : v.toFixed(d));

/** Compact resolved-mission text for the C validator (stdin): runways, iNav nav params, speed envelope, WPs. */
function gains(ac: string): { kpR: number; kdR: number; kpP: number; kdP: number } {
  // inner attitude-tracker gains (surface per deg attitude-err / per deg/s body-rate). A relaxed-stability
  // FLCS airframe (F-16) needs gentle P + heavy damping or it over-rotates into a departure; profile.env
  // overrides the light-aircraft defaults.
  const txt = readFileSync(`${resolve(import.meta.dirname, '../../..')}/aircraft/models/${ac}/profile.env`, 'utf8');
  const g = (k: string, d: number) => { const m = txt.match(new RegExp(`^\\s*${k}\\s*=\\s*([0-9.]+)`, 'm')); return m ? Number(m[1]) : d; };
  return { kpR: g('FB_KP_ROLL', 0.03), kdR: g('FB_KD_ROLL', 0.01), kpP: g('FB_KP_PITCH', 0.055), kdP: g('FB_KD_PITCH', 0.018) };
}

async function vinput(spec: string): Promise<string> {
  const m = await resolveMission(spec);
  const p = inavParams(m.aircraft);
  const g = gains(m.aircraft);
  const L: string[] = [];
  L.push(`ac ${m.aircraft}`);
  L.push(`to ${m.takeoff.lat} ${m.takeoff.lon} ${m.takeoff.elevM} ${m.takeoff.headingDeg}`);
  L.push(`ld ${m.land.lat} ${m.land.lon} ${m.land.elevM} ${m.land.headingDeg}`);
  L.push(`nav ${p.climbAngle} ${p.diveAngle} ${p.bankAngle} ${p.cruise} ${p.approachLen} ${p.glideAngle}`);
  L.push(`thr ${p.cruiseThr} ${p.minThr} ${p.maxThr} ${p.pitch2thr}`);
  L.push(`pid ${p.posZp} ${p.posZi} ${p.posZd} ${p.posZff} ${p.altResponse} ${p.maxClimbRate}`);
  L.push(`spd ${m.vs} ${m.vc} ${m.vne} ${m.vmin}`);
  const captureR = m.raw.success?.capture_radius_m ?? 150;              // fast jets fly wide -> bigger fangradius
  const climbAlt = m.raw.procedure?.angle_hold_alt ?? 0;               // climb straight to here before WP nav (0 = brief)
  L.push(`cfg ${captureR} ${climbAlt}`);
  L.push(`gain ${g.kpR} ${g.kdR} ${g.kpP} ${g.kdP}`);
  for (const w of m.waypoints) L.push(`wp ${w.lat} ${w.lon} ${w.altRel ?? 0}`);
  return L.join('\n') + '\n';
}

async function main() {
  const cmd = process.argv[2] ?? 'watch';
  if (cmd === 'vinput') { process.stdout.write(await vinput(process.argv[3] ?? 'c172')); return; }
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
    const m = await resolveMission(spec);
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
