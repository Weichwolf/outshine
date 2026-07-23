# H — Experiment Protocol: Parameter Sweep + "Looks Like a Cloud" Checklist

Source: methodology synthesized from the iterative before/after slides across Schneider 2015 (slides
55, 62–66, 83 — each showing an isolated component: Beer's-law-only, +HG, +powder, cheap-vs-full) and
Nubis³ 2023 (slides 30–36 — with/without multi-scattering, ambient-only, direct-only, combined); this
"isolate one term, show it alone, then combine" structure is the transferable method, not a specific
FlightBox-authored rig. No primary source publishes an automated sweep tool — this section is a
recommended protocol adapted from that manual A/B methodology, not a transcription.

## 1. Why a sweep, and why grid-of-references

Both primary talks build confidence in each lighting/density term by rendering it **in isolation**
against the same fixed cloud shape, then combining incrementally, comparing each step against a
reference photograph. This isolates whether an artifact comes from noise/density (shape) or from
lighting (shading) — exactly the ambiguity a "looks like static" bug report needs resolved first.

## 2. Recommended rig structure

1. **Fix a single deterministic scene**: one camera position/orientation, one sun angle, one
   cloud-deck altitude/coverage — the SAME scene for every sweep cell, so only the swept parameter
   varies frame-to-frame.
2. **Render a grid of stills**, one axis per parameter under test (e.g. rows = coverage `0.2/0.5/0.8`,
   columns = base-noise frequency `1/9km, 1/6km, 1/3km`) — a contact sheet, not a video, so results are
   directly comparable side by side.
3. **Isolate terms** exactly as the source talks do, as a diagnostic mode switch (not a final feature):
   - density/alpha only (no lighting) — checks *shape*
   - + Beer's law only (no HG, no powder) — checks basic depth-darkening
   - + HG phase (no powder) — checks the silver-lining/forward-scatter look
   - + powder — checks edge-darkening/self-shadow look
   - + ambient — checks the sky-fill contribution
   - full combined — the actual shipped look
4. **Compare against reference photographs** of the SAME cloud type (cumulus/stratus/cumulonimbus) at
   a roughly matching sun angle — freely available meteorological/aviation photo references; match
   qualitatively (silhouette shape, base flatness, edge brightness pattern), not pixel-for-pixel.
5. **Vary ONE axis at a time** across the grid; never change two parameters between adjacent cells,
   or a visual difference can't be attributed to either.

## 3. Concrete parameters worth sweeping first (given FlightBox's current state, per
[09-current-state-gaps.md](09-current-state-gaps.md))

| Parameter | Current value | Suggested sweep range | What it isolates |
|---|---|---|---|
| Density scale (`C.p2.w`) | 5.0 | 0.5, 1.0, 2.0, 5.0 | Whether "static" is caused by extinction saturation ([02-density-coverage.md §4](02-density-coverage.md)) |
| Temporal accumulation on/off | off | off / on (once implemented) | Whether "static" is caused by uncorrelated per-frame jitter ([05-temporal-reprojection.md](05-temporal-reprojection.md)) |
| Powder scale | 6.0 | 1, 3, 6, with density-scale FIXED at 1.0 | Whether powder saturates to a flat constant ([03-lighting-model.md §3](03-lighting-model.md)) |
| Base noise composite construction | plain Perlin+Worley-erode | true Perlin-Worley displacement ([01-noise-construction.md §3](01-noise-construction.md)) | Whether the silhouette itself lacks billow structure |
| Step count at fixed quality | 16-160 (quality-scaled) | 32, 64, 128 fixed, WITHOUT the density-scale/temporal confounds above | Whether visible banding (not noise) is a separate, additional artifact |

## 4. "Sieht aus wie eine Wolke" checklist (silhouette + shading, both required)

**Silhouette (shape) — check with density/alpha-only render:**
- [ ] Flat, sharply defined base (real cumulus bases are near-planar — a height-gradient with a hard
  bottom cutoff, not a soft symmetric blob)
- [ ] Rounded, billowing/"cauliflower" top — NOT a smooth Perlin blob (needs the Perlin-Worley
  displacement or equivalent erosion, [01-noise-construction.md](01-noise-construction.md))
- [ ] Wispy, eroded edges — density should taper to near-zero at the silhouette boundary over a few
  noise-texture texels, not clip hard to zero
- [ ] No visible tiling repeats of the base/detail noise texture at typical viewing distances
  ([01-noise-construction.md §5](01-noise-construction.md) sanity check)
- [ ] No banding rings from discrete march steps (visible as concentric arcs, especially near the
  camera) — indicates step count too low or jitter not doing its job

**Shading (light) — check with full lighting on:**
- [ ] Self-shadowing: the side away from the sun is visibly darker than the side facing it (Beer's law
  working directionally, not just as a flat multiply)
- [ ] Silver lining: looking toward the sun through/near a cloud edge shows a bright forward-scatter
  halo (HG forward lobe, `g0=0.8`, working)
- [ ] Dark, saturated edges facing the light in thick clouds specifically where the view vector nearly
  aligns with the light vector (powder effect, view-dependent — check it changes as you orbit the
  camera around a fixed cloud, not just as a static per-pixel darkening)
- [ ] Crevices/concavities read as slightly BRIGHTER than convex bulges nearby (the powder/ambient
  in-scattering effect, not just uniform depth-darkening)
- [ ] Color temperature shifts warm near a low sun (sun transmittance through the atmosphere reddening
  the cloud-lit side) and stays cool/blue in shadowed regions (sky ambient) — both terms visibly
  distinct, not blended into a single flat gray

**Temporal (motion) — check across several frames of a moving camera:**
- [ ] No visible "sparkle"/dancing noise on a STATIONARY camera (if present with a static camera and
  static sun, jitter/temporal accumulation is broken — this is the current FlightBox symptom)
- [ ] No visible smearing/ghosting trail on a FAST-panning camera (if present, history rejection/clamp
  is too weak or absent)
- [ ] Edges of the cloud silhouette stay crisp under motion, not "swimming" independently of the shape
  underneath (indicates jitter amplitude too large relative to step size, or reprojection error)

## 5. Iteration order (given the specific "looks like static" symptom)

Per this protocol's own logic (isolate before combining), and given
[09-current-state-gaps.md](09-current-state-gaps.md)'s ranked findings: fix temporal accumulation
FIRST (it is the most likely single cause and is independently testable — same shape/lighting code,
before/after temporal), THEN re-run the silhouette checklist on the now-stable image, THEN tune
density-scale/powder-scale together (they interact — §3 above), THEN, only once the image is visually
stable and correctly shaped, revisit the noise construction itself if the "cauliflower" checkbox still
fails.
