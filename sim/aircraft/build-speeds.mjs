// Generate models/<ac>/speeds.json — the per-aircraft speed envelope the missions pull from, one
// {min, avg, max} band per flight scenario, derived from the model's OWN stall (Vs) and cruise (Vc)
// speeds (profile.env FB_STALL / FB_CRUISE) via standard aviation ratios. Grounded, not guessed, and
// reproducible: re-run after tuning an airframe's Vs/Vc.
//
//   node aircraft/build-speeds.mjs [c172 sgs233 f16 …]   (default: all models with a profile.env)
import { readFileSync, writeFileSync, readdirSync, existsSync } from 'node:fs';

const MODELS = new URL('./models/', import.meta.url).pathname;

// scenario bands as multipliers of Vs (stall) or Vc (cruise), or vne. Aviation-standard:
//  - near-stall / slow / takeoff-rotate / landing-Vref key off Vs; cruise/fast/intercept off Vc.
//  - scout = best endurance (~1.2 Vs, loiter long); long_range = best L/D range (~1.4 Vs).
const BANDS = {
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
const VNE_OF_VC = 1.9;                    // structural never-exceed ~ 1.9 * cruise for these airframes

const env = (txt, key) => { const m = txt.match(new RegExp(`^\\s*${key}\\s*=\\s*([0-9.]+)`, 'm')); return m ? Number(m[1]) : null; };
const r1 = (x) => Math.round(x * 10) / 10;

const which = process.argv.slice(2);
const models = which.length ? which : readdirSync(MODELS).filter((d) => existsSync(`${MODELS}${d}/profile.env`));

for (const ac of models) {
  const p = `${MODELS}${ac}/profile.env`;
  if (!existsSync(p)) { console.error(`skip ${ac}: no profile.env`); continue; }
  const txt = readFileSync(p, 'utf8');
  const Vs = env(txt, 'FB_STALL'), Vc = env(txt, 'FB_CRUISE');
  if (Vs == null || Vc == null) { console.error(`skip ${ac}: need FB_STALL + FB_CRUISE`); continue; }
  const vne = r1(Vc * VNE_OF_VC);
  const base = { Vs, Vc, vne };
  const scenarios = {};
  for (const [name, b] of Object.entries(BANDS)) {
    const ref = base[b.of];
    const val = (k) => (k === 'vne' ? vne : r1(Math.min(ref * k, vne)));
    scenarios[name] = { min: val(b.min), avg: val(b.avg), max: val(b.max) };
  }
  const out = { note: `m/s TAS; derived from Vs=${Vs} Vc=${Vc} (profile.env) — see aircraft/build-speeds.mjs`,
                stall_ms: Vs, vne_ms: vne, scenarios };
  writeFileSync(`${MODELS}${ac}/speeds.json`, JSON.stringify(out, null, 2) + '\n');
  console.error(`${ac}: Vs=${Vs} Vc=${Vc} vne=${vne} -> speeds.json (${Object.keys(scenarios).length} scenarios)`);
}
