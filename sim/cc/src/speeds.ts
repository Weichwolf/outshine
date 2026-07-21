// The scenario -> speed model, in ONE place. Each flight scenario is a {min, avg, max} band expressed as
// a multiplier of the aircraft's stall (Vs) or cruise (Vc) speed — the SAME ratios for every airframe, so
// they live here as code, not duplicated per model as data. An aircraft contributes ONLY its Vs/Vc
// (profile.env FB_STALL / FB_CRUISE); a mission asks for a scenario speed and gets ratio·Vs/Vc at runtime.
// If an airframe ever needs a genuinely non-derivable value (a real structural Vne, a measured best-L/D),
// that single number is a per-aircraft override — never a full re-materialised table.

export type Scenario =
  | 'stall' | 'slow' | 'takeoff' | 'landing' | 'scout' | 'long_range' | 'cruise' | 'fast' | 'intercept';
export interface Band { min: number; avg: number; max: number; }

const VNE_OF_VC = 1.9;                       // structural never-exceed ~ 1.9 * cruise for these airframes

// stall/slow/takeoff/landing/scout/long_range key off Vs; cruise/fast/intercept off Vc.
// scout = best endurance (~1.2 Vs, loiter long); long_range = best L/D range (~1.4 Vs).
const BANDS: Record<Scenario, { of: 'Vs' | 'Vc'; min: number; avg: number; max: number | 'vne' }> = {
  stall:      { of: 'Vs', min: 0.95, avg: 1.00, max: 1.10 },
  slow:       { of: 'Vs', min: 1.10, avg: 1.20, max: 1.30 },
  takeoff:    { of: 'Vs', min: 1.10, avg: 1.30, max: 1.50 },
  landing:    { of: 'Vs', min: 1.15, avg: 1.30, max: 1.45 },
  scout:      { of: 'Vs', min: 1.15, avg: 1.25, max: 1.40 },
  long_range: { of: 'Vs', min: 1.30, avg: 1.40, max: 1.55 },
  cruise:     { of: 'Vc', min: 0.90, avg: 1.00, max: 1.15 },
  fast:       { of: 'Vc', min: 1.20, avg: 1.40, max: 'vne' },
  intercept:  { of: 'Vc', min: 1.30, avg: 1.55, max: 'vne' },
};

const r1 = (x: number) => Math.round(x * 10) / 10;

export function vne(Vc: number): number { return r1(Vc * VNE_OF_VC); }

/** Per-scenario {min,avg,max} m/s for an aircraft, derived from its stall Vs and cruise Vc. */
export function scenarioSpeeds(Vs: number, Vc: number): Record<Scenario, Band> {
  const v = vne(Vc); const ref: Record<'Vs' | 'Vc', number> = { Vs, Vc };
  const out = {} as Record<Scenario, Band>;
  for (const name of Object.keys(BANDS) as Scenario[]) {
    const b = BANDS[name]; const base = ref[b.of];
    const val = (k: number | 'vne') => (k === 'vne' ? v : r1(Math.min(base * k, v)));
    out[name] = { min: val(b.min), avg: val(b.avg), max: val(b.max) };
  }
  return out;
}
