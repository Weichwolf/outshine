# F — Haze / Fog / Aerial Perspective: Coupling Without Double-Hazing

Source: Koschmieder relation (standard atmospheric-optics result, widely cited e.g. Horvath 1971,
*Atmospheric Environment*); Hillaire SIGGRAPH 2016 Frostbite course (slides 39–43, 56, EXACT — the
cloud/aerial-perspective coupling scheme and height-fog approximation); Hillaire EGSR 2020 (already
implemented in FlightBox verbatim — referenced, not re-derived).

## 1. Koschmieder: VIS → extinction coefficient

```
σ = 3.912 / VIS
```

`VIS` = meteorological visibility (range at which a large black object against the horizon sky becomes
indistinguishable — the standard METAR/TAF "visibility" figure), `σ` = extinction coefficient (1/length,
same units as VIS⁻¹). The constant 3.912 = −ln(0.02): assumes the human contrast-detection threshold
is 2% (Duntley/Koschmieder's original derivation, reproduced in the visibility literature). This holds
only under the idealized conditions the model assumes: homogeneous illumination, spatially uniform
extinction, an ideally black target viewed against the horizon sky, constant eye contrast threshold —
i.e. it's the right ballpark formula for a first-pass fog model, not a rigorous radiative-transfer
result.

| VIS | σ (1/m) | σ (1/km) | Regime |
|---|---|---|---|
| 50 km (very clear) | 7.8×10⁻⁵ | 0.078 | Clear day |
| 10 km (typical haze) | 3.9×10⁻⁴ | 0.39 | Standard "good" visibility |
| **5 km** | **7.8×10⁻⁴** | **0.78** | Given in the task brief as the "must not go to milk" test case |
| 1 km (fog) | 3.9×10⁻³ | 3.9 | Fog |
| 200 m (dense fog) | 1.96×10⁻² | 19.6 | Dense fog |

At VIS = 5 km: transmittance over a 5 km sightline is exactly `exp(-3.912) ≈ 2%` by construction (that
IS the definition of the visibility range) — over a 1 km sightline, `exp(-0.78) ≈ 46%` transmittance;
over 10 km, `exp(-7.8×10⁻³×1000)... ` wait — recompute: over 10 km at σ=7.8×10⁻⁴/m, optical depth =
7.8 → transmittance ≈ 0.04%, i.e. essentially opaque. **This is the sanity check the brief asks for:**
a scene at VIS 5 km should have terrain/clouds beyond ~10–15 km essentially fully obscured by haze, and
should look progressively hazier (not uniformly milky-white) as distance increases — "milk" happens
when the ambient/inscatter term added at each distance is too bright relative to the transmittance
term, independent of σ (see §3).

## 2. Exponential height fog + Mie component

Standard atmospheric height-fog model (used across Hillaire 2016 slide 56, Bruneton 2008, and
essentially every production renderer with height fog):

```wgsl
fn heightFogDensity(y: f32, y0: f32, falloff: f32) -> f32 {
  return exp(-(y - y0) / falloff);   // y0 = reference altitude, falloff = scale height
}
```

integrated along the view ray to give optical depth, then Beer's law as in
[03-lighting-model.md](03-lighting-model.md). The **Mie component** (larger aerosol particles —
haze/pollution/dust, as opposed to Rayleigh's molecular scattering) is already correctly present in
FlightBox's Hillaire implementation: `mieScatteringBase = 3.996`, `mieDensity = exp(-altitudeKM/1.2)`
(`FBRenderer.cpp:180, 193`) — **this is the Hillaire EGSR2020 reference atmosphere's standard Mie
scale height (1.2 km) and coefficient**, already correct and not something to re-derive.

## 3. Hillaire 2016's height-fog approximation (slide 56, EXACT — the "why it goes milky" mechanism)

> "Uses scattered luminance at horizon. Seamless sky/fog transition. Per pixel coverage. Per pixel
> phase function. Approximations: Assumes low altitude fog. Ignores opaque shadows and self-shadowing.
> `ScatteredLight(P) ≈ ScatteredLight(Horizon)`"

I.e. the practical approximation samples the **sky-view LUT at the horizon direction** as a stand-in
for "how much ambient light does the fog itself scatter toward the camera at this depth" — this
inscatter term grows toward the sky's horizon brightness as optical depth increases, exactly the
formula FlightBox's terrain aerial-perspective already implements (`FBRenderer.cpp:454`, `let inscat =
skyViewSample(...) * (1.0 - viewTrans);`). **"Milky at VIS 5 km" happens when this inscatter term is
too bright relative to how quickly `viewTrans` collapses** — since `viewTrans` here is a RATIO of two
Hillaire-atmosphere transmittances (parametrized for the ~100 km-scale Earth atmosphere, not a
5 km-VIS ground fog), it does **not** currently respond to a VIS/Koschmieder input at all — there is no
`σ_haze` term feeding into `viewTrans` today (`FBRenderer.cpp:451-453` uses only the Rayleigh/Mie/ozone
atmosphere, no separate boundary-layer haze channel). **A VIS setting today literally cannot make the
scene hazier or clearer** — see [09-current-state-gaps.md](09-current-state-gaps.md).

## 4. Coupling clouds into the SAME aerial-perspective term, without doubling it (Hillaire 2016, slides 39–43 — EXACT scheme)

Two distinct problems, both explicitly addressed in the Frostbite deck:

**(a) Aerial perspective ON the cloud surface itself** (fog between camera and the cloud): "Per sample:
expensive to evaluate. Instead sampled once on cloud front interface. Compute mean depth on cloud
interface weighted by transmittance." (slide 40) — i.e. don't apply aerial-perspective fog per
raymarch-step inside the cloud (expensive, and wrong — the fog is between camera and cloud, not
inside it); compute ONE effective depth (transmittance-weighted mean of where the ray first becomes
optically significant) and apply the standard terrain-style aerial-perspective lookup once, using that
depth, exactly as if the cloud surface were opaque terrain at that depth.

**(b) Clouds influencing the SKY's aerial perspective / ambient** (cloud shadow + cloud mass changing
the background sky brightness that OTHER aerial perspective calculations sample): "Cloud hemisphere
sampling around camera → integrate cloud luminance / mean cloud transmittance → `New AP scattered
luminance = oldAP × transmittance + luminance`" (slides 42–43) — a small hemisphere-sampled LUT of
"how much cloud is overhead and how bright is it" gets folded into the same aerial-perspective
scattered-luminance term other passes (terrain, transparents) already sample, via a simple lerp:
`newAP = oldAP * cloudTransmittance + cloudLuminance`. This is exactly how they avoid "double haze":
there's still only ONE aerial-perspective term per pixel; cloud contribution modulates it rather than
being added as an independent second fog layer.

**FlightBox does neither (a) nor (b) today** — the cloud pass and the terrain aerial-perspective term
are computed completely independently and composited in the tonemap pass with a simple `alpha`-blend
(`FBRenderer.cpp:774`, `scene = scene * (1.0 - cl.a) + cl.rgb`). This is architecturally fine as a
first pass (it does NOT double-haze in the sense of applying two different fog functions to the same
ray-segment — the cloud's own in-scattering already accounts for its own optical depth, and the
terrain's aerial perspective is unaware of the cloud, so there is no literal double-counting) but it
also means clouds cast **no shadow/darkening on the terrain aerial-perspective term**, and the terrain
haze doesn't dim a cloud deck sitting in front of a hazy background — both are physically present in
the (a)/(b) scheme above and both are currently absent.

## 5. Recommendation

1. Add a **separate boundary-layer haze term** driven by Koschmieder's `σ = 3.912/VIS` (§1), as an
   exponential-height-fog Beer's-law term (§2) evaluated per-pixel on the terrain pass, **in addition
   to** (not replacing) the existing Hillaire aerial-perspective ratio — this is what makes a VIS
   setting actually do something. Anchor its inscatter color to the SAME sky-view LUT horizon sample
   Hillaire's approximation already uses (§3) so it blends seamlessly rather than introducing a second,
   differently-colored haze.
2. For clouds-in-front-of-terrain: apply the existing terrain aerial-perspective lookup ONCE at the
   cloud's transmittance-weighted mean depth (§4a) rather than leaving clouds and terrain haze
   compositionally independent, if/when cloud-behind-haze layering becomes visually important (e.g.
   distant deck glimpsed through ground haze).
3. Cloud-modulating-sky-ambient (§4b) is a nice-to-have, lower priority than §1/§2 for the current
   "looks like static" bug — it affects consistency under varying weather, not the primary artifact.
