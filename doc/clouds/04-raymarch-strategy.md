# D — March Strategy: Step Counts, Adaptive Stepping, Jitter, Resolution Tiers

> **Legacy studies of the demolished FBCloud* chain; kept for the noise/raymarch groundwork, see [../flightbox/render/clouds.md](../flightbox/render/clouds.md).**

Source: Schneider & Vos, SIGGRAPH 2015 (slides 74–92, EXACT numbers below); Schneider, Nubis³ 2023
(slides 44–51, EXACT — the cone-step/SDF acceleration and the two-pass resolution split); Hillaire,
SIGGRAPH 2016 (slide 44, EXACT resolution + timing numbers for the Frostbite scheme, which differs
from Schneider's).

## 1. Two-tier "cheap vs. full" sampling (Schneider 2015, slides 74–80 — EXACT)

The sampler has two cost tiers, corresponding directly to the two textures in
[01-noise-construction.md](01-noise-construction.md):

- **Cheap**: base-shape (128³) sample only — defines a coarse isosurface of "could there be a cloud
  here".
- **Full**: base-shape + detail (32³) erosion — the actual density used for lighting/alpha.

March procedure, quoted structure (slides 74–79):

1. March at a **large step size** using only cheap samples, until the cheap sample returns non-zero
   (i.e. the ray has entered the coarse isosurface).
2. **Step back one** cheap-step before switching to full sampling — guarantees the fine detail near
   the entry boundary is not skipped.
3. Switch to **full** (detailed) sampling with a smaller step size while density stays non-zero.
4. **Early-exit** the whole march once accumulated alpha reaches 1.0 (slide 78).
5. If several consecutive full samples return **zero** density, switch back to cheap stepping until
   another isosurface is hit or the layer top is reached (slide 79).

## 2. Step-count numbers — EXACT (Schneider 2015, slide 80, 89)

> "an initial potential 64 samples and end with a potential 128 at the horizon... I say potential
> because of the optimizations which can cause the march to exit early."

So: **64 potential steps** looking straight up through the cloud layer, **128 potential steps** at
grazing/horizon angles (ray traverses more of the shell obliquely) — both are upper bounds subject to
early-exit via alpha-saturation or the cheap/full switching above, not steps actually always taken.

## 3. Cone-step / signed-distance-field acceleration (Nubis³ 2023, slides 44–47 — EXACT numbers)

The evolved (2022/2023) method replaces the cheap/full two-tier march with **cone-step mapping**
(Dummer 2006, cited slide 44): precompute/maintain internal signed-distance fields so the ray can
"determine the largest step we can take before hitting any clouds" and take that step directly,
instead of iteratively probing at a fixed cheap-step size. Concrete before/after cost on the same
scene:

| Method | Cost |
|---|---|
| Fixed-step cheap/full march (their older approach) | **4.2 ms** |
| Cone-step / SDF-accelerated march | **1.3 ms** |

**3.2× speedup from adaptive step placement alone**, no resolution change. This is the highest-value,
lowest-risk optimization available if a future FlightBox iteration needs cloud march budget back — but
it requires maintaining a coarse SDF/occupancy structure, which is nontrivial; not recommended as a
first fix (see [09-current-state-gaps.md](09-current-state-gaps.md) for what to fix first).

## 4. Two-pass resolution split (Nubis³ / Envelope method, slide 50 — EXACT)

For the "flight-capable" envelope method specifically (the vertical-profile/temporal-amortized method
is NOT flight-capable per Nubis³'s own comparison table, slide 51 — see
[05-temporal-reprojection.md](05-temporal-reprojection.md)): split the march into two passes at
different resolutions —

| Pass | Resolution | Radius |
|---|---|---|
| Near | **960×540** (lower res *quality*, but denser samples since near clouds dominate the frame) | inner **200 m** |
| Far | **480×270** | remainder |

The exact split point (200 m) is scene-specific to their deck example; the *principle* — near geometry
gets more angular resolution because it fills more screen space and needs anti-aliasing, far geometry
can be coarser because it's naturally blurred by distance/haze — is the transferable part.

## 5. Blue-noise / dithered jitter — the correct pattern (Schneider 2015 §temporal, detailed in
[05-temporal-reprojection.md](05-temporal-reprojection.md); summarized here for the march itself)

Per-pixel jitter of the ray-march start offset is standard practice to convert banding into noise that
temporal accumulation then removes. The critical requirement: **the jitter pattern must be spatially
uncorrelated at the scale of a single frame's noise floor, AND must be either (a) held fixed across a
short accumulation window with a rotating offset only between windows, or (b) fed into a temporal
accumulation buffer that averages it out.** Jitter that changes every single frame with NO
accumulation does not converge to a smooth image — it just replaces static banding with **shimmering,
uncorrelated per-frame noise**, which is visually indistinguishable from "image static." This is the
single most likely mechanism behind a raymarch that "looks like static": full re-render every frame,
each frame's jitter uncorrelated with the last, no history buffer to average across. See
[05-temporal-reprojection.md](05-temporal-reprojection.md) and
[09-current-state-gaps.md](09-current-state-gaps.md).

## 6. FlightBox's current march (`FBRenderer.cpp:1274-1404`) vs. the above

| Aspect | Source recipe | FlightBox current |
|---|---|---|
| Two-tier cheap/full stepping | Yes (§1) — cheap coarse march, then step-back-one, then full | **No** — `density()` always evaluates the full (base+detail) sampler every step; no coarse isosurface pre-pass |
| Step count | 64–128 *potential*, heavily early-exited | `nSteps = max(16, 160 * quality)` (`FBRenderer.cpp:1355`) — within range at quality=1, but **every** step is a full sample (no cheap tier to make the high end affordable) |
| Early-exit on alpha=1 | Yes (slide 78) | Yes — `if (transm < 0.02) break;` (`FBRenderer.cpp:1378`), correct |
| Early-exit on consecutive zero density | Yes (slide 79) | **No** — no "back to cheap after N zero samples" logic; a step through empty sky between two deck layers still costs a full sample |
| Adaptive/cone-step placement | Yes (Nubis³, 3.2× win) | **No** — fixed `dt` per ray, uniform across the whole `tStart..tEnd` span |
| Resolution | Quarter-res march (Schneider/HZD) OR half-res (Hillaire/Frostbite) | **Quarter-res** (`CloudW = Width/4`, `FBRenderer.cpp:1475`) — matches the HZD tier, but WITHOUT that scheme's temporal accumulation (see next section) |
| Per-pixel jitter | Yes, but only meaningful WITH temporal accumulation | Present (`FBRenderer.cpp:1357`, screen-space hash + `C.p1.w` time term) but **re-evaluated fully every frame with no history buffer** — see §5 above and [05-temporal-reprojection.md](05-temporal-reprojection.md) |

The combination "quarter-res + per-frame-changing jitter + full re-render every frame + no temporal
buffer" is architecturally the *worst* of both worlds: it pays the quarter-res quality cost (visible
blockiness on upsample) without collecting the quarter-res scheme's entire reason for existing (its
80–95% cost reduction comes from spreading the cost over 16 frames of accumulation, not from cheaper
per-frame sampling alone).
