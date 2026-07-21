// Mission test runner — the fail-fast orchestrator. Resolves a mission, compiles it, flies it through the
// fast validator (real libJSBSim + iNav control verbatim), then evaluates the envelope + every task's verify
// predicates over the flown track, aborting on the FIRST violation with the failing check + measured value.
// A mission passes iff every phase completes, every predicate holds, and the envelope was never violated.

import { readFileSync, existsSync } from 'node:fs';
import { spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { resolveMission, groundElev, type Mission } from './mission.js';
import { inavParams, assertInavCoverage } from './ringtrack.js';
import { CAPTURE, CORRIDOR, ALT_BAND, SPEED_BAND, type Sample, type Track, range, type Point } from './predicates.js';

const SIM = resolve(import.meta.dirname, '../../..');
const VALIDATOR = process.env.VALIDATOR ?? `${SIM}/validator/validator`;
const MODELS_ROOT = process.env.MODELS_ROOT ?? 'aircraft/models';

/** Compact resolved-mission text the C validator reads on stdin (runways, iNav nav/throttle/PID/rate params,
 *  speed envelope, config, waypoints). Single source used by `cc vinput` and the runner. */
export function buildVinput(m: Mission): string {
  assertInavCoverage(m.aircraft);   // fail-fast if the aircraft's inav.diff has a setting the validator doesn't account for
  const p = inavParams(m.aircraft);
  const L: string[] = [];
  L.push(`ac ${m.aircraft}`);
  L.push(`to ${m.takeoff.lat} ${m.takeoff.lon} ${m.takeoff.elevM} ${m.takeoff.headingDeg}`);
  L.push(`ld ${m.land.lat} ${m.land.lon} ${m.land.elevM} ${m.land.headingDeg} ${m.land.lengthM}`);
  L.push(`nav ${p.climbAngle} ${p.diveAngle} ${p.bankAngle} ${p.cruise} ${p.approachLen} ${p.glideAngle}`);
  L.push(`thr ${p.cruiseThr} ${p.minThr} ${p.maxThr} ${p.pitch2thr}`);
  L.push(`pid ${p.posZp} ${p.posZi} ${p.posZd} ${p.posZff} ${p.altResponse} ${p.maxClimbRate} ${p.controlSmoothness}`);
  L.push(`posxy ${p.posXYp} ${p.posXYi} ${p.posXYd}`);                     // fw_nav heading->roll PID gains
  L.push(`spd ${m.vs} ${m.vc} ${m.vne} ${m.vmin}`);
  const captureR = m.raw.success?.capture_radius_m ?? m.raw.transit?.capture_radius_m ?? 150;
  const climbAlt = m.raw.procedure?.angle_hold_alt ?? 0;
  L.push(`cfg ${captureR} ${climbAlt}`);
  L.push(`gain ${p.fwPr ? 0 : 0} 0 0 0`);   // legacy gain slot (unused; inner loop uses rate PID)
  L.push(`rate ${p.fwPr} ${p.fwIr} ${p.fwDr} ${p.fwFfr} ${p.fwPp} ${p.fwIp} ${p.fwDp} ${p.fwFfp} ${p.pLevel} ${p.iLevel}`);
  L.push(`yaw ${p.fwPy} ${p.turnAssist}`);                                // yaw rate PID + turn-assist (coordinated turns)
  L.push(`launch ${p.launchClimbAngle} ${p.launchTimeout}`);             // NAV LAUNCH climb angle + timeout
  L.push(`loiter ${p.loiterRadius / 100} ${p.rthAltitude / 100}`);       // loiter radius (m) + RTH altitude (m)
  for (const w of m.waypoints) L.push(`wp ${w.lat} ${w.lon} ${w.altRel ?? 0}`);
  return L.join('\n') + '\n';
}

/** Landing metrics from the validator's LAND line: touchdown along/cross-track from the runway threshold,
 *  heading error vs the runway, touchdown groundspeed, rollout distance, runway length, and whether it
 *  actually rolled to a full STOP on the runway. */
export interface Landing { along: number; xte: number; hdgErr: number; gs: number; rollout: number; runwayLen: number; stopped: boolean; cas: number; sinkRate: number; }
export interface FlightResult { track: Track; verdict: string; ok: boolean; landing?: Landing; }

/** Fly the vinput through the validator (VOUT trajectory). CSV lines -> Track; LAND -> landing; OK/FAIL -> verdict.
 *  Run under `setarch -R` (ASLR disabled): the sim math is deterministic (CLAUDE.md Prinzip 5), but JSBSim's
 *  FGTrim takes an address-layout-dependent path when a trim does NOT converge (the SGS glider: "udot doesn't
 *  appear to be trimmable"), which made the SAME mission flip PASS/FAIL near a grading boundary. Fixing FGTrim
 *  is out (read-only submodule); pinning the layout makes the oracle reproducible, which is the requirement. */
export function flyValidator(vin: string): FlightResult {
  const setarch = existsSync('/usr/bin/setarch');
  const r = setarch
    ? spawnSync('setarch', ['-R', VALIDATOR], { input: vin, encoding: 'utf8', maxBuffer: 64 * 1024 * 1024, cwd: SIM, env: { ...process.env, MODELS_ROOT, VOUT: '1' } })
    : spawnSync(VALIDATOR, [], { input: vin, encoding: 'utf8', maxBuffer: 64 * 1024 * 1024, cwd: SIM, env: { ...process.env, MODELS_ROOT, VOUT: '1' } });
  const track: Track = []; let verdict = '', ok = false; let landing: Landing | undefined;
  for (const line of (r.stdout ?? '').split('\n')) {
    if (!line) continue;
    if (line.startsWith('LAND ')) { const f = line.split(' '); landing = { along: +f[1], xte: +f[2], hdgErr: +f[3], gs: +f[4], rollout: +f[5], runwayLen: +f[6], stopped: f[7] === '1', cas: +f[8], sinkRate: +f[9] }; continue; }
    if (line[0] === 'O' || line[0] === 'F') { verdict = line.trim(); ok = line.startsWith('OK'); continue; }
    const f = line.split(' '); if (f.length < 14 || !/^[0-9.]/.test(f[0])) continue;
    track.push({ t: +f[0], lat: +f[1], lon: +f[2], hAsl: +f[3], hAgl: +f[4], roll: +f[5], pitch: +f[6],
      yaw: +f[7], ias: +f[8], vs: +f[9], gs: +f[10], course: +f[11], wp: +f[12], phase: f[13] });
  }
  return { track, verdict, ok, landing };
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

  // per-WP CAPTURE in order (the p2p core): each waypoint reached within the grading tolerance, IN SEQUENCE.
  // The tolerance is DECOUPLED from how the validator advances legs — the validator uses iNav's own
  // isWaypointReached (bearing-swing passby, navigation.c), so this is an INDEPENDENT accuracy measurement,
  // not the same radius grading itself (which could never fail). The auto-appended FAF is labelled distinctly.
  // Between consecutive task waypoints the flight must also stay inside a CORRIDOR (no wandering off the leg).
  const capR = m.raw.success?.capture_radius_m ?? m.raw.transit?.capture_radius_m ?? 100;   // grading accuracy, not the nav advance trigger
  // Corridor half-width between consecutive task WPs (~2.5× the scaled airframes' ~200 m turn radius): a normal
  // turn stays inside it; it catches gross wandering, not turn geometry. NOT widened to paper over the SGS
  // glider's residual inner-loop oscillation — that is a real validator replication gap (see below), left honest.
  const corridorW = m.raw.success?.corridor_width_m ?? m.raw.transit?.corridor_width_m ?? 500;
  let from = 0, prevTaskIdx = 0; let prevP: Point | null = null;
  for (let i = 0; i < m.waypoints.length; i++) {
    const w = m.waypoints[i]; const P: Point = { lat: w.lat, lon: w.lon };
    const tag = w.faf ? 'FAF' : `wp${i + 1}`;                            // system-injected approach fix vs authored task WP
    const r = CAPTURE(fr.track.slice(from), P, capR);
    if (!push({ name: `capture:${tag}`, pass: r.pass, detail: r.detail })) return done();
    const capIdx = fr.track.findIndex((s, idx) => idx >= from && range(P, s) <= capR);
    // CORRIDOR: the leg from the previous TASK waypoint to this one stayed within the corridor (not the
    // approach leg to the FAF, which follows the localizer/glideslope geometry, not a straight task line).
    if (prevP && !w.faf && capIdx > prevTaskIdx) {
      const c = CORRIDOR(fr.track.slice(prevTaskIdx, capIdx + 1), prevP, P, corridorW);
      if (!push({ name: `corridor:wp${i + 1}`, pass: c.pass, detail: c.detail })) return done();
    }
    const spec2 = (m.raw.waypoints?.[i]) ?? {};
    if (capIdx >= 0 && spec2.alt_tol_m != null) {   // altitude held at the fix (terrain-correct AGL)
      const s = fr.track[capIdx]; const d = Math.abs(s.hAgl - w.altAgl);
      if (!push({ name: `alt:wp${i + 1}`, pass: d <= spec2.alt_tol_m, detail: d <= spec2.alt_tol_m ? `AGL ${s.hAgl.toFixed(0)}m ~ ${w.altAgl}±${spec2.alt_tol_m}m at capture` : `AGL ${s.hAgl.toFixed(0)}m off ${w.altAgl}±${spec2.alt_tol_m}m at capture` })) return done();
    }
    if (capIdx >= 0 && spec2.speed_tgt_ms != null) {   // speed on the leg to this fix
      const r2 = SPEED_BAND(fr.track.slice(from, capIdx + 1), spec2.speed_tgt_ms, spec2.speed_tol_ms ?? 6);
      if (!push({ name: `speed:wp${i + 1}`, pass: r2.pass, detail: r2.detail })) return done();
    }
    if (capIdx >= 0) { from = capIdx; if (!w.faf) { prevTaskIdx = capIdx; prevP = P; } }
  }

  // LANDING — a real landing, not a wheels-down moment: touched down ON the runway (aligned, on the strip,
  // in the touchdown zone), at an acceptable speed, and ROLLED TO A FULL STOP on the runway. (FORMAT.md §2)
  const td = m.raw.land ?? {}; const L = fr.landing;
  if (!L) { push({ name: 'landing', pass: false, detail: `no landing (${fr.verdict})` }); return done(); }
  const halfWidth = td.runway_half_width_m ?? 25;       // touched down on the strip, not off to the side
  const zoneEnd = L.runwayLen * (td.touchdown_zone_frac ?? 0.5);
  const hdgTol = td.align_tol_deg ?? 20;                // aligned + correct direction
  // iNav's rangefinder-less FW autoland does NOT flare — it holds a level-pitch, power-off GLIDE to ground
  // contact (sim-critic B1), so it touches down at ~the approach speed, well above a flared 1.3·Vs. 2.0·Vs is
  // what the c172's real autoland achieves; this is the HW truth, not a soft-landing ideal. A per-airframe
  // override still applies (a motor-glider with no spoilers + a high idle floor lands hotter still).
  const casMax = td.touchdown_cas_max_ms ?? m.vs * 2.0;
  const sinkMax = td.touchdown_sink_max_ms ?? 3.5;      // a level-pitch power-off glide to ground, not a flared arrival — firmer than a flared touchdown

  if (!push({ name: 'touchdown:on-runway', pass: Math.abs(L.xte) <= halfWidth && L.along >= -30 && L.along <= zoneEnd,
    detail: `along ${L.along.toFixed(0)}m (zone 0..${zoneEnd.toFixed(0)}), cross-track ${L.xte.toFixed(0)}m (±${halfWidth})` })) return done();
  if (!push({ name: 'touchdown:aligned', pass: Math.abs(L.hdgErr) <= hdgTol, detail: `heading ${L.hdgErr.toFixed(0)}° off runway (±${hdgTol}°)` })) return done();
  if (!push({ name: 'touchdown:speed', pass: L.cas <= casMax, detail: `touchdown CAS ${L.cas.toFixed(1)} ≤ ${casMax.toFixed(1)} m/s (Vs ${m.vs.toFixed(1)}); GS ${L.gs.toFixed(1)}` })) return done();
  if (!push({ name: 'touchdown:soft', pass: L.sinkRate <= sinkMax, detail: `sink rate ${L.sinkRate.toFixed(1)} ≤ ${sinkMax.toFixed(1)} m/s at touchdown` })) return done();
  push({ name: 'rollout:stopped-on-runway', pass: L.stopped && (L.along + L.rollout) <= L.runwayLen,
    detail: L.stopped ? `stopped after ${L.rollout.toFixed(0)}m rollout (touchdown ${L.along.toFixed(0)}m + roll ${L.rollout.toFixed(0)}m ≤ ${L.runwayLen.toFixed(0)}m runway)` : `never came to a stop on the runway (rolled ${L.rollout.toFixed(0)}m)` });

  return done();

  function done(): RunResult {
    return { mission: m.name || spec, aircraft: m.aircraft, pass: checks.every((c) => c.pass), checks, track: fr.track, verdict: fr.verdict };
  }
  void ALT_BAND;
}
