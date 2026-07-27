# B — Density Model: Coverage, Type, Height Gradient, Absolute Extinction

> **Legacy studies of the demolished FBCloud* chain; kept for the noise/raymarch groundwork, see [../flightbox/render/clouds.md](../flightbox/render/clouds.md).**

Source: Schneider & Vos, SIGGRAPH 2015 (slides 33–46); Schneider, Nubis³ 2023 (slides 15–25); physical
extinction values are standard atmospheric-optics figures (cited inline, not from the game talks —
the game talks deliberately use *unitless* density, not physical σ).

## 1. Weathermap (coverage / type / precipitation)

Schneider 2015 (slides 38–41): a 2D texture sampled by world XZ position drives 3 channels:

| Channel | Meaning | Range |
|---|---|---|
| R | Coverage | 0 (clear) .. 1 (solid overcast) |
| G | Precipitation | 0 .. 1 — also darkens the cloud (rain clouds get **increased absorption**, slide 67) |
| B | Cloud type | 0 (stratus) .. 1 (cumulus); precipitation channel forces a transition toward **cumulonimbus at ≥70 % coverage** (slide 41) |

This is a **2D texture animated by a simulation** (wind advection + a "chance of rain" scalar), not a
per-frame procedural function — the weathermap is itself a slow-moving low-res texture (Schneider
gives no explicit resolution; commonly implemented at 512×512 covering tens of km, matching Heckel's
worked example which independently arrives at the same order of magnitude).

**FlightBox has no weathermap at all.** Coverage/type come from three scalars (`HudState.cloud_low/mid/high`,
`FBState.h:25`) fed from Open-Meteo — a single global value per layer, not a spatial field. This is an
acceptable simplification for a first pass (no artist-authored weather needed) but means there is
currently no mechanism for "clouds thicken toward that mountain range" — everything is a uniform deck
modulated only by the noise texture itself.

## 2. Height gradient per cloud type

### Schneider 2015 (slide 33): 3 presets, blended by weathermap type

The talk states there are 3 mathematical height-gradient presets (stratus / stratocumulus / cumulus)
blended by the type value, but the *exact* per-preset curve is not published as a formula in the
deck — only the qualitative behaviour: density increases with altitude near the base then falls off
near the top, and coverage further suppresses density at the very bottom ("wispy bottoms", slide 35).

### Nubis³ / Envelope method (slide 20, EXACT — use this as the canonical formula)

```c
float height_fraction  = remap(height, min_height, max_height, 0.0, 1.0);
float top_gradient     = pow(1.0 - height_fraction, 1.5);   // falls off toward the top
float bottom_gradient  = pow(height_fraction, 2.0);          // rises from the base
float dimensional_profile = bottom_gradient * top_gradient;  // (× edge_gradient for the horizon falloff)
```

Different cloud types are obtained by varying `min_height`/`max_height` (the envelope) and the two
exponents (1.5 / 2.0 are Nubis³'s defaults for a generic cumulus-like envelope) — a flatter stratus
uses a much shallower `top_gradient` exponent (density stays high right up to a low ceiling) and a
tall cumulonimbus stretches `max_height` far above `min_height` with a slower top falloff.

### FlightBox's current formula (`FBRenderer.cpp:1274-1279`)

```wgsl
fn heightGrad(h: f32, low: f32, high: f32) -> f32 {
  let cumulus = smoothstep(0.0, 0.12, h) * smoothstep(1.0, 0.55, h);
  let stratus = smoothstep(0.0, 0.08, h) * smoothstep(0.55, 0.25, h);
  return mix(cumulus, stratus, low);
}
```

Structurally the same idea (rise-then-fall gradient over normalized height `h`), using `smoothstep`
instead of `pow` — a reasonable substitution. **What's missing vs. Nubis³: no `edge_gradient`
equivalent** — Nubis³ multiplies by a falloff toward the 35 km draw-distance edge (§ noise doc) so
distant cloud mass fades before it hits a hard horizon cutoff; FlightBox's shell march instead just
caps `tEnd` at 240 km with no density falloff, which can produce a visible density "wall" at the cap.

## 3. Real-world height bands (Schneider 2015, slide 72 — EXACT numbers)

| Class | Altitude band | Render method |
|---|---|---|
| **Strato** (volumetric, our concern) | **1500 m – 4000 m** | Full raymarch through a spherical shell |
| **Alto/Cirro** (high) | **above 4000 m** | 2D scrolling texture card, NOT volumetric (too thin to be worth raymarching) |

FlightBox's cloud-deck placement (`FBRenderer.cpp:1541-1542`): `baseAGL = cloud_base or 1500 m`,
`topAGL = baseAGL + 2600 + 1400*cloud_high` → top ranges **4100–5500 m AGL**. This **exceeds
Schneider's 4000 m strato ceiling** by up to 1500 m when `cloud_high` is large — the current single
volumetric shell is being asked to represent both strato AND alto/cirro cloud simultaneously. Per the
source, that upper band should be a flat 2D card, not part of the volumetric march (cheaper, and
visually distinct — high cirrus doesn't have the same billowy 3D structure).

## 4. Absolute extinction — physical values (NOT from the game talks; standard atmospheric optics)

The SIGGRAPH talks work entirely in **unitless density** (`[0,1]` noise composite × art-directed scale)
because game clouds are art-directed, not measured. For a *physically anchored* starting point:

| Cloud type | Extinction coefficient σ (1/m) | Optical depth over 100 m |
|---|---|---|
| Thin stratus | 0.01 – 0.03 | 1.0 – 3.0 |
| Cumulus (typical) | **0.04 – 0.12** | 4 – 12 (essentially opaque beyond ~50 m) |
| Cumulonimbus (dense core) | 0.1 – 0.3 | fully opaque within 10–30 m |

These figures come from cloud liquid-water-content-derived Mie extinction (standard result:
σ ≈ 0.75 · LWC / (ρ_water · r_eff), with LWC ~0.3–1 g/m³ and effective droplet radius ~10 μm giving the
0.04–0.12/m cumulus range above) — cited here as an anchor, not re-derived; treat as **order-of-
magnitude**, not a precision figure, since real clouds vary by an order of magnitude with LWC.

**FlightBox's `kExtinct = 0.06` (`FBRenderer.cpp:1263`) sits exactly in the physical cumulus range** —
good. The problem is `C.p2.w` ("density scale", set to **5.0**, `FBRenderer.cpp:1556`) is multiplied
onto the ALREADY-saturated `[0,1]` density (`FBRenderer.cpp:1298`, `return clamp(d, 0.0, 1.0) * C.p2.w`)
— so the variable actually carries values up to **5.0**, and `sigma = dens * kExtinct` reaches **0.3/m**,
i.e. cumulonimbus-core optical density used uniformly for a coverage-driven "pleasant default deck."
This inflates the effective extinction by 5× versus what a physically-tuned `kExtinct` alone would
give, and does so silently (the variable is still named/treated as if it were the `[0,1]` density in
the powder/erosion math a few lines earlier). See [09-current-state-gaps.md](09-current-state-gaps.md).

## 5. Coverage → density remap (the actual "turn up the fill" control)

Schneider 2015 (slide 34–35) and Nubis³ (slide 24, § noise doc above) agree: coverage does **not**
scale density multiplicatively — it **shifts the remap threshold**. FlightBox already does this
correctly: `remap(shape, 1.0 - C.p0.z, 1.0, 0.0, 1.0)` (`FBRenderer.cpp:1289`, where `C.p0.z` is
coverage) is precisely the Nubis³ pattern applied to coverage instead of (or in addition to) the
dimensional profile. This part of the implementation is faithful to the source.
