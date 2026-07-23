# C — Lighting Model: Beer-Lambert, Dual-Lobe HG, Powder, Multi-Scatter, Ambient, Light March

Source: Schneider & Vos, SIGGRAPH 2015 (slides 48–87, EXACT quotes below); Hillaire, SIGGRAPH 2016
Frostbite course (slides 27–43, 54–62, EXACT `g`-values below); Wrenninge's multi-octave scheme is
cited by Hillaire (slide 61 `[Wrenninge10]`) but the per-octave multiplier constants are not published
in any of the primary decks — flagged as estimated where used.

## 1. Beer's Law (transmittance)

`T = exp(-σ·d)` — optical thickness d through the medium (Schneider 2015, slide 51). This is the
foundation; both the sun-ward light march and the primary view-ray march use it. **FlightBox does
this correctly** (`FBRenderer.cpp:1398`, `let tr = exp(-sigma * stepM);`).

## 2. Dual-lobe Henyey-Greenstein — EXACT VALUES (Hillaire 2016, slides 35–38)

```
HG phase with g = 0.8                              (forward-scattering silver-lining lobe)
2-lobe HG phase with g0 = 0.8, g1 = -0.5, lerp = 0.5   (adds a back-scattering lobe, mixed 50/50)
```

This is a direct quote from the Frostbite deck — Hillaire explicitly tried fitting a single HG lobe to
measured cloud phase functions and found back-scattering was still not visible enough, hence the
2-lobe mix. **Use `g0 = 0.8, g1 = -0.5, lerp = 0.5` as the reference values** unless a specific look
needs retuning.

FlightBox currently uses **`g = 0.75` (forward) / `g = -0.4` (back), mixed at `0.45`**
(`FBRenderer.cpp:1363`, `mix(hgPhase(cosT, 0.75), hgPhase(cosT, -0.4), 0.45)`) — close to the Hillaire
reference but not identical; not a likely cause of the "static" artifact, but worth aligning to the
cited values (`0.8` / `-0.5` / `0.5`) since they are a *measured* fit, not an arbitrary choice.

Schneider 2015 does not give an explicit `g` value in the deck text (slides 53–55 show the visual
comparison only) — Hillaire's numbers are the best-sourced concrete figures available.

## 3. Powder (Beer-Powder) — Schneider 2015, slides 56–67 (concept EXACT, formula NOT published)

The "powder sugar" effect: light gathered *inside* a cloud from nearby in-scattering makes crevices
appear brighter than convex bulges/edges facing the viewer — the opposite of what plain Beer's law
alone predicts (which darkens uniformly with depth, but never brightens concavities). Schneider
explicitly says: "I am still looking for the Beer's-Powder approximation method in the ACM digital
library and I haven't found anything mentioned with that name yet" (slide 61) — **there is no
published closed-form powder formula from the primary source**; only the qualitative requirement:

- it must be **view-dependent** — visible only where the view vector approaches the light vector
  (slide 65: "we only see it where our view vector approaches the light vector")
- it must strengthen with depth/density (more in-scattering deeper in the cloud)
- rain clouds get *additional* darkening on top of this via increased absorption (slide 67)

The widely reproduced practical form (not from a primary talk, but the standard implementation seen
across UE4/production shaders that cite Schneider) is:

```wgsl
let powder = 1.0 - exp(-density * kPowderScale);   // kPowderScale commonly 1-3 in unitless-density schemes
```

**FlightBox's powder term** (`FBRenderer.cpp:1394`, `1.0 - exp(-dens * 6.0)`) uses the same functional
form; `dens` here can reach 5.0 due to the density-scale issue in
[02-density-coverage.md §4](02-density-coverage.md), so `powder` saturates to ~1.0 almost everywhere
inside any visible cloud — the powder term is likely contributing **no gradient at all** currently
(always ≈1, i.e. `(0.25 + 0.75·powder) ≈ 1.0` constant), silently degrading to plain Beer's law +
constant. This is consistent with a "looks flat/noisy, not shaped" symptom.

## 4. Multi-scattering approximation — Wrenninge octaves (cited by Hillaire 2016, slides 61–62)

Hillaire: **"Use [Wrenninge10]: multi-octaves single scattering / extinction / phase"** — i.e. run the
*same* single-scattering formula (Beer-Lambert × HG phase) several times ("octaves") with attenuated
extinction, sharpened-toward-isotropic phase, and attenuated contribution, and sum:

```wgsl
var scatter = vec3f(0.0);
var atten = 1.0;      // extinction attenuation per octave
var eccAtten = 1.0;   // phase-sharpness attenuation per octave (mixes phase toward isotropic)
var contrib = 1.0;    // energy contribution per octave (must decay <1 to converge)
for (var i = 0; i < 3; i++) {           // typically 2-4 octaves is enough
  let msPhase = mix(isotropic, phase, eccAtten);
  scatter += contrib * msPhase * exp(-opticalDepth * extinction * atten);
  atten *= 0.5; eccAtten *= 0.5; contrib *= 0.5;   // exact multipliers NOT published by any primary source
}
```

The `0.5/0.5/0.5` triple is the value commonly reproduced in practice and is architecturally what
Wrenninge's talk describes (geometric falloff per octave, analogous to fBm's persistence), but **no
primary source in this research set publishes the exact numeric constants** — treat any specific
number (including FlightBox's own `0.5 / 0.5 / 0.55`, `FBRenderer.cpp:1388-1392`) as **art-directed,
not a measured physical constant**. FlightBox's implementation already matches this scheme
structurally; this is one of the parts of the current code that is *not* a likely source of the
artifact.

## 5. Ambient (sky) contribution

- **Schneider 2015** (slide 87): "Ambient sky contribution increases over height... We add up our
  ambient and direct components and attenuate to the atmosphere color based on the depth channel."
- **Hillaire 2016** (slide 60): approximates ambient as "integrate luminance to scatter" from the sky
  environment (their sky-view LUT, exactly what FlightBox already has), with two stated approximations:
  phase function treated as uniform (isotropic), and a single environment color ignoring cloud
  self-occlusion.
- **Nubis³** (slide 32, EXACT): `float ambient_scattering = pow(1.0 - dimensional_profile, 0.5);` — i.e.
  ambient light penetration uses the SAME dimensional-profile field as density, just inverted and
  square-rooted (steeper falloff near the surface, matching "majority of light penetrates just the
  surface" reasoning, slide 33).

FlightBox's ambient term (`FBRenderer.cpp:1372-1373, 1396`) samples the sky-view LUT twice (zenith +
horizon-toward-sun) and blends by height `h` — architecturally matches Hillaire's "sample the sky
environment" approach (this is the right LUT to reuse, since it's already Hillaire-correct). It does
**not** use the Nubis³ `pow(1-profile, 0.5)` gradient — worth adding as a multiplier on top of the
existing sky-sample so ambient also fades correctly toward the cloud core, not just by absolute height.

## 6. The 6-cone light march (Schneider 2015, slides 81–84 — EXACT)

> "In our approach, we sample 6 times in a cone toward the sun. This smooths the banding we would
> normally get with 6 [straight] samples and weights our lighting function with neighboring density
> values, which creates a nice ambient effect. The last sample is placed far away from the rest in
> order to capture shadows cast by distant clouds." (slide 82)
>
> "To improve performance... we switched to sampling the cheap version of our shader once the alpha
> of the image reached 0.3, [making] the shader 2x faster." (slide 83)

Concretely: **5 near samples in a cone pattern + 1 distant sample** (not 6 equally-spaced samples along
a line) toward the sun, accumulating optical depth for Beer's law; once the *view-ray* alpha (not the
light-ray alpha) passes **0.3**, switch the light samples from the expensive (full-detail-noise)
density function to the cheap (base-shape-only) one.

FlightBox's `sunOD()` (`FBRenderer.cpp:1304-1317`) takes **6 samples along a straight ray with
doubling step size** (100 m → 200 m → 400 m → ... over 6 steps ≈ 6.3 km total reach) — a reasonable
"exponential cone" approximation of the "near samples dense, far sample distant" idea, but it is a
**single straight ray**, not an actual cone (no lateral jitter/spread of the 5 near samples), so it
gets the distance profile right but not the banding-smoothing Schneider attributes to the cone spread.
It also always uses the full (expensive) `density()` function regardless of view-ray alpha — the 0.3
cheap-switch optimization is not implemented (a performance opportunity, not a correctness bug, on an
Iris-Xe budget where the light march runs once per *primary* march step, i.e. potentially 160× per
pixel — see [07-igpu-performance-budget.md](07-igpu-performance-budget.md)).

## 7. Rain-cloud darkening (Schneider 2015, slide 67)

Precipitation increases absorption for cumulonimbus specifically — not implemented, and not currently
relevant to FlightBox (no per-cell precipitation signal exists; see
[02-density-coverage.md §1](02-density-coverage.md)).

## 8. Energy-conserving integration (Hillaire 2016, slides 29–33 — directly relevant, and already used correctly elsewhere in this codebase)

The naive per-step accumulation `scattering += sampledLuminance * transmittance; transmittance *=
sampleTransmittance` is **not energy conservative** and over-darkens dense media (Hillaire 2016, slide
30 labels this explicitly "Wrong?", slide 31 "Over attenuation, dark media"). The fix — already
implemented in FlightBox's own sky-view LUT (`FBRenderer.cpp:284`, `kSkyViewCS`) — is the analytic
per-step integral:

```
scatteringIntegral = (sampledScattering - sampledScattering * sampleTransmittance) / extinction
scattering += scatteringIntegral * transmittance
```

The cloud pass's own integration (`FBRenderer.cpp:1398-1400`, `scat += transm * srcLum * (1.0 - tr)`)
is the algebraically equivalent form **when `srcLum` is treated as an already-normalized per-step
radiance source** (as opposed to a raw scattering coefficient needing division by extinction) — this
is consistent and correct, not a bug. Worth stating explicitly since it is easy to mis-flag as the
"naive" form Hillaire warns against; it is not.
