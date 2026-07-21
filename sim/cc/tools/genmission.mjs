// Mission generator — emit a declarative circuit ("Platzrunde") at a real OurAirports runway, for a given
// aircraft. Takeoff + land on the SAME runway (iNav safehome ≤200 m from arming → near-field circuit, §6),
// with a few waypoints forming a closed pattern out to a scaled distance/altitude. Speeds/envelope come
// from the aircraft's Vs/Vc so a mission is data, not code. Usage:
//   node genmission.mjs <ICAO> <aircraft> [tier] > missions/<name>.json
import { readFileSync } from 'node:fs';
import { resolve, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const SIM = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const DB = JSON.parse(readFileSync(`${SIM}/geo/airports.json`, 'utf8'));
const D2R = Math.PI / 180;

function env(ac, key, d) {
  const t = readFileSync(`${SIM}/aircraft/models/${ac}/profile.env`, 'utf8');
  const m = t.match(new RegExp(`^\\s*${key}\\s*=\\s*([0-9.]+)`, 'm'));
  return m ? Number(m[1]) : d;
}
function dest(lat, lon, brg, dist) {
  const br = brg * D2R, clat = Math.cos(lat * D2R);
  return [lat + (Math.cos(br) * dist) / 111320, lon + (Math.sin(br) * dist) / (111320 * clat)];
}

const [icao, ac, tier = '1'] = process.argv.slice(2);
if (!icao || !ac) { console.error('usage: genmission.mjs <ICAO> <aircraft> [tier]'); process.exit(2); }
const ap = DB[icao]; if (!ap) { console.error(`airport ${icao} not in DB`); process.exit(2); }
const rw = ap.runways.filter((r) => r.length_m > 900).sort((a, b) => b.length_m - a.length_m)[0] ?? ap.runways[0];

const vs = env(ac, 'FB_STALL', 11), vc = env(ac, 'FB_CRUISE', 20);
const jet = ac === 'f16';
// pattern scale: legs and altitudes grow with cruise speed (turn radius ∝ V²)
const S = jet ? 3.2 : 1.0;                       // jets fly wider
const patAlt = Math.round((jet ? 600 : 260) * (tier === '2' ? 1.4 : 1));
const legR = 3000 * S, cross = 2600 * S;
const hdg = rw.heading_deg, side = 1;            // right-hand circuit

const th = [rw.lat, rw.lon];
const [w1la, w1lo] = dest(th[0], th[1], hdg, legR);                        // upwind/departure
const [w2la, w2lo] = dest(w1la, w1lo, (hdg + 90 * side) % 360, cross);     // crosswind
const [w3la, w3lo] = dest(th[0], th[1], (hdg + 130 * side) % 360, legR * 0.7); // base back toward field

// per-leg altitude band (tier 2): the aircraft must hold the pattern altitude at each fix. Speed is bounded
// by the global envelope (v_min/v_max) — a precise per-leg speed_tgt needs the airframe's real throttle-
// cruise, which differs from the nav-reference Vc, so it is not auto-emitted.
// alt band (tier 2) only on wp2 — the ESTABLISHED downwind fix; wp1 is the departure end where the aircraft
// may still be climbing to pattern altitude (esp. in thin air at a high-elevation field).
const altTol = tier === '2' ? 70 : undefined;
const waypoints = [
  { lat: +w1la.toFixed(6), lon: +w1lo.toFixed(6), alt_agl: patAlt },
  { lat: +w2la.toFixed(6), lon: +w2lo.toFixed(6), alt_agl: patAlt, alt_tol_m: altTol },
  { lat: +w3la.toFixed(6), lon: +w3lo.toFixed(6), alt_agl: Math.round(patAlt * 0.8) },
];

const mission = {
  id: `${icao.toLowerCase()}-${ac}-tier${tier}`,
  name: `${tier === '2' ? 'Envelope circuit' : 'Platzrunde'} — ${ac} @ ${icao} (${ap.icao})`,
  aircraft: ac,
  envelope: {
    v_min_ms: +(vs * 0.9).toFixed(1), v_max_ms: +(vc * (jet ? 2.9 : 1.9)).toFixed(1),
    bank_max_deg: jet ? 55 : 45, pitch_max_deg: jet ? 35 : 30,
    alt_floor_agl_m: 10, alt_ceil_agl_m: patAlt + (tier === '2' ? 250 : 500),
    on_nan_abort: true, divergence_guards: true,
  },
  takeoff: { airport: icao, runway: rw.ident, rotate_agl_m: 50 },
  waypoints,
  land: { airport: icao, runway: rw.ident, touchdown_tol_m: jet ? 400 : 200, touchdown_gs_max_ms: jet ? 90 : 35 },
  success: { capture_radius_m: jet ? 2000 : 300 },
  procedure: jet ? { angle_hold_alt: patAlt - 40 } : {},
  abort: { timeout_s: 900, on_nan: true, on_envelope_violation: true },
};
process.stdout.write(JSON.stringify(mission, null, 2) + '\n');
