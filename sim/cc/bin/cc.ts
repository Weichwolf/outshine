// FlightBox headless Command Center CLI — a client of the running flightbox hub, over ws://.../msp.
//   cc watch                     stream telemetry to stdout (any number can watch in parallel)
//   cc fly <mission|aircraft>    fly a mission against the running aircraft (arm -> WP -> land)
// Connects to CC_URL (default ws://127.0.0.1:8080/msp).

import { readdirSync, readFileSync, existsSync } from 'node:fs';
import { resolve } from 'node:path';
import { CC, sleep } from '../src/client.js';
import { flyMission } from '../src/harness.js';
import { resolveMission, runway } from '../src/mission.js';
import { buildVinput, runMission } from '../src/runner.js';

const fmt = (v: number | undefined, d = 1) => (v === undefined ? '-' : v.toFixed(d));

async function main() {
  const cmd = process.argv[2] ?? 'watch';
  if (cmd === 'vinput') { process.stdout.write(buildVinput(await resolveMission(process.argv[3] ?? 'c172'))); return; }
  if (cmd === 'origin') {   // takeoff-runway "lat lon heading" for a mission (spawn origin; no DEM needed)
    const SIM = resolve(import.meta.dirname, '../../..');
    const spec = process.argv[3] ?? 'c172';
    // SAME candidate resolution as resolveMission() — so flightbox spawns at exactly the runway the validator
    // and tests use (single source; a mismatch here would desync the spawn origin from the mission).
    const path = [spec, `${spec}.json`, `missions/${spec}`, `missions/${spec}.json`]
      .map((c) => resolve(SIM, c)).find((p) => existsSync(p));
    if (!path) throw new Error(`mission not found: ${spec}`);
    const m = JSON.parse(readFileSync(path, 'utf8'));
    const r = runway(m.takeoff.airport, m.takeoff.runway);
    process.stdout.write(`${r.lat} ${r.lon} ${r.heading_deg}\n`);
    return;
  }
  if (cmd === 'run') {
    // fail-fast mission test through the fast validator: envelope + per-task verify predicates
    const r = await runMission(process.argv[3] ?? 'c172');
    process.stdout.write(`\n>> ${r.pass ? 'PASS' : 'FAIL'}  ${r.mission}  [${r.aircraft}]  (${r.verdict})\n`);
    for (const c of r.checks) process.stdout.write(`   ${c.pass ? '✓' : '✗'} ${c.name}: ${c.detail}\n`);
    process.exit(r.pass ? 0 : 1);
  }
  if (cmd === 'run-all') {
    // batch: fly every declarative mission (missions/ + missions/world/) through the validator, fail-fast each
    const SIM = resolve(import.meta.dirname, '../../..');
    const base = readdirSync(`${SIM}/missions`).filter((f) => f.endsWith('.json')).map((f) => f.replace(/\.json$/, ''));
    let world: string[] = [];
    try { world = readdirSync(`${SIM}/missions/world`).filter((f) => f.endsWith('.json')).map((f) => `world/${f.replace(/\.json$/, '')}`); } catch { /* no world dir */ }
    const specs = process.argv[3] ? [process.argv[3]] : [...base.sort(), ...world.sort()];
    // The fast validator is a pre-filter for CONVENTIONAL airframes. The fast-jet F-16 is validated by
    // flightbox directly (the validator's simplified energy model can't fly it enroute — see validator.c
    // header). So f16 missions are RUN (their diagnostics printed) but reported as FLIGHTBOX-REQUIRED, not
    // counted in the validator gate — the gate is "every conventional mission passes", not silently red for f16.
    let pass = 0, gate = 0;
    const rows: string[] = [], fbox: string[] = [];
    for (const spec of specs) {
      try {
        const r = await runMission(spec);
        const isJet = r.aircraft === 'f16';
        const failed = r.checks.filter((c) => !c.pass).map((c) => c.name).join(',');
        if (isJet) { fbox.push(`FLIGHTBOX  ${spec.padEnd(24)} [${r.aircraft}]  (validator pre-filter n/a for fast-jet; ${r.pass ? 'passed anyway' : 'diag: ' + (failed || r.verdict)})`); continue; }
        gate++; if (r.pass) pass++;
        rows.push(`${r.pass ? 'PASS' : 'FAIL'}  ${spec.padEnd(24)} [${r.aircraft}]${r.pass ? '' : '  ✗ ' + failed}`);
      } catch (e) { gate++; rows.push(`ERROR ${spec.padEnd(24)} ${(e as Error).message}`); }
    }
    for (const row of rows) process.stdout.write(row + '\n');
    if (fbox.length) { process.stdout.write('\n-- flightbox-validated (not in the fast-validator gate) --\n'); for (const row of fbox) process.stdout.write(row + '\n'); }
    process.stdout.write(`\n== ${pass}/${gate} conventional PASS  (+${fbox.length} f16 flightbox-required) ==\n`);
    process.exit(pass === gate ? 0 : 1);
  }
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
