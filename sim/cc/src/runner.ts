// Mission test runner — the fail-fast orchestrator. Resolves a mission, compiles it, flies it through the
// fast validator (real libJSBSim + iNav control verbatim), then evaluates the envelope + every task's verify
// predicates over the flown track, aborting on the FIRST violation with the failing check + measured value.
// A mission passes iff every phase completes, every predicate holds, and the envelope was never violated.

import { readFileSync } from 'node:fs';
import { spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { resolveMission, groundElev, type Mission } from './mission.js';
import { inavParams } from './ringtrack.js';
import { CAPTURE, ALT_BAND, SPEED_BAND, type Sample, type Track, range, type Point } from './predicates.js';

const SIM = resolve(import.meta.dirname, '../../..');
const VALIDATOR = process.env.VALIDATOR ?? `${SIM}/validator/validator`;
const MODELS_ROOT = process.env.MODELS_ROOT ?? 'aircraft/models';

/** Compact resolved-mission text the C validator reads on stdin (runways, iNav nav/throttle/PID/rate params,
 *  speed envelope, config, waypoints). Single source used by `cc vinput` and the runner. */
export function buildVinput(m: Mission): string {
  const p = inavParams(m.aircraft);
  const L: string[] = [];
  L.push(`ac ${m.aircraft}`);
  L.push(`to ${m.takeoff.lat} ${m.takeoff.lon} ${m.takeoff.elevM} ${m.takeoff.headingDeg}`);
  L.push(`ld ${m.land.lat} ${m.land.lon} ${m.land.elevM} ${m.land.headingDeg}`);
  L.push(`nav ${p.climbAngle} ${p.diveAngle} ${p.bankAngle} ${p.cruise} ${p.approachLen} ${p.glideAngle}`);
  L.push(`thr ${p.cruiseThr} ${p.minThr} ${p.maxThr} ${p.pitch2thr}`);
  L.push(`pid ${p.posZp} ${p.posZi} ${p.posZd} ${p.posZff} ${p.altResponse} ${p.maxClimbRate} ${p.controlSmoothness}`);
  L.push(`spd ${m.vs} ${m.vc} ${m.vne} ${m.vmin}`);
  const captureR = m.raw.success?.capture_radius_m ?? m.raw.transit?.capture_radius_m ?? 150;
  const climbAlt = m.raw.procedure?.angle_hold_alt ?? 0;
  L.push(`cfg ${captureR} ${climbAlt}`);
  L.push(`gain ${p.fwPr ? 0 : 0} 0 0 0`);   // legacy gain slot (unused; inner loop uses rate PID)
  L.push(`rate ${p.fwPr} ${p.fwIr} ${p.fwDr} ${p.fwFfr} ${p.fwPp} ${p.fwIp} ${p.fwDp} ${p.fwFfp} ${p.pLevel} ${p.iLevel}`);
  for (const w of m.waypoints) L.push(`wp ${w.lat} ${w.lon} ${w.altRel ?? 0}`);
  return L.join('\n') + '\n';
}

export interface FlightResult { track: Track; verdict: string; ok: boolean; }

/** Fly the vinput through the validator (VOUT trajectory). CSV lines -> Track; the OK/FAIL line -> verdict. */
export function flyValidator(vin: string): FlightResult {
  const r = spawnSync(VALIDATOR, [], { input: vin, encoding: 'utf8', maxBuffer: 64 * 1024 * 1024,
    cwd: SIM, env: { ...process.env, MODELS_ROOT, VOUT: '1' } });
  const track: Track = []; let verdict = '', ok = false;
  for (const line of (r.stdout ?? '').split('\n')) {
    if (!line) continue;
    if (line[0] === 'O' || line[0] === 'F') { verdict = line.trim(); ok = line.startsWith('OK'); continue; }
    const f = line.split(' '); if (f.length < 14 || !/^[0-9.]/.test(f[0])) continue;
    track.push({ t: +f[0], lat: +f[1], lon: +f[2], hAsl: +f[3], hAgl: +f[4], roll: +f[5], pitch: +f[6],
      yaw: +f[7], ias: +f[8], vs: +f[9], gs: +f[10], course: +f[11], wp: +f[12], phase: f[13] });
  }
  return { track, verdict, ok };
}

/** Terrain-correct AGL host-side (FORMAT.md §1: "Terrain lives in the CC"). The validator flies with a flat
 *  ground (airport elev — correct for gear contact at the field; enroute the ground value is dynamics-moot),
 *  and reports true GPS h_asl; AGL for the predicates = h_asl − DEM(lat,lon). DEM sampled on a ~110 m grid
 *  (deduped) so a 10 Hz track over a 12 km path is ~100 lookups, not thousands. */
async function fillAgl(track: Track, tilesUrl: string): Promise<void> {
  const key = (s: { lat: number; lon: number }) => `${s.lat.toFixed(3)},${s.lon.toFixed(3)}`;
  const uniq = new Map<string, { lat: number; lon: number }>();
  for (const s of track) if (!uniq.has(key(s))) uniq.set(key(s), { lat: s.lat, lon: s.lon });
  const dem = new Map<string, number | null>();
  await Promise.all([...uniq].map(async ([k, p]) => dem.set(k, await groundElev(p.lat, p.lon, tilesUrl))));
  for (const s of track) { const g = dem.get(key(s)); if (g != null) s.hAgl = s.hAsl - g; }
}

export interface Check { name: string; pass: boolean; detail: string; }
export interface RunResult { mission: string; aircraft: string; pass: boolean; checks: Check[]; track: Track; verdict: string; }

interface Envelope { vMin: number; vMax: number; bankMax: number; pitchMax: number; altFloor: number; altCeil: number; }
function envelopeOf(m: Mission): Envelope {
  const e = m.raw.envelope ?? {};
  return {
    vMin: e.v_min_ms ?? m.vs * 0.95, vMax: e.v_max_ms ?? m.vne,
    bankMax: e.bank_max_deg ?? m.raw.envelope?.bank_max_deg ?? 60,
    pitchMax: e.pitch_max_deg ?? 40,
    altFloor: e.alt_floor_agl_m ?? -5, altCeil: e.alt_ceil_agl_m ?? 1e9,
  };
}

/** Envelope enforced ∀t (fail-fast): IAS band, bank/pitch limits, AGL floor/ceiling. */
function checkEnvelope(track: Track, env: Envelope): Check {
  for (const s of track) {
    const airborne = s.hAgl > 8;
    const enroute = s.phase === 'task';                    // floor/ceiling apply enroute, not during takeoff/landing
    if (airborne && s.ias < env.vMin) return { name: 'envelope:v_min', pass: false, detail: `IAS ${s.ias.toFixed(1)} < ${env.vMin} at t=${s.t.toFixed(0)}s` };
    if (s.ias > env.vMax) return { name: 'envelope:v_max', pass: false, detail: `IAS ${s.ias.toFixed(1)} > ${env.vMax} at t=${s.t.toFixed(0)}s` };
    if (airborne && Math.abs(s.roll) > env.bankMax) return { name: 'envelope:bank', pass: false, detail: `bank ${s.roll.toFixed(0)}° > ${env.bankMax}° at t=${s.t.toFixed(0)}s` };
    if (airborne && Math.abs(s.pitch) > env.pitchMax) return { name: 'envelope:pitch', pass: false, detail: `pitch ${s.pitch.toFixed(0)}° > ${env.pitchMax}° at t=${s.t.toFixed(0)}s` };
    if (enroute && s.hAgl < env.altFloor) return { name: 'envelope:alt_floor', pass: false, detail: `AGL ${s.hAgl.toFixed(0)}m < ${env.altFloor}m at t=${s.t.toFixed(0)}s (enroute)` };
    if (enroute && s.hAgl > env.altCeil) return { name: 'envelope:alt_ceil', pass: false, detail: `AGL ${s.hAgl.toFixed(0)}m > ${env.altCeil}m at t=${s.t.toFixed(0)}s` };
  }
  return { name: 'envelope', pass: true, detail: 'never violated' };
}

/** Run a p2p mission through the validator + evaluate (envelope, per-WP CAPTURE in order, touchdown). Fail-fast. */
export async function runMission(spec: string): Promise<RunResult> {
  const m = await resolveMission(spec);
  const fr = flyValidator(buildVinput(m));
  const checks: Check[] = [];
  const push = (c: Check) => { checks.push(c); return c.pass; };

  if (!fr.track.length) { push({ name: 'flight', pass: false, detail: `no trajectory (${fr.verdict})` }); return done(); }
  await fillAgl(fr.track, process.env.TILES_URL ?? 'http://localhost:8081');   // terrain-correct AGL from the DEM

  const env = envelopeOf(m);
  if (!push(checkEnvelope(fr.track, env))) return done();

  // per-WP CAPTURE in order (the p2p core): each waypoint reached within the capture radius, IN SEQUENCE,
  // and — where the mission specifies a band — at the intended altitude/speed when captured (phase-local).
  const capR = m.raw.success?.capture_radius_m ?? m.raw.transit?.capture_radius_m ?? 150;
  let from = 0;
  for (let i = 0; i < m.waypoints.length; i++) {
    const w = m.waypoints[i]; const P: Point = { lat: w.lat, lon: w.lon };
    const r = CAPTURE(fr.track.slice(from), P, capR);
    if (!push({ name: `capture:wp${i + 1}`, pass: r.pass, detail: r.detail })) return done();
    const capIdx = fr.track.findIndex((s, idx) => idx >= from && range(P, s) <= capR);
    const spec2 = (m.raw.waypoints?.[i]) ?? {};
    if (capIdx >= 0 && spec2.alt_tol_m != null) {   // altitude held at the fix (terrain-correct AGL)
      const s = fr.track[capIdx]; const d = Math.abs(s.hAgl - w.altAgl);
      if (!push({ name: `alt:wp${i + 1}`, pass: d <= spec2.alt_tol_m, detail: d <= spec2.alt_tol_m ? `AGL ${s.hAgl.toFixed(0)}m ~ ${w.altAgl}±${spec2.alt_tol_m}m at capture` : `AGL ${s.hAgl.toFixed(0)}m off ${w.altAgl}±${spec2.alt_tol_m}m at capture` })) return done();
    }
    if (capIdx >= 0 && spec2.speed_tgt_ms != null) {   // speed on the leg to this fix
      const r2 = SPEED_BAND(fr.track.slice(from, capIdx + 1), spec2.speed_tgt_ms, spec2.speed_tol_ms ?? 6);
      if (!push({ name: `speed:wp${i + 1}`, pass: r2.pass, detail: r2.detail })) return done();
    }
    from = capIdx >= 0 ? capIdx : from;
  }

  // touchdown: reached the landing threshold, on the ground, near the aim point
  const td = m.raw.land ?? {}; const tdTol = td.touchdown_tol_m ?? 200;
  const P: Point = { lat: m.land.lat, lon: m.land.lon };
  // touchdown = validator reached the ground near the threshold; VOUT samples @10 Hz so the very last
  // low-AGL land-phase sample is the flare, within a few m of the deck (< 10 m AGL).
  const onGround = fr.track.filter((s) => s.hAgl < 10 && s.phase === 'land');
  const best = onGround.reduce((b, s) => Math.min(b, range(P, s)), Infinity);
  push({ name: 'touchdown', pass: fr.ok && best <= tdTol, detail: best <= tdTol ? `down ${best.toFixed(0)}m ≤ ${tdTol}m from threshold` : `no touchdown within ${tdTol}m (closest ${isFinite(best) ? best.toFixed(0) + 'm' : 'never on final'})` });

  return done();

  function done(): RunResult {
    return { mission: m.name || spec, aircraft: m.aircraft, pass: checks.every((c) => c.pass), checks, track: fr.track, verdict: fr.verdict };
  }
  void ALT_BAND;
}
