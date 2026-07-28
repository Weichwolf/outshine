# A — Noise Construction: Perlin-Worley Recipe

> **Legacy studies of the demolished FBCloud* chain; kept for the noise/raymarch groundwork, see [../flightbox/render/clouds.md](../clouds.md).**

Source: Schneider & Vos, SIGGRAPH 2015 (slides 27–32, 47); Schneider, Nubis³ 2023 recap of "Nubis,
Evolved" 2022 (slides 22–24); confirmed independently by Nijhoff's Himalayas Shadertoy writeup
(texture split only, no new numbers).

## 1. Why plain Perlin fBm fails

Standard layered-Perlin fBm (several octaves of Perlin noise + a height gradient) produces "wispy"
shapes but **no billows/bulges** — no sense of the cauliflower structure or implied vertical motion
real cumulus has (Schneider 2015, slides 23–26). This is very likely why a first attempt "looks like
static": Perlin fBm alone, unmodulated by a coverage remap and unclamped, produces a smooth continuous
density field with no hard silhouette — there is nothing to erode, so any erosion-noise term ends up
visible everywhere instead of only at cloud edges.

## 2. The two-texture split (verbatim from the source)

| Texture | Resolution | Channels | Content | Purpose |
|---|---|---|---|---|
| **Base shape** | **128³** | 4 (RGBA) | R = Perlin-Worley composite; G/B/A = Worley at increasing frequencies | Base cloud shape (low-frequency, billowy) |
| **Detail** | **32³** | 3 (RGB) | Worley at increasing frequencies | High-frequency erosion at the cloud *edges only* |
| Curl noise (Schneider 2015 only, optional) | 128² (2D) | 3 | Curl (non-divergent) noise | Distorts the detail texture to fake fluid/turbulence swirls |

Both textures are **tileable** (wrap-around Worley cell lookup, standard technique) so they repeat
seamlessly across the whole cloud layer instead of showing a texture edge.

Uncompressed storage: 128³ × 4 ch × 1 B (RGBA8) = **8 MiB**; 32³ × 4 ch = 128 KiB. Nubis³ 2023 confirms
the same order of magnitude for its evolved 4-channel 128³ noise: **"4 Channel, 128×128×128 Voxels,
Uncompressed 2 Bytes/Texel, 4.194 Megabytes"** (Nubis³ slide 22 — 2 B/texel there because it stores a
16-bit format, not RGBA8). **This confirms our `RGBA8Unorm` 128³/32³ choice (`command_center/fb/FBRenderer.cpp:1416-1429`,
8 MiB total) is the right order of magnitude for this GPU class** — do not go larger without a reason.

## 3. Perlin-Worley construction

"We layered [Worley noise] like the standard Perlin fBm approach... then used it as an offset to
dilate Perlin noise. This allowed us to keep the connectedness of Perlin noise but add some billowy
shapes to it. We referred to this as Perlin-Worley noise." (Schneider 2015, slide 28.) The published
talks do not give the exact blend weight; the widely reproduced practical form (used in the Nijhoff/
Bauer et al. open implementations) is:

```wgsl
// perlin-worley: billow the level-sets of Perlin with an inverted-Worley displacement, then remap
// so the Worley component only ever narrows (never widens) the Perlin base.
fn perlinWorley(uv: vec3f, freq: f32) -> f32 {
  let p  = perlinFbm(uv, freq);                 // 2-3 octaves, smooth connected base
  let w  = 1.0 - worleyFbm(uv, freq * 2.0);      // inverted Worley = tight cellular billows
  return remap(p, 0.0, 1.0, w, 1.0);             // Worley narrows the low end -> keeps billow silhouette
}
```

**FlightBox currently does NOT build this composite** — `kCloudBaseCS` (`FBRenderer.cpp:1225-1238`)
stores plain `perlinFbm` in R and plain `worleyFbm` at 3 frequencies in G/B/A, and the raymarch's
`density()` function then blends R-channel base scales directly (`FBRenderer.cpp:1280-1298`) without
ever combining Worley as a **displacement of** Perlin — it uses Worley only as post-erosion. This is
architecturally closer to Nubis's *envelope method* (density-field-then-erode) than to Schneider's
*Perlin-Worley base + Worley erosion*, which is a valid alternative construction, but the erosion
math must still follow §2.4 below for the edges to stay soft (see [09-current-state-gaps.md](09-current-state-gaps.md)).

## 4. Octave counts and frequencies — CONCRETE

Neither Schneider 2015 nor Nubis³ publishes the exact number of fBm octaves per noise channel (they
show the composited result, not the generator loop). The frequencies that *are* given are in world
distance, from the weathermap description (Schneider 2015, slide 44): clouds are drawn in a **35 km
radius** around the camera, with the horizon transition to cumulus starting at **15 km**. This anchors
the SCALE the noise must tile at: the base-shape 128³ texture, if tiled once every ~2–4 km of world
space, gives clouds with ~500 m–1 km scale of billow, which matches typical strato-cumulus (see
[02-density-coverage.md](02-density-coverage.md) for exact heights). A commonly used practical choice
(not from the primary source, but consistent with it and widely reproduced) is:

| Layer | Frequency (repeats per km) | Purpose |
|---|---|---|
| Base low (R composite) | 1 tile / **4–9 km** | Overall cloud-mass shape |
| Base high (secondary octave of the same texture) | 1 tile / **1–3 km** | Medium-scale lumps |
| Detail 1 (32³ tex, near freq) | 1 tile / **0.3–0.9 km** | Cauliflower bulges at the visible edge |
| Detail 2 (32³ tex, far freq) | 1 tile / **0.1–0.3 km** | Fine wisping at the very edge |

FlightBox's current frequencies (`FBRenderer.cpp:1285-1294`, `posKm / 9.0`, `posKm / 24.0` for base;
`posKm / 0.9`, `posKm / 0.28` for detail) sit **inside this range** — the frequency choice itself is
plausible; see [09-current-state-gaps.md](09-current-state-gaps.md) for why the result still looks
wrong (it is the *erosion formula*, not the frequency, that is most likely broken).

## 5. The exact erosion / coverage remap (verbatim, Nubis³ slide 24–25)

```c
// Nubis3 / Envelope method, verbatim from the deck:
float height_fraction = remap(height, min_height, max_height, 0.0, 1.0);
float top_gradient     = pow(1.0 - height_fraction, 1.5);
float bottom_gradient  = pow(height_fraction, 2.0);
float edge_gradient    = remap(sample_height, 0.0, 35.0, 1.0, 0.0);   // 35 km draw-radius falloff
float dimensional_profile = bottom_gradient * top_gradient * edge_gradient;

// Erosion — SAME formula, both vertical-profile and envelope methods (slides 24-25):
cloud_density = saturate(noise - (1.0 - dimensional_profile));
```

`remap(x, oldMin, oldMax, newMin, newMax)` is the standard linear remap used throughout:
`newMin + (x - oldMin) / (oldMax - oldMin) * (newMax - newMin)`, exactly as already defined in
FlightBox's `kAtmoCommon`/cloud shader (`FBRenderer.cpp:1222`). Good — that helper is correct and
shared correctly.

**The critical erosion formula is `saturate(noise - (1.0 - profile))`** — i.e. `remap(noise, 1-profile,
1, 0, 1)` clamped. This means: where `profile` (coverage/shape) is 1, the full noise range `[0,1]`
survives unmodified (dense core). Where `profile` drops toward 0, only the *very top* of the noise
range survives — a thin sliver — before the whole thing clips to 0. This is what produces sharp
silhouettes with wispy edges instead of a uniform haze: **the noise never gets to modulate density
directly; it only ever carves INTO an already-defined shape.**

FlightBox's current formula (`FBRenderer.cpp:1289, 1297`) does apply this same remap pattern twice
(once for coverage, once for erosion) — structurally correct — but note the *order/base* differs from
Nubis³: ours remaps the **smooth Perlin shape** by coverage first, THEN erodes the *coverage-remapped*
result by Worley detail; Nubis³ eroded the **dimensional profile** (height/coverage envelope, not yet
noise) by the *base noise itself* in one step. Both are valid variants of the same idea. What matters,
per this source, is that erosion is always `saturate(x - (1 - keep))`, never a plain multiply.

## Frequency ↔ km scale sanity check

Given the 35 km draw radius and 15 km cumulus transition (§4), and 128³/32³ texture resolutions: a
128³ texture tiled at "1 tile / 6 km" means one texel ≈ 47 m — fine detail for a texture whose highest
useful frequency (before it just looks like TV static at texel scale) is around 1 tile/2 km (texel ≈
16 m, near the diffraction limit of what 720p pixels 1–5 km away can resolve anyway). **Going below
~1 tile/1 km on the 128³ texture is wasted resolution and risks visible tiling repeats** at long
sightlines — worth checking against FlightBox's `posKm / 9.0`/`posKm / 24.0` (fine) and especially the
32³ detail texture at `posKm / 0.28` (≈ 8.75 m/texel over a 280 m tile — reasonable for fine edge
wisps, on the edge of what a 32³ texture can usefully resolve without banding; 32³ ÷ 0.28 km period
gives one texel every 8.75 m, consistent with the intent).
