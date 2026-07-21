// Fail-fast predicate evaluator — the VERIFIABLE half of the mission format (FORMAT.md §3). Predicates are
// defined over the sampled flight track S (one tuple/tick). A mission passes iff every phase completes,
// every task's verify predicates hold, and the envelope was never violated; the runner aborts on the first
// violation. Position-based predicates are wind-robust (range/XTE/coverage); heading-based (LOS, run-in
// axis) use track χ for geometry and yaw ψ only where pointing physically matters.

const G = 9.80665;
const D2R = Math.PI / 180, R2D = 180 / Math.PI;

export interface Sample {
  t: number; lat: number; lon: number; hAsl: number; hAgl: number;
  roll: number; pitch: number; yaw: number; ias: number; vs: number; gs: number; course: number;
  wp: number; phase: string;
}
export type Track = Sample[];
export interface Point { lat: number; lon: number; altAgl?: number; }
export type Leg = Point[];

const clat = (lat: number) => Math.cos(lat * D2R);
/** Great-circle-ish local range in metres (equirectangular, fine at mission scale). */
export function range(p: Point, s: { lat: number; lon: number }): number {
  const e = (s.lon - p.lon) * 111320 * clat(p.lat), n = (s.lat - p.lat) * 111320;
  return Math.hypot(e, n);
}
export function bearing(from: { lat: number; lon: number }, to: Point): number {
  const y = Math.sin((to.lon - from.lon) * D2R) * Math.cos(to.lat * D2R);
  const x = Math.cos(from.lat * D2R) * Math.sin(to.lat * D2R) - Math.sin(from.lat * D2R) * Math.cos(to.lat * D2R) * Math.cos((to.lon - from.lon) * D2R);
  return (Math.atan2(y, x) * R2D + 360) % 360;
}
const wrap180 = (x: number) => { while (x > 180) x -= 360; while (x < -180) x += 360; return x; };
/** ENU offset of s relative to origin o (metres, east/north). */
function enu(o: { lat: number; lon: number }, s: { lat: number; lon: number }): [number, number] {
  return [(s.lon - o.lon) * 111320 * clat(o.lat), (s.lat - o.lat) * 111320];
}
/** Signed cross-track distance of sample s from the infinite line through leg endpoints a->b (metres). */
export function xte(a: Point, b: Point, s: { lat: number; lon: number }): number {
  const [ex, ny] = enu(a, s); const [bx, byn] = enu(a, b);
  const L = Math.hypot(bx, byn); if (L < 1e-6) return Math.hypot(ex, ny);
  return (ex * byn - ny * bx) / L;   // left-positive
}

export interface PredResult { pass: boolean; value: number; detail: string; }
const ok = (value: number, detail: string): PredResult => ({ pass: true, value, detail });
const bad = (value: number, detail: string): PredResult => ({ pass: false, value, detail });

/** ∃t: range(P,t) ≤ r */
export function CAPTURE(S: Track, P: Point, r: number): PredResult {
  let best = Infinity; for (const s of S) best = Math.min(best, range(P, s));
  return best <= r ? ok(best, `closest ${best.toFixed(0)}m ≤ ${r}m`) : bad(best, `never within ${r}m (closest ${best.toFixed(0)}m)`);
}
/** ∃t≤t_max: range(P,t) ≤ r */
export function CAPTURE_BY(S: Track, P: Point, r: number, tMax: number): PredResult {
  let best = Infinity, bt = 0; for (const s of S) if (s.t <= tMax && range(P, s) < best) { best = range(P, s); bt = s.t; }
  return best <= r ? ok(bt, `captured at t=${bt.toFixed(0)}s ≤ ${tMax}s`) : bad(best, `not within ${r}m by t=${tMax}s (closest ${best.toFixed(0)}m)`);
}
/** ∀t∈phase: |XTE(leg,t)| ≤ W */
export function CORRIDOR(S: Track, a: Point, b: Point, W: number): PredResult {
  let worst = 0, wt = 0; for (const s of S) { const x = Math.abs(xte(a, b, s)); if (x > worst) { worst = x; wt = s.t; } if (x > W) return bad(x, `left corridor ±${W}m: XTE ${x.toFixed(0)}m at t=${s.t.toFixed(0)}s`); }
  return ok(worst, `max |XTE| ${worst.toFixed(0)}m ≤ ${W}m (at t=${wt.toFixed(0)}s)`);
}
/** ∀t∈phase: |h_agl−a| ≤ tol */
export function ALT_BAND(S: Track, a: number, tol: number): PredResult {
  let worst = 0, wv = a; for (const s of S) { const d = Math.abs(s.hAgl - a); if (d > worst) { worst = d; wv = s.hAgl; } if (d > tol) return bad(s.hAgl, `alt ${s.hAgl.toFixed(0)}m off band ${a}±${tol}m at t=${s.t.toFixed(0)}s`); }
  return ok(worst, `max alt dev ${worst.toFixed(0)}m ≤ ${tol}m (${wv.toFixed(0)}m)`);
}
/** ∀t∈phase: |IAS−v| ≤ tol (IAS from [flt]) */
export function SPEED_BAND(S: Track, v: number, tol: number): PredResult {
  let worst = 0, wv = v; for (const s of S) { const d = Math.abs(s.ias - v); if (d > worst) { worst = d; wv = s.ias; } if (d > tol) return bad(s.ias, `IAS ${s.ias.toFixed(1)} off band ${v}±${tol} at t=${s.t.toFixed(0)}s`); }
  return ok(worst, `max IAS dev ${worst.toFixed(1)} ≤ ${tol} (${wv.toFixed(1)})`);
}
/** ∃ contiguous [ta,tb], tb−ta≥T, ∀t: r_min ≤ range(C,t) ≤ r_max (annulus dwell) */
export function DWELL(S: Track, C: Point, rMin: number, rMax: number, T: number): PredResult {
  let start = -1, best = 0;
  for (let i = 0; i < S.length; i++) {
    const r = range(C, S[i]); const inBand = r >= rMin && r <= rMax;
    if (inBand) { if (start < 0) start = S[i].t; best = Math.max(best, S[i].t - start); } else start = -1;
  }
  return best >= T ? ok(best, `dwelled ${best.toFixed(0)}s ≥ ${T}s in [${rMin},${rMax}]m`) : bad(best, `longest contiguous dwell ${best.toFixed(0)}s < ${T}s in [${rMin},${rMax}]m`);
}
/** unwrapped |ΔΘ(C)| ≥ 2πN within an in-band window (real closed orbits, not fly-throughs) */
export function LAPS(S: Track, C: Point, N: number, rMax = Infinity): PredResult {
  let total = 0, prev: number | null = null;
  for (const s of S) {
    if (range(C, s) > rMax) { prev = null; continue; }
    const th = bearing(C, s);
    if (prev !== null) total += wrap180(th - prev);
    prev = th;
  }
  const laps = Math.abs(total) / 360;
  return laps >= N ? ok(laps, `${laps.toFixed(2)} laps ≥ ${N}`) : bad(laps, `only ${laps.toFixed(2)} laps < ${N}`);
}
/** grid poly; covered = cells within swath/2 of track; |covered|/|poly| ≥ X */
export function COVERAGE(S: Track, poly: Point[], swathM: number, X: number, grid = swathM / 2): PredResult {
  let minLa = Infinity, maxLa = -Infinity, minLo = Infinity, maxLo = -Infinity;
  for (const p of poly) { minLa = Math.min(minLa, p.lat); maxLa = Math.max(maxLa, p.lat); minLo = Math.min(minLo, p.lon); maxLo = Math.max(maxLo, p.lon); }
  const dLa = grid / 111320, dLo = grid / (111320 * clat((minLa + maxLa) / 2));
  let cells = 0, covered = 0;
  for (let la = minLa; la <= maxLa; la += dLa) for (let lo = minLo; lo <= maxLo; lo += dLo) {
    if (!pointInPoly(la, lo, poly)) continue;
    cells++;
    for (const s of S) if (range({ lat: la, lon: lo }, s) <= swathM / 2) { covered++; break; }
  }
  const frac = cells ? covered / cells : 0;
  return frac >= X ? ok(frac, `coverage ${(frac * 100).toFixed(0)}% ≥ ${(X * 100).toFixed(0)}% (${covered}/${cells} cells)`) : bad(frac, `coverage ${(frac * 100).toFixed(0)}% < ${(X * 100).toFixed(0)}%`);
}
function pointInPoly(la: number, lo: number, poly: Point[]): boolean {
  let inside = false;
  for (let i = 0, j = poly.length - 1; i < poly.length; j = i++) {
    const yi = poly[i].lat, xi = poly[i].lon, yj = poly[j].lat, xj = poly[j].lon;
    if (((yi > la) !== (yj > la)) && (lo < (xj - xi) * (la - yi) / (yj - yi) + xi)) inside = !inside;
  }
  return inside;
}
/** lo ≤ t(evt) ≤ hi */
export function TIME_WINDOW(tEvt: number, lo: number, hi: number): PredResult {
  return tEvt >= lo && tEvt <= hi ? ok(tEvt, `event at t=${tEvt.toFixed(0)}s ∈ [${lo},${hi}]`) : bad(tEvt, `event t=${tEvt.toFixed(0)}s ∉ [${lo},${hi}]`);
}
/** ∀t∈dwell: target within ±tol of the gimbal beam (abeam track ±90°) */
export function LOS(S: Track, P: Point, tol: number): PredResult {
  let worst = 0;
  for (const s of S) {
    const b = bearing(s, P); const abeamL = (s.course + 90) % 360, abeamR = (s.course + 270) % 360;
    const off = Math.min(Math.abs(wrap180(b - abeamL)), Math.abs(wrap180(b - abeamR)));
    if (off > worst) worst = off;
    if (off > tol) return bad(off, `LOS ${off.toFixed(0)}° off gimbal ±${tol}° at t=${s.t.toFixed(0)}s`);
  }
  return ok(worst, `max LOS off-beam ${worst.toFixed(0)}° ≤ ${tol}°`);
}
/** every interval where pred(sample) is false lasts ≤ g_max seconds */
export function NO_GAP(S: Track, pred: (s: Sample) => boolean, gMax: number): PredResult {
  let gapStart = -1, worst = 0;
  for (const s of S) {
    if (!pred(s)) { if (gapStart < 0) gapStart = s.t; worst = Math.max(worst, s.t - gapStart); }
    else gapStart = -1;
    if (worst > gMax) return bad(worst, `gap ${worst.toFixed(0)}s > ${gMax}s at t=${s.t.toFixed(0)}s`);
  }
  return ok(worst, `max gap ${worst.toFixed(0)}s ≤ ${gMax}s`);
}
/** mean −d range/dt ≥ rate_min over the τ s before capture (genuine sustained closing) */
export function CLOSING(S: Track, Tgt: (t: number) => Point, rateMin: number, tau: number, r: number): PredResult {
  let capIdx = -1; for (let i = 0; i < S.length; i++) if (range(Tgt(S[i].t), S[i]) <= r) { capIdx = i; break; }
  if (capIdx < 0) return bad(0, `never captured (no closing window)`);
  const tCap = S[capIdx].t; let i0 = capIdx; while (i0 > 0 && S[i0].t > tCap - tau) i0--;
  const r0 = range(Tgt(S[i0].t), S[i0]), r1 = range(Tgt(S[capIdx].t), S[capIdx]);
  const dt = Math.max(1e-3, S[capIdx].t - S[i0].t); const rate = (r0 - r1) / dt;
  return rate >= rateMin ? ok(rate, `closing ${rate.toFixed(1)} m/s ≥ ${rateMin} over ${tau}s`) : bad(rate, `closing only ${rate.toFixed(1)} m/s < ${rateMin} (lucky crossing?)`);
}
/** ∀t: range(entity(t), scripted_path(t)) ≤ tol — the scripted entity stayed on its own plan */
export function ON_PLAN(entity: (t: number) => Point, plan: (t: number) => Point, ts: number[], tol: number): PredResult {
  let worst = 0; for (const t of ts) { const d = range(entity(t), plan(t)); if (d > worst) worst = d; if (d > tol) return bad(d, `entity off plan ${d.toFixed(0)}m at t=${t}s`); }
  return ok(worst, `entity on plan (max ${worst.toFixed(0)}m ≤ ${tol}m)`);
}
/** discrete Fréchet distance between the flown ground track and a pattern polyline ≤ W */
export function FRECHET(S: Track, pattern: Point[], W: number): PredResult {
  const P = S.map((s) => ({ lat: s.lat, lon: s.lon }));
  const n = P.length, m = pattern.length; if (!n || !m) return bad(Infinity, `empty track/pattern`);
  const ca = Array.from({ length: n }, () => new Float64Array(m).fill(-1));
  const d = (i: number, j: number) => range(pattern[j], P[i]);
  for (let i = 0; i < n; i++) for (let j = 0; j < m; j++) {
    const c = d(i, j);
    if (i === 0 && j === 0) ca[i][j] = c;
    else if (i === 0) ca[i][j] = Math.max(ca[i][j - 1], c);
    else if (j === 0) ca[i][j] = Math.max(ca[i - 1][j], c);
    else ca[i][j] = Math.max(Math.min(ca[i - 1][j], ca[i - 1][j - 1], ca[i][j - 1]), c);
  }
  const fr = ca[n - 1][m - 1];
  return fr <= W ? ok(fr, `Fréchet ${fr.toFixed(0)}m ≤ ${W}m`) : bad(fr, `Fréchet ${fr.toFixed(0)}m > ${W}m (wrong pattern)`);
}

/** Coordinated-turn radius, used to bound how tight a task's geometry may be. */
export const turnRadius = (V: number, bankDeg: number) => (V * V) / (G * Math.tan(Math.max(5, bankDeg) * D2R));
