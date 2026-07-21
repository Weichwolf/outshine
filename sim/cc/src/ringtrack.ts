// Ring-track generator + scorer, native TypeScript. From a resolved mission this builds the dense DIRECTED
// corridor the aircraft must thread, from iNav's OWN nav math (climb-out at nav_fw_climb_angle, straight
// enroute legs joined at the coordinated turn radius, an approach glideslope aligned to the landing runway).
//
// Two gate kinds — pick by whether DIRECTION matters:
//   * ring   — a directed hoop (plane normal = flight tangent). "Passed" = the track pierces the plane
//              from the correct side within the radius. Use where the crossing DIRECTION is relevant
//              (climb corridor, straight legs, glideslope).
//   * sphere — a position ball. "Passed" = the track's closest approach comes within the radius, ANY
//              direction. Use where only REACHING the position matters (a waypoint to visit, a target).

import { readFileSync, existsSync } from 'node:fs';
import { resolve } from 'node:path';
import type { Mission } from './mission.js';

const G = 9.80665;
const SIM = resolve(import.meta.dirname, '../../..');
const D2R = Math.PI / 180, R2D = 180 / Math.PI;

const clat = (lat: number) => Math.cos(lat * D2R);
const enuOff = (lat: number, lon: number, clat0: number, clon0: number): [number, number] =>
  [(lon - clon0) * 111320 * clat(clat0), (lat - clat0) * 111320];
const offLL = (clat0: number, clon0: number, e: number, n: number): [number, number] =>
  [clat0 + n / 111320, clon0 + e / (111320 * clat(clat0))];
export function bearing(a: number, b: number, c: number, d: number): number {
  const y = Math.sin((d - b) * D2R) * Math.cos(c * D2R);
  const x = Math.cos(a * D2R) * Math.sin(c * D2R) - Math.sin(a * D2R) * Math.cos(c * D2R) * Math.cos((d - b) * D2R);
  return (Math.atan2(y, x) * R2D + 360) % 360;
}
const dest = (lat: number, lon: number, brg: number, dist: number): [number, number] => {
  const br = brg * D2R; return offLL(lat, lon, Math.sin(br) * dist, Math.cos(br) * dist);
};
const distLL = (a: [number, number], b: [number, number]): number => {
  const [e, n] = enuOff(b[0], b[1], a[0], a[1]); return Math.hypot(e, n);
};

// iNav fixed-wing nav params (defaults = iNav settings.yaml default_value; overridden per aircraft by
// inav.diff). The C validator flies iNav's OWN control cascade from these, so its trajectory is iNav's.
export interface NavParams {
  climbAngle: number; diveAngle: number; bankAngle: number; cruise: number; approachLen: number; glideAngle: number;
  cruiseThr: number; minThr: number; maxThr: number; pitch2thr: number;       // nav.fw throttle model (us)
  posZp: number; posZi: number; posZd: number; posZff: number;                // fw_alt (POS_Z) PID, raw 0-255
  altResponse: number; maxClimbRate: number;                                  // alt->climbrate cascade (cm/s)
  controlSmoothness: number;                                                  // nav_fw_control_smoothness (pitch-cmd LPF)
  fwPr: number; fwIr: number; fwDr: number; fwFfr: number;                    // roll rate PID
  fwPp: number; fwIp: number; fwDp: number; fwFfp: number;                    // pitch rate PID
  fwPy: number; turnAssist: number;                                          // yaw rate PID P + turn-assist gain
  pLevel: number; iLevel: number;                                            // ANGLE-mode strength + rate LPF Hz
  loiterRadius: number; rthAltitude: number;                                 // loiter/orbit radius (cm); RTH alt (cm)
  launchClimbAngle: number; launchVelocity: number; launchTimeout: number;   // NAV LAUNCH sequence
}
// Every setting an aircraft's inav.diff may carry, and how the validator accounts for it. A setting must be
// USED (read into a NavParams field the control law consumes) or explicitly ACK'd as sim-irrelevant — the
// coverage check below fails if an inav.diff carries any setting not on one of these lists.
const INAV_USED = new Set([
  'nav_fw_climb_angle', 'nav_fw_dive_angle', 'nav_fw_bank_angle', 'fw_reference_airspeed', 'nav_fw_land_approach_length',
  'nav_fw_cruise_thr', 'nav_fw_min_thr', 'nav_fw_max_thr', 'nav_fw_pitch2thr',
  'nav_fw_pos_z_p', 'nav_fw_pos_z_i', 'nav_fw_pos_z_d', 'nav_fw_pos_z_ff', 'nav_fw_alt_control_response', 'nav_fw_auto_climb_rate',
  'nav_fw_control_smoothness', 'fw_p_roll', 'fw_i_roll', 'fw_d_roll', 'fw_ff_roll',
  'fw_p_pitch', 'fw_i_pitch', 'fw_d_pitch', 'fw_ff_pitch', 'fw_p_yaw', 'fw_turn_assist_yaw_gain',
  'fw_p_level', 'fw_i_level', 'nav_fw_loiter_radius', 'nav_rth_altitude',
]);
const INAV_SIM_IRRELEVANT = new Set([
  'looptime',                          // FC scheduler period — the validator steps the FDM at a fixed rate
  // NAV LAUNCH is a bungee/hand launch (high initial energy); the validator takes off ROLLING from a runway,
  // so the whole launch group — detection, spin-up, and the steep launch climb angle — does not apply.
  'nav_fw_launch_accel', 'nav_fw_launch_detect_time', 'nav_fw_launch_max_altitude', 'nav_fw_launch_max_angle',
  'nav_fw_launch_min_time', 'nav_fw_launch_motor_delay', 'nav_fw_launch_spinup_time',
  'nav_fw_launch_climb_angle', 'nav_fw_launch_velocity', 'nav_fw_launch_timeout',
]);
/** Assert every setting in an inav.diff is accounted for; throws naming any unaccounted setting. */
export function assertInavCoverage(ac: string): void {
  const f = `${SIM}/aircraft/models/${ac}/inav.diff`;
  if (!existsSync(f)) return;
  const unaccounted = [...readFileSync(f, 'utf8').matchAll(/^set\s+([a-z_0-9]+)\s*=/gm)]
    .map((m) => m[1]).filter((s) => !INAV_USED.has(s) && !INAV_SIM_IRRELEVANT.has(s));
  if (unaccounted.length) throw new Error(`inav.diff(${ac}): settings not covered by the validator: ${[...new Set(unaccounted)].join(', ')}`);
}
export function inavParams(ac: string): NavParams {
  const p: NavParams = {
    climbAngle: 20, diveAngle: 15, bankAngle: 35, cruise: 15, approachLen: 350, glideAngle: 3,
    cruiseThr: 1400, minThr: 1200, maxThr: 1700, pitch2thr: 10,
    posZp: 30, posZi: 5, posZd: 10, posZff: 30, altResponse: 40, maxClimbRate: 500, controlSmoothness: 0,
    fwPr: 5, fwIr: 7, fwDr: 0, fwFfr: 50, fwPp: 5, fwIp: 7, fwDp: 0, fwFfp: 50, fwPy: 6, turnAssist: 20,
    pLevel: 20, iLevel: 5, loiterRadius: 7500, rthAltitude: 10000,
    launchClimbAngle: 18, launchVelocity: 0, launchTimeout: 5000,
  };
  const f = `${SIM}/aircraft/models/${ac}/inav.diff`;
  if (existsSync(f)) {
    const t = readFileSync(f, 'utf8');
    const g = (name: string, d: number, scale = 1) => { const m = t.match(new RegExp(`set ${name}\\s*=\\s*(-?\\d+(?:\\.\\d+)?)`)); return m ? Number(m[1]) * scale : d; };
    p.climbAngle = g('nav_fw_climb_angle', p.climbAngle);
    p.diveAngle = g('nav_fw_dive_angle', p.diveAngle);
    p.bankAngle = g('nav_fw_bank_angle', p.bankAngle);
    p.cruise = g('fw_reference_airspeed', p.cruise * 100) / 100;
    p.approachLen = g('nav_fw_land_approach_length', p.approachLen * 100) / 100;
    p.cruiseThr = g('nav_fw_cruise_thr', p.cruiseThr);
    p.minThr = g('nav_fw_min_thr', p.minThr);
    p.maxThr = g('nav_fw_max_thr', p.maxThr);
    p.pitch2thr = g('nav_fw_pitch2thr', p.pitch2thr);
    p.posZp = g('nav_fw_pos_z_p', p.posZp); p.posZi = g('nav_fw_pos_z_i', p.posZi);
    p.posZd = g('nav_fw_pos_z_d', p.posZd); p.posZff = g('nav_fw_pos_z_ff', p.posZff);
    p.altResponse = g('nav_fw_alt_control_response', p.altResponse);
    p.maxClimbRate = g('nav_fw_auto_climb_rate', p.maxClimbRate);
    p.controlSmoothness = g('nav_fw_control_smoothness', p.controlSmoothness);
    p.fwPr = g('fw_p_roll', p.fwPr); p.fwIr = g('fw_i_roll', p.fwIr); p.fwDr = g('fw_d_roll', p.fwDr); p.fwFfr = g('fw_ff_roll', p.fwFfr);
    p.fwPp = g('fw_p_pitch', p.fwPp); p.fwIp = g('fw_i_pitch', p.fwIp); p.fwDp = g('fw_d_pitch', p.fwDp); p.fwFfp = g('fw_ff_pitch', p.fwFfp);
    p.fwPy = g('fw_p_yaw', p.fwPy); p.turnAssist = g('fw_turn_assist_yaw_gain', p.turnAssist);
    p.pLevel = g('fw_p_level', p.pLevel); p.iLevel = g('fw_i_level', p.iLevel);
    p.loiterRadius = g('nav_fw_loiter_radius', p.loiterRadius);
    p.rthAltitude = g('nav_rth_altitude', p.rthAltitude);
    p.launchClimbAngle = g('nav_fw_launch_climb_angle', p.launchClimbAngle);
    p.launchVelocity = g('nav_fw_launch_velocity', p.launchVelocity);
    p.launchTimeout = g('nav_fw_launch_timeout', p.launchTimeout);
  }
  return p;
}
export const turnRadius = (V: number, bankDeg: number) => (V * V) / (G * Math.tan(Math.max(5, bankDeg) * D2R));

export type GateKind = 'climb' | 'route' | 'approach' | 'point';
export interface Gate { lat: number; lon: number; asl: number; r: number; kind: GateKind; gate: 'ring' | 'sphere'; brg?: number; }
export type Pt = [number, number, number];   // lat, lon, asl

/** Sphere gate: reach the position within radius, direction irrelevant. */
export const sphere = (lat: number, lon: number, asl: number, r: number, kind: GateKind = 'point'): Gate =>
  ({ lat, lon, asl, r, kind, gate: 'sphere' });

/** The directed ring corridor for a mission: climb-out + straight enroute legs + approach glideslope. */
export function buildTrack(m: Mission, opts: { ringRadius?: number; nClimb?: number; nApproach?: number } = {}): Gate[] {
  const ringRadius = opts.ringRadius ?? 150, nClimb = opts.nClimb ?? 12, nApproach = opts.nApproach ?? 12;
  const P = inavParams(m.aircraft);
  const to = m.takeoff, ld = m.land, wps = m.waypoints;
  const V = P.cruise, spacing = Math.max(120, turnRadius(V, P.bankAngle) * 0.9);
  const toElev = to.elevM;
  const gates: Gate[] = [];
  const ring = (lat: number, lon: number, asl: number, brg: number, kind: GateKind): Gate =>
    ({ lat, lon, asl, brg, r: ringRadius, kind, gate: 'ring' });

  // climb-out: rise at nav_fw_climb_angle along the departure runway heading
  const tanC = Math.tan(P.climbAngle * D2R);
  const wp1asl = toElev + (wps[0]?.altRel ?? 100);
  const climbLen = Math.max(spacing * nClimb, (wp1asl - toElev) / Math.max(tanC, 0.02));
  for (let i = 1; i <= nClimb; i++) {
    const d = climbLen * i / nClimb; const [la, lo] = dest(to.lat, to.lon, to.headingDeg, d);
    gates.push(ring(la, lo, toElev + d * tanC, to.headingDeg, 'climb'));
  }
  // enroute: straight legs through the waypoints (iNav flies straight-to-WP then turns)
  const appAlt = ld.elevM + P.approachLen * Math.tan(P.glideAngle * D2R);
  const [appLat, appLon] = dest(ld.lat, ld.lon, (ld.headingDeg + 180) % 360, P.approachLen);
  const last = gates[gates.length - 1];
  const rpts: Pt[] = [[last.lat, last.lon, last.asl]];
  for (const w of wps) rpts.push([w.lat, w.lon, toElev + (w.altRel ?? 0)]);
  rpts.push([appLat, appLon, appAlt]);
  let length = 0;
  for (let i = 0; i < rpts.length - 1; i++) length += distLL([rpts[i][0], rpts[i][1]], [rpts[i + 1][0], rpts[i + 1][1]]);
  const needRoute = Math.max(1, 100 - nClimb - nApproach) + rpts.length;
  const rspacing = Math.min(spacing, length / needRoute);
  for (let i = 0; i < rpts.length - 1; i++) {
    const [la0, lo0, z0] = rpts[i]; const [la1, lo1, z1] = rpts[i + 1];
    const seg = distLL([la0, lo0], [la1, lo1]); const br = bearing(la0, lo0, la1, lo1);
    const k = Math.max(1, Math.floor(seg / rspacing));
    for (let j = 0; j < k; j++) { const f = j / k; gates.push(ring(la0 + (la1 - la0) * f, lo0 + (lo1 - lo0) * f, z0 + (z1 - z0) * f, br, 'route')); }
  }
  // approach: glideslope on the landing runway centreline into the threshold
  const tanG = Math.tan(P.glideAngle * D2R);
  for (let i = nApproach; i >= 1; i--) {
    const d = P.approachLen * i / nApproach; const [la, lo] = dest(ld.lat, ld.lon, (ld.headingDeg + 180) % 360, d);
    gates.push(ring(la, lo, ld.elevM + d * tanG, ld.headingDeg, 'approach'));
  }
  return gates;
}

/** Index of the first pass of gate `g` at/after `start`; else -1. */
export function pierceIndex(tj: Pt[], g: Gate, start = 0): number {
  const casl = g.asl, r = g.r;
  if (g.gate === 'sphere') {
    let prev: [number, number, number] | null = null;
    for (let idx = start; idx < tj.length; idx++) {
      const [e, n] = enuOff(tj[idx][0], tj[idx][1], g.lat, g.lon); const u = tj[idx][2] - casl;
      if (Math.hypot(e, n, u) < r) return idx;
      if (prev) {                                             // closest approach on the segment (fast fly-through)
        const [pe, pn, pu] = prev; const de = e - pe, dn = n - pn, du = u - pu; const L2 = de * de + dn * dn + du * du;
        if (L2 > 1e-9) { let t = -(pe * de + pn * dn + pu * du) / L2; t = Math.max(0, Math.min(1, t));
          if (Math.hypot(pe + t * de, pn + t * dn, pu + t * du) < r) return idx; }
      }
      prev = [e, n, u];
    }
    return -1;
  }
  const br = (g.brg ?? 0) * D2R, ne = Math.sin(br), nn = Math.cos(br);   // ring: forward pierce of the plane
  let prev: [number, number, number, number] | null = null;
  for (let idx = start; idx < tj.length; idx++) {
    const [e, n] = enuOff(tj[idx][0], tj[idx][1], g.lat, g.lon); const u = tj[idx][2] - casl; const s = e * ne + n * nn;
    if (prev) { const [pe, pn, pu, ps] = prev;
      if (ps < 0 && s >= 0) { const t = ps / (ps - s);
        const ct = (pe + t * (e - pe)) * (-nn) + (pn + t * (n - pn)) * ne; const cu = pu + t * (u - pu);
        if (Math.hypot(ct, cu) < r) return idx; } }
    prev = [e, n, u, s];
  }
  return -1;
}

/** (threaded, inorder): gates passed at all, and the longest run passed IN SEQUENCE. */
export function score(tj: Pt[], gates: Gate[]): { threaded: number; inorder: number } {
  let threaded = 0; for (const g of gates) if (pierceIndex(tj, g) >= 0) threaded++;
  let inorder = 0, cur = 0, i = 0;
  for (const g of gates) { const j = pierceIndex(tj, g, i); if (j >= 0) { cur++; i = j; inorder = Math.max(inorder, cur); } else cur = 0; }
  return { threaded, inorder };
}
