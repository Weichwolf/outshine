# Real-Time Volumetric Clouds + Haze — Research Index

Implementation-ready distillation of the state of the art for real-time volumetric cloud and
haze/aerial-perspective rendering on a weak iGPU (target: Intel Iris Xe, ~1.5–2.2 TFLOPS FP32,
~50 GB/s shared memory — see [07-igpu-performance-budget.md](07-igpu-performance-budget.md) for the
exact comparison class). Written for FlightBox's WebGPU/WGSL renderer: 720p fixed frame target,
camera-relative WGS84-ECEF world space, Hillaire (EGSR 2020) transmittance + sky-view + aerial-
perspective LUTs already implemented (`command_center/fb/FBRenderer.cpp`).

This is a **research synthesis**, not a source transcription — every formula/number below is cited to
its primary talk/paper/course inline; where two sources disagree or a number is estimated rather than
measured, that is flagged explicitly rather than silently averaged.

## Primary sources (full list, cited per-section below)

| Source | What it gives us | Slide/page anchor used in citations |
|---|---|---|
| Andrew Schneider & Nathan Vos, **"The Real-Time Volumetric Cloudscapes of Horizon Zero Dawn"**, SIGGRAPH 2015 Advances in Real-Time Rendering course. PDF: `guerrilla-games.com/read/the-real-time-volumetric-cloudscapes-of-horizon-zero-dawn` | Perlin-Worley noise construction, coverage/height-gradient remap, Beer-Powder lighting, 6-cone light march, quarter-res + 1/16-pixel temporal scheme | Slide numbers as printed in the deck footer (1–96) |
| Andrew Schneider, **"Nubis³"** (SIGGRAPH 2023 Advances course, includes a full recap of "Nubis, Evolved" SIGGRAPH 2022). PDF: `guerrilla-games.com/read/nubis-cubed` | Exact `remap()`/erosion code (`cloud_density = saturate(noise - (1.0 - dimensional_profile))`), envelope vs. vertical-profile methods, cone-step distance-field march acceleration, two-pass near/far resolution split, PS4/PS5 timing numbers | Slide numbers as printed (1–220, cloud content in 1–60) |
| Sébastien Hillaire, **"Physically Based Sky, Atmosphere and Cloud Rendering in Frostbite"**, SIGGRAPH 2016 "Physically Based Shading" course. Deck: `blog.selfshadow.com/publications/s2016-shading-course/hillaire/` | Energy-conserving analytic scattering integration (already in our sky-view LUT), 2-lobe HG fit (**g0=0.8, g1=−0.5, lerp=0.5** — exact), Wrenninge multi-octave multi-scatter citation, cloud/aerial-perspective coupling scheme, XB1 perf numbers, Sunny-16 exposure anchor | Slide numbers as printed (1–64) |
| Sébastien Hillaire, **"A Scalable and Production Ready Sky and Atmosphere Rendering Technique"**, EGSR 2020 | The transmittance/sky-view/aerial-perspective LUT scheme — **already implemented verbatim** in `FBRenderer.cpp` (`kAtmoCommon`/`kTransmittanceCS`/`kSkyViewCS`). Referenced, not re-derived. | n/a — see our own code |
| Magnus Wrenninge, **"Oz: The Great and Volumetric"**, SIGGRAPH 2010 Talks (cited as `[Wrenninge10]` by Hillaire 2016; a longer treatment appeared later as "Art-Directable Multiple Volumetric Scattering", SIGGRAPH 2013 Talks) | Multi-octave multiple-scattering approximation (attenuate extinction/phase-sharpness/contribution per octave) | Cited via Hillaire 2016 slide 61–62; exact per-octave multipliers are not published — see note in [03-lighting-model.md](03-lighting-model.md) |
| Koschmieder (1924/1925 formalization; cited via the atmospheric-visibility literature, e.g. Horvath 1971 *Atmospheric Environment*) | `σ = 3.912 / VIS` — extinction coefficient from meteorological visibility | n/a — a single formula, widely reproduced |
| Maxime Heckel, **"Real-time dreamy Cloudscapes with Volumetric Raymarching"** (blog, secondary/pedagogical) | A minimal worked GLSL implementation with concrete numbers (step sizes, FBM lacunarity/gain) — useful as a sanity-check reference, not a primary source | n/a |
| Reinder Nijhoff, **"Volumetric clouds: Himalayas"** (blog on the `MdGfzh` Shadertoy) | Confirms the 128³/32³ two-texture split used verbatim from Schneider 2015; no additional numeric detail beyond that | n/a |

## Files

| File | Section | Content |
|---|---|---|
| [01-noise-construction.md](01-noise-construction.md) | A | Perlin-Worley recipe: channels, octaves, frequencies in km, tileability, detail erosion |
| [02-density-coverage.md](02-density-coverage.md) | B | Coverage/weathermap, height-gradient formulas per cloud type, absolute extinction values |
| [03-lighting-model.md](03-lighting-model.md) | C | Beer-Lambert, dual-lobe HG (exact g-values), powder, Wrenninge octaves, ambient, 6-cone light march |
| [04-raymarch-strategy.md](04-raymarch-strategy.md) | D | Step counts/strategies, cone-step acceleration, blue-noise jitter, resolution tiers |
| [05-temporal-reprojection.md](05-temporal-reprojection.md) | E | Camera-only reprojection math, history clamp/reject, 4×4 Bayer vs. half-res schemes, convergence |
| [06-haze-aerial-perspective.md](06-haze-aerial-perspective.md) | F | Exponential height fog + Mie, Koschmieder VIS→σ, coupling to the Hillaire aerial-perspective LUT without double-hazing |
| [07-igpu-performance-budget.md](07-igpu-performance-budget.md) | G | Iris Xe vs. PS4/XB1-class budget, WGSL `f16`, 3D-texture bandwidth, what does NOT fit |
| [08-experiment-protocol.md](08-experiment-protocol.md) | H | Parameter-sweep rig, reference-photo comparison, "looks like a cloud" checklist |
| [09-current-state-gaps.md](09-current-state-gaps.md) | — | The 5 concrete ways `command_center/fb/FBRenderer.cpp`'s current cloud pass violates the above, with file:line references |

See [PROGRESS.md](PROGRESS.md) for source-coverage tracking.
