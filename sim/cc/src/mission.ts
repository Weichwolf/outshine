// Declarative mission -> resolved geometry, NATIVE TypeScript (no Python). A mission is data: takeoff +
// waypoints (AGL) + land. Resolving it: runway threshold lat/lon + heading from the OurAirports world DB
// (geo/airports.json), and each waypoint's home-relative altitude from the DEM (ground elevation via the
// fb-tiles /elev endpoint) — "Terrain lebt im CC". iNav only ever gets home-relative GPS numbers. The
// aircraft contributes its Vs/Vc (profile.env); the scenario speed envelope comes from speeds.ts.

import { readFileSync, existsSync } from 'node:fs';
import { resolve } from 'node:path';
import { scenarioSpeeds, vne as vneOfVc, type Scenario, type Band } from './speeds.js';
import { inavParams } from './ringtrack.js';

const SIM = resolve(import.meta.dirname, '../../..');   // cc/dist/src -> sim/

interface RunwayRec { ident: string; heading_deg: number; lat: number; lon: number; elev_m: number; length_m: number | null; surface: string; }
const AIRPORTS: Record<string, { icao: string; runways: RunwayRec[] }> =
  JSON.parse(readFileSync(`${SIM}/geo/airports.json`, 'utf8'));

export interface Waypoint { lat: number; lon: number; altAgl: number; altRel: number | null; }
export interface Runway { icao: string; runway: string; lat: number; lon: number; elevM: number; headingDeg: number; lengthM: number; }
export interface Mission {
  name: string; aircraft: string;
  takeoff: Runway; land: Runway; waypoints: Waypoint[];
  vs: number; vc: number; vne: number; vmin: number; speeds: Record<Scenario, Band>;   // aircraft speed envelope
  raw: any;                                                                             // the declarative spec, verbatim
}

/** Runway threshold record by airport ICAO + runway ident, from the world DB. */
export function runway(icao: string, ident: string): RunwayRec {
  const ap = AIRPORTS[icao];
  if (!ap) throw new Error(`airport ${icao} not in DB`);
  const rw = ap.runways.find((r) => r.ident === ident);
  if (!rw) throw new Error(`runway ${ident} not at ${icao} (have ${ap.runways.map((r) => r.ident).join(',')})`);
  return rw;
}

/** DEM ground elevation (m ASL) at lat/lon via fb-tiles /elev, or null if unreachable. */
export async function groundElev(lat: number, lon: number, tilesUrl: string): Promise<number | null> {
  if (!tilesUrl) return null;
  try {
    const r = await fetch(`${tilesUrl}/elev?lat=${lat}&lon=${lon}&block=1`);
    if (!r.ok) return null;
    const v = Number((await r.text()).trim());
    return Number.isFinite(v) && v > -1e8 ? v : null;
  } catch { return null; }
}

// Speeds derive from the SAME model files flightbox flies — never a hand-set profile.env value that could
// diverge. Vc = iNav's nav reference airspeed (inav.diff fw_reference_airspeed). Vs = 1g stall from the
// JSBSim physics: Vs = √(2·W / (ρ·S·CLmax)) with W (total weight) + S (wing area) parsed from {model}.xml
// and CLmax a standard ~1.4. Vne/Vmin are structural/handling multiples of these.
// CLmax at MODEL Reynolds (~5e4–2e5), not the textbook full-scale ~1.4 — low-Re airfoils reach a lower
// CLmax, so the true stall speed is HIGHER than a 1.4 estimate. 1.1 is a conservative low-Re value; the
// resulting Vs sets the envelope floor + the validator's stall/departure guards, so erring high is safe.
const G0 = 9.80665, RHO0 = 1.225, LBS_N = 4.4482216, FT2_M2 = 0.09290304, CL_MAX = 1.1;
function stallFromXml(ac: string): number {
  const xml = readFileSync(`${SIM}/aircraft/models/${ac}/${ac}.xml`, 'utf8');
  const sm = xml.match(/<wingarea[^>]*>\s*([0-9.]+)/i);
  const S = sm ? Number(sm[1]) * FT2_M2 : 0;
  const wLbs = [...xml.matchAll(/<(?:emptywt|weight)[^>]*>\s*([0-9.]+)/gi)].reduce((s, m) => s + Number(m[1]), 0);
  const W = wLbs * LBS_N;
  return S > 0 && W > 0 ? Math.sqrt((2 * W) / (RHO0 * S * CL_MAX)) : 11;
}
function vsVc(ac: string): { vs: number; vc: number; vne: number; vmin: number } {
  const vc = inavParams(ac).cruise;                 // fw_reference_airspeed (inav.diff)
  const vs = Math.round(stallFromXml(ac) * 10) / 10; // 1g stall from {model}.xml physics
  // Vne ≈ structural never-exceed; Vmin ≈ min safe maneuvering (~1.2·Vs). These bound the mission envelope;
  // flightbox itself imposes no Vne (only non-divergence + stall), so they are advisory limits, not FDM gates.
  return { vs, vc, vne: vneOfVc(vc), vmin: Math.round(1.2 * vs * 10) / 10 };
}

/** Resolve a declarative mission (name or path under sim/) to a flyable GPS/ASL plan. */
export async function resolveMission(spec: string, tilesUrl = process.env.TILES_URL ?? 'http://localhost:8081'): Promise<Mission> {
  const cands = [spec, `${spec}.json`, `missions/${spec}`, `missions/${spec}.json`];
  const path = cands.map((c) => resolve(SIM, c)).find((p) => existsSync(p));
  if (!path) throw new Error(`mission not found: ${spec} (tried missions/${spec}.json)`);
  const m = JSON.parse(readFileSync(path, 'utf8'));
  const to = runway(m.takeoff.airport, m.takeoff.runway);
  const ld = runway(m.land.airport, m.land.runway);
  const home = to.elev_m;
  const waypoints: Waypoint[] = [];
  for (const w of m.waypoints ?? []) {
    const g = await groundElev(w.lat, w.lon, tilesUrl);
    const asl = g != null ? w.alt_agl + g : null;
    waypoints.push({ lat: w.lat, lon: w.lon, altAgl: w.alt_agl, altRel: asl != null ? asl - home : null });
  }
  const { vs, vc, vne, vmin } = vsVc(m.aircraft);
  const rw = (o: RunwayRec, icao: string): Runway => ({ icao, runway: o.ident, lat: o.lat, lon: o.lon, elevM: o.elev_m, headingDeg: o.heading_deg, lengthM: o.length_m ?? 1500 });
  return {
    name: m.name ?? '', aircraft: m.aircraft,
    takeoff: rw(to, m.takeoff.airport), land: rw(ld, m.land.airport),
    waypoints, vs, vc, vne, vmin, speeds: scenarioSpeeds(vs, vc), raw: m,
  };
}
