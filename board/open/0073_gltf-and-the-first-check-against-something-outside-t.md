Type: feature
Area: corpus
Tags: oracle, khronos, perf

**I.26 glTF, and the first check against something outside this tree**

*Added 2026-08-12 on the owner's ruling. This file's own measurement rule ranks* correctness — checked
against something outside *above* consistency — two parts of this tree agree*, and says another digit of
internal agreement is worth less than the first external check. **This repository has never had one.**
Rendering a glTF scene here and the same scene in Blender is that check. Blender 5.2.0 LTS is on this
host (build 2026-07-14) and runs headless.*

**Four rungs, and each isolates one thing.** The order is the design, because a light comparison whose
camera is half a pixel out produces a residual at every silhouette that nobody can attribute.

| Rung | What it compares | Needs |
|---|---|---|
| 1 | **coverage** — a binary mask, no light at all | the reader, nothing else |
| 2 | **depth** — linear view-space range | an existing readback |
| 3 | **direct diffuse radiance** — linear, pre-tone-map | a linear tap that does not exist |
| 4 | **shadow and indirect** — as a bias curve, never a verdict | rung 3 |

**Rung 3 is three-way, not two-way.** For a flat facet under one directional light the answer is closed
form — `L = ρ·E·cos θ / π` — so the referee is arithmetic and Blender is the tie-breaker on what
arithmetic cannot reach. That matters because Cycles is not ground truth everywhere: its own Principled
BSDF carries open energy-conservation issues and the Glass BSDF fails the white-furnace test
(blender/blender #158426, #159635), so the oracle is pinned to the lobe it is known-good on.

**Blender's default lighting, read and not recalled.** *Owner's ruling, 2026-08-12, two clauses: **match
Blender rather than making Blender match us** — our scenario declares whatever Blender's defaults
already are, so the reference is configured by* not *being configured — and **Blender is open source, so
read what Cycles computes** rather than inferring it from renders. Everything below was queried from the
shipping binary with `blender --factory-startup --background` (**5.2.0 LTS, hash `fbe6228777e7`, built
2026-07-14**) or read in the source that performs the conversion. The app bundle ships the Cycles kernel
headers whole at `…/Blender.app/Contents/Resources/5.2/scripts/addons_core/cycles/source/kernel/`, which
is the exact kernel this binary runs; host-side paths are `intern/cycles/…` at tag `v5.2.0`.*

| Quantity | Factory value | Where the number is |
|---|---|---|
| key light | `POINT`, power **1000 W**, colour (1,1,1), radius **0.1 m**, at **(4.076245, 1.005454, 5.903862)**, `normalize` on, `exposure` 0 | startup scene, queried |
| what "Power" becomes | `strength = colour · energy · 2^exposure` — the only place a watt enters Cycles | `blender/light.cpp:58` |
| point radiance | `area = 4πr²`, `invarea = normalize ? 1/area : 1`, `eval_fac = invarea/π` → **2533.0 W·m⁻²·sr⁻¹**, total flux 1000 W, intensity **79.577 W/sr** | `scene/light.cpp:132-145` |
| its irradiance at the origin | **1.51627 W/m²** perpendicular at d = 7.244467 m. A uniform sphere fully above the horizon gives *exactly* the point-source value, so there is **no area-light approximation** to carry | derived from the two rows above |
| world | `Background` node, colour **0.05087608844041824** linear on all three channels, strength **1.0** | startup scene, queried |
| is the world a light? | **Yes, by default.** `sampling_method` is `AUTOMATIC` and `sample_as_light = (method != NONE)`; a `BackgroundLight` is created with MIS on whenever a world exists | `blender/light.cpp:90-93,136` |
| what that ambient is worth | a uniform environment of radiance `L` puts exactly **`ρ·L`** out of an unoccluded facet — no π — hence **10.5 %** of the key at normal incidence at the origin, and **all** of the light on the unlit side | derived |
| camera | 50 mm, sensor 36 mm, fit `AUTO`, clip 0.1/100 m, at (7.358891, −6.925791, 4.958309), Euler XYZ (1.109319, 0, 0.814928); `AUTO` fits the larger raster dimension, so at 1920×1080 **hfov 39.5978°**, vfov 22.8952° | startup scene; `blender/camera.cpp:415-425`, `:674` `fov = 2·atan((0.5·sensor)/lens/aspect)` |
| subject | cube of ±1 m, 6 flat-shaded quads, 8 vertices | startup scene, queried |
| engine | **`BLENDER_EEVEE`** — Cycles is *not* the factory renderer | startup scene |
| view transform | **AgX**, display sRGB, exposure 0, gamma 1 | startup scene |
| sampling | 4096 samples, adaptive on at 0.01, **denoising on**, Blackman-Harris **1.5 px**, `max_bounces` 12 / diffuse 4, `film_exposure` 1.0, light tree on, `sample_clamp_indirect` 10.0 | startup scene |
| a newly added lamp | every type defaults to **10 W**; `SUN` angle **0.00918043 rad = 0.526°** | queried |
| **Sun lamp units** | `SunLight::area = π·sin²(angle/2)`, `eval_fac = 1/area`, so the disk's irradiance on a perpendicular surface is `strength` **exactly and independently of the angle** — Blender's Sun Strength *is* W/m² perpendicular to the beam | `scene/light.cpp:298`, `:316` |
| Diffuse BSDF | `max(dot(N,ω),0)·(1/π)` times the closure weight — **exactly Lambertian**, and the closure carries no roughness parameter at all | `kernel/closure/bsdf_diffuse.h:46` |
| pixel filter | box is the constant 1; the importance table spans `[0, width/2]` symmetric, and `raster = (x,y)` **before** the table offset is added — so **the integer raster coordinate is the pixel centre** | `scene/film.cpp:26-29`, `:74-81`; `kernel/camera/camera.h:458-464` |
| narrowest filter reachable | `pixel_filter_type='BOX'` with `filter_width` at its RNA minimum **0.01 px** → every sample within **±0.005 px** of the pixel centre | `cycles/properties.py:875-888` |
| transparent film | applies only to a ray carrying `PATH_RAY_TRANSPARENT_BACKGROUND`, i.e. camera rays, and writes alpha — **an exact coverage channel that does not touch the lighting** | `kernel/integrator/shade_background.h:103-107` |

- [ ] **The factory values the harness does not set are declared once, not re-queried and gated.** *Relaxed 2026-08-12 on the owner's ruling — **we do not aim to be bit-identical with Blender, so the exact version does not matter much**, and a gate that refuses a run because a factory default moved defends an exactness we are not claiming. What the table above is, after the relaxation: **the derivation of the numbers we do set**, read in the source once so that nobody re-derives them from memory. It is documentation of a reading, not a contract with a binary.* The `blender --factory-startup` flag stays, because *configured by not being configured* still removes a whole class of accidental divergence — it just is not policed
- [ ] The deviations from the factory startup are a **closed, reasoned list**, and there are six: **engine** `BLENDER_EEVEE → CYCLES` (unavoidable — the oracle is the path tracer) · **samples** fixed and `use_adaptive_sampling` off (4096 with an adaptive threshold is not a reproducible number) · **denoising off** (a denoiser is an estimator with no error bar) · **pixel filter** `BOX` at 0.01 px for the geometric rungs (§ below) · **output** OpenEXR float32, which ignores the view transform by Blender's own colour-management rule and thereby deletes AgX in one move · **resolution** 1280×720, this engine's budget. Nothing else is touched, and in particular **the world colour, the world strength, `film_exposure`, `scale_length` and the sensor stay at their factory values**
- [ ] **The world stays Blender's factory world on every rung** — `0.05087608844041824` linear at strength 1.0, sampled as a light. It is the half of *default lighting* that costs nothing to match and it is the half most likely to be wrong here, because our ambient is a hemisphere over geodetic up and Blender's is a full sphere (§ II.8)
- [ ] **The key light is Blender's `SUN` lamp for the light rungs, and this is a declared deviation from the literal factory default, with its reason.** Blender's factory key is a point light and `src/render/` has no punctual light of any kind — `Gpu.h:22 SceneLight` is one irradiance pair, one cascade buffer and one shadow atlas — so matching the default literally would put the first external check this repository has ever had behind three unbuilt features on our side. The sun costs **no conversion at all**: Strength is W/m² perpendicular to the beam by `scene/light.cpp:298,316`, which is the unit `IrradianceStage`'s `sunDirectNormal` already carries, leaving exactly one unknown in the comparison — which is the unknown rung 3 exists to settle
- [ ] **The factory point light is not dropped, it is a rung** — scene 8 below is the 1000 W point at its factory position, and it is the rung that first requires the punctual light § II.8 already owes. The ladder drives that feature rather than the feature blocking the ladder
- [ ] **`KHR_lights_punctual` is refused as the light channel for rungs 1–7 as a simplification, not as a necessity** — and the reason on the previous version of this line was incomplete. The 683 lm/W factor is real and is in the shipped importer (`io_scene_gltf2/blender/com/conversion.py:10 PBR_WATTS_TO_LUMENS = 683`), but `blender/imp/light.py:57-77` applies it **only in mode `SPEC`**; `COMPAT` multiplies point-likes by 4π alone, and **`RAW` passes the number through unchanged**, in which case a glTF directional `intensity` becomes Blender Sun Strength one-to-one and is therefore W/m² perpendicular by the row above. `SPEC` is the default (`io_scene_gltf2/__init__.py:198-206`), so the factor is opt-out, not unavoidable
- [ ] `import_settings['export_import_convert_lighting_mode'] = 'RAW'` recorded on the row of any comparison that does let a light cross the glTF boundary, because the mode is the unit
- [ ] **The anti-aliasing objection is dissolved for the geometric rungs, and it is dissolved by the source rather than by tolerance.** With `BOX` at 0.01 px, Cycles evaluates the same predicate a centre-sampling rasteriser does — *is the pixel centre inside the primitive* — because the integer raster coordinate is the centre and the sample never leaves ±0.005 px of it. The residue is an **instrument floor of 0.005 px**, published beside the result, with an expected disagreement of ≈ 0.01 × the silhouette length in pixels; on a subject filling 30 % of 1280×720 that is **≈ 30 pixels at risk and ≈ 15 expected to flip**, and a run reporting more than the floor has found something
- [ ] Rung 1 therefore has **two** products and they answer different questions: the **binary mask** at 1 spp / box 0.01 against our centre-sampled coverage, whose acceptance is the floor above; and the **alpha coverage** at N spp / box 1.0, which is the analytic pixel-area fraction and gives a **signed sub-pixel edge offset** (`α = 0.5` is the edge through the centre) — the boundary metric with an error bar instead of a count
- [ ] Our side of rungs 1 and 2 needs **no colour tap at all**: `render/Renderer.h:66 ReadDepth` already returns reversed-Z float depth with its range conversion stated, so coverage is *depth ≠ far* and range is `kNearM/depth/cos(off-boresight)`. The two cheapest rungs are unblocked today and depend only on the reader and the studio scenario (§ I.25)
- [ ] **Indirect light stops being a confound and becomes a setting**: `diffuse_bounces = 0` makes Cycles' answer *exactly* the direct term, so rungs 5–8 are compared against closed form with no bounce to argue about, and rung 9's product is the 0-bounce against 4-bounce difference — the bias curve, which is the only actionable form
- [ ] Scenes 1–8 contain **one convex subject and no second surface**, so interreflection is identically zero by geometry and not merely by a setting. The first scene with a second surface is 9, and that is the scene where indirect is the subject
- [ ] Blender's own residual against the closed form is published beside ours on every radiance rung — the oracle states its error before it judges ours — and for the Diffuse BSDF that residual is expected to be Monte-Carlo noise alone, because `bsdf_diffuse.h:46` *is* the closed form

**What decides a rung, and why a Blender point release cannot move it.** *Owner's ruling, 2026-08-12:
**we are not aiming for bit-identity with Blender.** That relaxation is only safe if the tolerances are
written down and are visibly far above point-release noise — otherwise "we do not need exactness" is a
sentence that quietly absorbs a real regression. So the tolerances are stated here, in the document,
with the distance to the noise floor beside each.*

- [ ] **A release version pins the oracle, never a commit SHA.** `Blender 5.2 LTS` is the pin; the build hash is recorded for the record and is not a condition of the run. A build from a different point release is a valid oracle
- [ ] **The version is recorded on every comparison row, and that survives the relaxation intact** — not as an exactness claim but as the **attribution** of a red: when parity fails, one line answers whether our renderer moved or the oracle did, and without it that question costs a bisection
- [ ] **The version is part of the oracle cache key**, one string, and it prevents exactly one defect: the pin is bumped, scene and recipe unchanged, and the cache serves a render from the old Blender while the manifest claims the new one — **the manifest lying about its own output**. It is the same hash-is-the-filename discipline § I.22 already applies to a provider
- [ ] **The tolerances, and each is at least an order above point-release noise.** Rung 1 coverage: boundary p95 ≤ 0.5 px against an instrument floor of 0.005 px — **a factor of 100**. Rung 2 depth: p99 ≤ 1e-4 relative, against a float32 mantissa floor near 6e-8 — **three orders**. Rung 3 radiance: median relative difference ≤ 1 % against the closed form, where Monte-Carlo noise at a declared sample count is the only Blender-side term and is itself published. **Nothing at that scale moves because a point release changed a sampler**, and if a point release *did* move a number by 1 % it would be a finding about Cycles worth having
- [ ] **The 0.005 px instrument floor survives the relaxation untouched, because its justification is geometric and not empirical**: Cycles' box filter is the constant 1, the integer raster coordinate is the pixel centre, and `filter_width` floors at 0.01 px by its own RNA minimum. That argument does not mention a version
- [ ] **`Diffuse BSDF` at roughness 0, never Principled — and the relaxation *reinforces* this rather than loosening it.** Principled was rewritten in Cycles 4.0; a design leaning on it would be genuinely version-sensitive in exactly the quantity rung 3 measures. The Diffuse BSDF is `max(dot(N,ω),0)·(1/π)` times the closure weight and has no roughness parameter at all, so it is the one lobe whose value is a closed form rather than a model choice
- [ ] **Light and material never crossing the glTF boundary is kept and is not a version argument** — it is about importer semantics, and the lighting-mode factor that motivates it (`SPEC` versus `RAW`) is a mode, not a release
- [ ] **No acceptance anywhere is framed as pixel identity.** Every rung's product is a distribution, a residual shape or a bias curve, and a rung whose acceptance could only be written as *the images are equal* is a rung that was designed wrong
- [ ] **Cycles on Metal is not bit-reproducible at 4096 spp, measured, and this is evidence for the ruling above rather than a caveat under it.** Two runs of `render/coverage/triangle`'s `coverage` recipe at seed 0, everything else identical: **11 of 921 600 pixels differ (1.2e-5)**, max |Δ| **2.9e-11**. That delta is exactly `2^-35`, which is **one float32 ulp in the binade `[1.22e-4, 2.44e-4)`** — so the disagreement is confined to samples about four orders below the subject's own level and cannot be anything but the last bit of a near-black accumulation. **Alpha is bit-identical**, and the 1 spp `default` mask is bit-identical entire
- [ ] **What the derived cache key therefore promises: a valid render of that recipe, never identical bytes.** Two cache states can differ in the last bits of eleven pixels and both be correct, so a key is a statement about *provenance* and not about *content*. **Coverage scoring is unaffected by construction** — the mask reads alpha, alpha is bit-identical, and the radiance rungs' tolerances (≥ 1 % relative, § above) sit **nine orders** above 2.9e-11. *Stated here because the alternative reading — treating non-determinism as a defect to chase — would put a bit-exactness contract under an estimator that has none, and it is the exact shape `CLAUDE.md`'s determinism rule is **not** about: the rule binds our mathematics, and the oracle is a Monte-Carlo integrator by choice*
- [ ] **The derived cache key covers both Blender versions and neither alone is enough** — the **declared** version, so a manifest bump on an unchanged host misses, and the **observed** version and build hash, so a host that moved under an unchanged manifest misses. Plus every subject's file digest and the converted glTF's digest · the whole declared scene, camera, light, world and material · the whole render recipe · a derivation version · and the **product** — `exr`, `png`, `raw` — separately. Built the way `Data::ContentKey` builds one, newline-separated, and stored in the one global content store (§ I.22): `hash = filename`, no index, no sidecar, writes landing on a temporary and renamed so a name never precedes its bytes. **There is no second cache**

**IoU is reported and never enforced, because it is shape-driven.** *Owner's ruling, 2026-08-12, with
the derivation re-run here and confirmed exact. For two masks whose boundary is displaced by a mean δ,
the symmetric difference is a band of area ≈ P·δ, so `IoU ≈ 1 − P·δ/A` to first order — and `P/A` is a
property of the **subject's shape and size**, not of the renderer.*

*At 1280×720 with an equilateral triangle filling a quarter of the frame: `A = 230 400 px²`,
`s = √(4A/√3) = 729.5 px`, `P = 3s = 2 188 px`, so `P/A = 0.009498 /px`.*

| δ | IoU | what it is |
|---|---|---|
| 0.005 px | 0.99995 | the instrument floor |
| 0.05 px | 0.99953 | nothing |
| **0.105 px** | **0.999** | the old threshold, back-solved |
| 0.5 px | 0.9953 | **a pixel-centre convention bug** |

- [ ] **The two findings the arithmetic produces, and both are reasons not to gate on it.** A fixed `IoU ≥ 0.999` means `δ ≤ 0.105 px` on that triangle and `δ ≤ 0.053 px` on a subject a quarter the area — `P/A` doubles when linear size halves — so **one constant drifts 2× in strictness as the subject changes**, silently. And a **half-pixel convention error scores 0.9953**, which reads as near-perfect to anyone who has not done this arithmetic. A number that hides the defect it was chosen to catch is worse than no number
- [ ] **Boundary displacement p95, in pixels, is the deciding instrument on every geometric rung.** It is shape-independent and it **names** the defect rather than scoring it: 0.05 px is nothing · **0.5 px is a pixel-centre or raster-convention error** · ~3 px is a projection error · a radial trend is focal length · a shear is handedness. IoU is published beside it for continuity with the literature and decides nothing
- [ ] **Three subject classes, three instruments, and the class is a declared property of each rung rather than a global threshold:**

| Subject class | Instrument | Number |
|---|---|---|
| **opaque, all geometry ≥ 1 px** | boundary p95 | **≤ 0.1 px** — 20× the 0.005 px floor: room for float32 and for a tessellation ordering, none at all for a convention error |
| **opaque, sub-pixel geometry present** | boundary p95 | **≤ 0.5 px, reported and not enforced** — a rasteriser drops a triangle no sample centre hits and a 128-spp path tracer finds it. That is a **sampling-policy difference**, not an error, and gating on it would make the ladder punish us for being a rasteriser |

- [ ] **Two classes decide a *parity* rung, and foliage is not a third class — it is a different question with a different ladder** (§ I.26.7). *A foliage row stood here as a weak-instrument exemption, was deleted on a foliage-exclusion rule, and the rule was then overruled. Both versions were wrong in the same way: they treated a forest as a **subject** of the parity comparison, badly measured. It is not. A forest is measured by **frame time and by eye in motion**, and it does not appear in this table because no coverage instrument applies to it at all — not because it is exempt from one*
- [ ] **Which class each rung is, stated in its manifest, because it is the rung's own property**: rungs 1–4, 6–11, 13–15 and 17 are **opaque ≥ 1 px** and carry the 0.1 px number · rung 5 is opaque ≥ 1 px **at sponge level ≤ 2**, where the smallest feature is `729/9² ≈ 9 px`, and **level 3 is deliberately the sub-pixel arm** at `≈ 1 px` · rungs 16 (`Fox`'s limbs and tail at distance), 18 (`SciFiHelmet`'s greebles), 19, 20 and 21 are **sub-pixel present** and report rather than enforce · rung 12 is **format conformance** and carries neither number (§ I.26.5)
- [ ] **Rung 20 is the one rung where a sub-pixel report is the product rather than a concession** — a Julia isosurface at rising subdivision *is* the sub-pixel regime, and the number worth having is where boundary p95 leaves the 0.1 px band as subdivision rises. That curve is a measurement of our rasterisation policy against a path tracer's, on a subject built to force it

**Radiance: the oracle is configured DOWN to the physics the engine claims on that rung.** *Owner's
ruling, 2026-08-12, and it is the clause that stops the ladder growing a permanent red.*

| Oracle configured to | Realistic median relative difference |
|---|---|
| direct, Lambertian, no bounce | **≤ 1 %** — a closed form; both sides should hit it |
| + an analytic sky | **2–3 %** |
| full GI, against a renderer with none | **10 %+ in the open, 100 %+ in shadow** |

- [ ] **`diffuse_bounces = 0` stays until this engine implements bounce, and the last row of that table is not a defect.** A number produced against physics we do not model measures **our ambition, not our correctness**, and it would sit in the tree for months looking like a failing gate — which is how a suite learns to be ignored. When bounce lands, the oracle's bounce count rises with it, in the same commit
- [ ] Rung 4's shadow-and-indirect product keeps its existing shape and this makes it consistent rather than changing it: it is a **bias curve, never a pass/fail**, of the form *our screen-space occlusion removes 0.6× of what one Cycles bounce removes over this geometry* — which is exactly *configured down, difference reported* stated for the one rung where the difference is the subject
- [ ] **Every tolerance above is published with the instrument floor beside it**, and none of them is within an order of magnitude of point-release noise — that is what makes the version relaxation safe rather than convenient
- [ ] **Every tolerance above governs a subject *we* generate. Where the subject is a Khronos asset, the acceptance is Khronos's own and § I.26.12 is where it comes from** — including the clause that decides what happens when we fail one: **a Khronos criterion we do not pass is a defect with a named cause, never an accommodated threshold, never a skip, and never a case left out of the matrix**

**The ten scenes, one new thing per rung.** *Owner's ruling, 2026-08-12: about ten glTF scenes of
ascending complexity, tests under `test/render/`, references rendered in Blender, our pipeline developed
to match. The value of ascending complexity is that a red names its own step, so a rung that adds two
things has thrown that value away. Every `.glb` is emitted by a script in this tree and never authored
(principle 2). The camera is set on the Blender side from the glTF `yfov` by
`sensor_fit='VERTICAL'`, `lens = sensor_height / (2·tan(yfov/2))`, which reproduces `yfov` exactly by
`blender/camera.cpp:674` — never by the importer.*

| # | Scene | The one thing it adds | Judgeable on | First needs |
|---|---|---|---|---|
| 1 | one triangle, ~30 % of frame, declared camera | the reader, the projection, the raster convention | coverage | reader · studio scenario |
| 2 | one quad, rotated off both axes | depth that varies across the frame | coverage · depth | — |
| 3 | the ±1 m cube, 12 triangles, flat normals | indices, winding, back-face culling, a silhouette from six planes | coverage · depth | — |
| 4 | UV sphere, 32×16, normals declared smooth | a curved silhouette — every boundary pixel now has its own sub-pixel edge offset | coverage · depth | — |
| 5 | scene 3 lit: one `SUN` at declared irradiance + factory world | the first radiance number | + direct radiance | **linear tap** · declared studio light |
| 6 | scene 4 lit by the same | the whole `cos θ` sweep from 0° to 90° in one image | + direct radiance | — |
| 7 | three spheres: two `baseColorFactor`s and one `emissiveFactor` | albedo linearity and the emissive channel | + direct radiance | material factors through glTF |
| 8 | scene 3 under Blender's **factory point light**, 1000 W at its factory position | inverse-square falloff — *the literal default lighting* | + direct radiance | **a punctual light in `render/`** (§ II.8) |
| 9 | cube on a ground plane, sun at 30° elevation | a cast shadow, and the first non-zero interreflection | + shadow and indirect, as a bias curve | shadow under a declared studio light |
| 10 | scene 9 plus a generated checker `baseColorTexture` at 1 texel per pixel | texture sampling and the base-colour sRGB decode | coverage · direct radiance | glTF texture path |

- [ ] Scene 1's triangle declared so the mask is not decided by a tie: no edge axis-aligned, no edge through a pixel centre at the declared camera, and vertices exactly representable in float32. A deliberate tie is scene 1's **second** fixture, run as a declared expected-difference rather than as a pass
- [ ] **The ~30 % holds, the roll is what buys it, and the roll's *sign* is the whole of it — `+22.5°`, derived and not set.** *A round measured a unit right-isoceles triangle at **28.1 %** of a 16:9 frame and concluded the 30 % was unreachable; the ceiling it found is real only at **zero** roll, where the legs are axis-aligned, which is the one orientation scene 1's no-tie rule forbids.* The arithmetic, over the usable frame after an 8 px margin (1264 × 704 of 1280 × 720): the unit triangle `{(0,0),(1,0),(0,1)}` rotated by `θ` in a y-up raster has bounding box `w(θ) × h(θ)`, fits at `k = min(1264/w, 704/h)` px per unit, and covers `0.5·k²/921 600`.

| roll | bbox, units | fits at | frame fraction |
|---|---|---|---|
| 0° | 1.000 × 1.000 | 704 px | 26.9 % |
| **−12°** | 0.978 × 1.186 | 593.6 px | **19.1 %** |
| +12° | 1.186 × 0.978 | 719.7 px | 28.1 % |
| **−22.5°** | 0.924 × 1.307 | 538.8 px | **15.8 %** |
| **+22.5°** | 1.307 × 0.924 | 762.0 px | **31.5 %** |
| +45° | 1.414 × 0.707 | 893.8 px | 43.3 % |

- [ ] **Two rolls of equal magnitude and opposite sign are indistinguishable on the no-tie criterion and differ by up to 1.65× in area**, because the frame is 16:9 and the shape is not: the edges lie at `θ`, `45°+θ`, `90°+θ`, so `−θ` and `+θ` put the same three angles the same distance from the raster axes, while only one of them lays the long bounding extent along the long frame axis. **The sign is therefore a derivation with a stated criterion — *the wider bounding extent goes across the wider frame axis* — and never a `[SET]` number.** *A camera that took the other branch lost a third of the subject with nothing to notice it (the bug tasks in `board/`)*
- [ ] **`+22.5°` is where both constraints peak together, so nothing trades.** The minimum edge-to-axis distance is maximised at exactly `θ = 22.5°`, where all three edges sit 22.5° off — against 12°, 33° and 12° at the roll first written down — and that same roll is the smallest one clearing 30 %. Distance follows from the binding axis: half-height `0.46194 m` at `tan(yfov/2) = 0.24` exactly, projected to 352 of 360 px, gives **`d = 1.968493 m`** and **`1.312329e-3 m/px`**, with the half-width then 497.8 px inside the 632 available
- [ ] **The subject's frame fraction is a declared, computed, refused-on-mismatch property of every geometric rung, not a note beside the camera.** It is what the § below's `IoU ≈ 1 − P·δ/A` is applied under and what § I.26's *≈ 30 pixels at risk* is counted over, so a camera that silently frames the subject smaller tightens both without saying so. Right: the manifest declares it `derived` with its derivation, the runner recomputes it from the projected geometry, and a deviation beyond a stated tolerance is a **refusal naming both numbers**. *Written as a requirement because a declared camera got the frame fraction wrong by 1.65× and nothing in the tree could notice (the bug tasks in `board/`)*
- [ ] Scenes 1–4 are judgeable on coverage and depth alone and are **valid geometry tests long before any light model agrees** — that is the whole reason the ladder is ordered this way, and it means the reader, the projection and the raster convention are settled before the first radiance number is asked for
- [ ] Scene 4 is where the boundary-distance distribution earns its place: three straight edges (scene 1) cannot separate a focal-length error from a principal-point error, and a circle can
- [ ] Scene 6 is the rung that **measures our ambient's shape** rather than assuming it: under a full uniform sphere environment every facet of a free-floating sphere should return `ρ·L` regardless of orientation, and `render/stages/SurfaceLight.h:89` weights the sky by `(1 + n·up)/2`, which is right for a dome over dark ground and wrong for a sphere. The residual is expected to be a clean `(1 + n·up)/2` in the elevation direction, and finding exactly that shape is the rung passing, not failing — what it produces is the size of a declared model difference
- [ ] Scene 7's emissive rung is the first consumer of `Material`'s emissive field, which § II.8 records as declared and unreached — *"`Material` has the field and `SurfaceState::Emits()` derives from it; nothing emits"*
- [ ] Scene 8's acceptance is the falloff **exponent** and not only the level: fitting `E(d) ∝ d^-n` over the cube's six faces separates a wrong constant from a wrong law, and a wrong law is the failure a single-distance test cannot see
- [ ] Scene 10 is valid **only at 1 texel per pixel**, declared as a scene constraint and not as a caveat — away from 1:1 Cycles uses its own mip and filter policy and we use ours, and the comparison would measure the choice rather than the implementation
- [ ] Each scene is one directory under `test/render/<feature>/<case>/` whose single tracked file is its `manifest.json` (§ I.26.10) — the scene, the recipe and the acceptance in one declaration, with the Blender side driven from it rather than from a per-scene script, and the requirement identifiers it covers named in `covers` (§ I.20). *Restated 2026-08-12: "its `.glb`, its Blender script and its expected values" described three tracked artefacts per scene, and a per-scene script is a place a scene can differ from every other scene in a way nothing compares*
- [ ] The ladder's own acceptance: **rung `n` is not run until rung `n−1` is green**, which is what makes a red name its own step. A harness that runs all ten and reports eight reds has produced one finding, not eight

- [x] glTF 2.0 reader, **both containers decided by the bytes and never by the file name** — `.glb` with its `BIN` chunk, `.gltf` with its buffer beside it (`src/gltf/Document.cpp`, `src/gltf/Types.cpp`; held by `test/unit/gltf/AGlbCarriesWhatItDeclares` and `test/unit/gltf/TheTriangleProjectsToTheOraclesArea`). A `.glb` named `.gltf` is still a `.glb`, and a reader that trusted the extension would report a JSON parse failure at byte 0 for a file that is fine. An unknown GLB chunk is skipped, which is what the format asks so a future chunk does not make a readable file unreadable
- [x] **All six component types with the format's own signed asymmetry** — `−128` and `−127` both normalise to `−1`, because a signed component divides by its positive maximum and clamps (`src/gltf/Document.cpp` `Normalise`, asserted to 1e-12 in `test/unit/gltf/AGlbCarriesWhatItDeclares`) — and `byteStride` honoured, proven with `0xCDCDCDCD` **live in the padding**: a reader stepping 12 bytes instead of 16 reads the padding as a float, which is a visibly wrong answer instead of a wrong answer that happens to be zero. u8, u16 and u32 indices all decode to the same six values; `min`/`max` cross and are asserted to reach the decoded extremes exactly. *Five of the six component types are asserted by a fixture; `Int16` (5122) is implemented and carried by none, which is the cheapest gap in this section to close*
- [x] **An attribute is a name, not a slot** (`src/gltf/Types.h`, `src/gltf/Types.cpp` `Primitive::Find` and `MissingSemantics`; held by `test/unit/gltf/TheTriangleProjectsToTheOraclesArea`, which reaches its subject's positions through `Find("POSITION")` and gets `NORMAL` back as the name that is missing): the reader answers *what is in the file* and never *what shape does it become*, so `TANGENT`, `TEXCOORD_1`, `COLOR_0` and `JOINTS_0` cross with no line each and the vertex-layout question (§ I.26.6) stays open where it belongs. *This is the shape the four narrowings below were superseded in favour of, and it is the reason the reader is wider than this section first asked for.*
- [x] **All seven primitive modes cross, and refusing them at the reader was the wrong boundary** (`src/gltf/Types.h` `PrimitiveMode`, held by `test/unit/gltf/AGlbCarriesWhatItDeclares`). *Superseded 2026-08-12: this line read `TRIANGLES` only, which is right about the picture and wrong about the layer* — a reader that refuses `mode: 3` cannot report what a file contains, and `MeshPrimitiveModes` becomes unreadable rather than readable-and-declined. **The refusal moves down to the consumer**, one line below, and § I.26.6's `POINTS`/`LINES`/`STRIP`/`FAN` **REFUSED** is unchanged: we still draw none of them
- [ ] **The mesh consumer refuses a non-triangle primitive by name, and nothing else in the engine may read `PrimitiveMode`.** It is the same division as the `POSITION`/`NORMAL` line below: the reader records, the consumer refuses. Nothing enforces it today because nothing consumes the reader at all — which is the next line and not a second defect
- [ ] **Nothing consumes the reader yet, and the bridge is its own feature.** `Document::ReadElements` hands out `Count × components` doubles per accessor and no file in `src/` builds a `core/ChunkVtx.h` run from one, so rung 1 cannot run however green the reader is. *Stale as written and corrected 2026-08-12: a consumer exists — `src/gltf/Subject.cpp` flattens the default scene into world-space **positions only** and refuses a non-triangle mode, an absent attribute, a short run and an out-of-range index, each by name. What is still absent is the bridge to a **shaded** vertex, which is where the rest of this line's list is paid for.* **Every permissiveness the reader earned above is paid for at that bridge, by name**: a non-triangle mode · a primitive with no `NORMAL` · an index ≥ the vertex count · a component count that is not the layout's · a `matrix` that does not invert. *Written as its own line because the reader's own header states the division — "nothing here answers what shape does it become" — and a division stated on only one side is half a design*
- [x] **Sparse accessors, both arms** (`src/gltf/Document.cpp` `ApplySparse`; held by `test/unit/gltf/ASparseAccessorResolves`): over a base `bufferView`, and with **no `bufferView` at all**, where the accessor reads as zeros and the overrides are written into them. The second arm is the one a reader gets wrong, because *no bufferView* looks like *nothing to read* until a morph target comes out empty. *Supersedes this section's original **sparse accessors refused** and § I.26.6's `REFUSED` row alike — the ruling is under that table*
- [x] **Node `matrix` and the TRS triple, and a file that carries both is refused** (`src/gltf/Types.h` `Node`, `src/gltf/Transform.cpp` `FromTrs`/`FromColumnMajor`, `src/gltf/Document.cpp` `WorldTransform`; held by `test/unit/gltf/AMatrixNodeAndItsTrsAgree`). Composition is `T * R * S` in the format's order, the hierarchy is walked to the scene root, a node that reaches itself is refused, and a node that is the child of two nodes is refused because a glTF hierarchy is a forest. **A matrix node and its TRS twin place the same vertex 1.78e-15 m apart**, against an expected world matrix written in whole integers so the test does not check the reader against itself
- [x] **`scenes` and the default scene cross as the file states them** (`src/gltf/Document.cpp`; held by `test/unit/gltf/AGlbCarriesWhatItDeclares`, which asserts the default scene is the one the file names). *Two refusals on this arm are implemented and held by no claim — a `scene` index the file does not carry, and a scene naming a root node it does not carry — so they belong beside `Int16` on the list of what the next fixture should carry. And § I.26.6's **multiple scenes REFUSED** is a statement about what we render, not about what the reader may report — the same division as the primitive modes above.*
- [x] **Every refusal is a sentence that names the file and what was missing, and there is no partial document** (`src/gltf/Document.cpp` `Refuse`, `Document::Error()`; held by `test/unit/gltf/AFileThatCannotMeanAnythingIsRefusedByName`, **13 subjects each asserting the wording of its own refusal**). Asserting only that a file was refused would pass a reader that refused everything for one reason, which is the vacuous shape this repository keeps finding. A file that read nine of its ten accessors would hand its caller a subject that is silently not the declared one, so a refused read leaves `out` **empty** rather than a run of zeros a caller could mistake for a mesh at the origin. *The class-level claim is nevertheless false in one place — see the bug tasks in `board/`, `extensionsRequired`*
- [ ] `POSITION` **and** `NORMAL` required, never derived — our vertex layout has no spelling for a mesh without a normal, and generating one would put our smoothing decision inside a comparison whose subject is somebody else's geometry. An oracle comparison must contain no repair. *Superseded 2026-08-12 by § I.26.12 on the owner's Khronos ruling, and the half that survives is the half about **our** fixtures: the glTF specification says **MUST** calculate flat normals when none are specified (`Specification.adoc:1428`), and flat is not a smoothing decision — so a fetched document without `NORMAL` loads. `POSITION` alone remains required, and every fixture we generate still carries its normal.* *The **mechanism** is built and the **requirement** is not: `Gltf::MissingSemantics(primitive, {"POSITION", "NORMAL"})` (`src/gltf/Types.cpp`) returns the names a primitive lacks and derives nothing, and `test/unit/gltf/TheTriangleProjectsToTheOraclesArea` asks it of Khronos's `Triangle` and gets back exactly `NORMAL`. Nothing calls it outside that test, so a document with no normal is still read happily — the enforcement is the consumer's and the consumer does not exist*
- [ ] **The rule stands unrelaxed for the coverage rung, and the question was asked properly rather than assumed.** *Khronos's `Triangle` carries `POSITION` alone, and the case for relaxing is real: coverage is decided from Cycles' alpha on the oracle side and from `depth ≠ far` on ours, so **the comparison needs no normal at either end**.* It is refused anyway, on the cost of the two ways to accept it: **deriving** one is the repair this line forbids, and it puts a smoothing decision inside the rung whose whole subject is the reader · **admitting** a normal-less vertex means a second vertex layout in the engine, bought for one fetched asset, at the rung whose job is *the reader, the projection, the raster convention* — a shape change to `core/ChunkVtx.h` earned by a test fixture. **The third way costs nothing and is already required**: rung 1's subject is generated here (§ I.26.5 row 1), so it carries a normal because we write one
- [ ] **The synthetic floor's generator emits `POSITION` and `NORMAL` for every fixture it writes**, which makes the reader's requirement and the fixture supply consistent by construction — triangle, quad, cube, sphere, sponge and Julia set all come out readable, and a generated fixture that omitted the normal would be refused by our own reader before any oracle ran
- [ ] **Khronos's `Triangle` is therefore the schema's worked example and never rung 1's subject**, and it is out on two counts at once: no `NORMAL`, and it is a *fetched* file where § I.26.5 row 1 requires a generated one so that a coverage failure at rung 1 has exactly one cause. *Recorded because the manifest under `render/coverage/triangle/` says so in its own notes and a later round would otherwise read the directory name as the ruling*
- [x] **Both glTF cameras, with the two conventions stated once and checked by where a point lands** (`src/gltf/Camera.h`, `src/gltf/Camera.cpp`; held by `test/unit/gltf/ACameraCrossesWithItsConventions`, 24 claims): perspective and orthographic · **`aspectRatio` absent means the viewport supplies it and `zfar` absent means an infinite frustum** — absent rather than substituted, so no large number stands in for infinity and the infinite arm is a different matrix · near at NDC z = −1 and far at +1 · the frustum's top edge at NDC y = +1 and therefore at raster row −0.5, the integer coordinate being the pixel centre · the view transform as the **inverse** of the camera node's world transform, with a point 1 m ahead landing at view z = −1. *Asserting the projection's entries against the algebra that produced them would prove the typing was careful and nothing else; each of these is a sign a reader can get wrong on its own*
- [ ] `cameras.perspective` with `yfov`, `znear`, `aspectRatio` — and what our reversed-Z infinite projection does with `zfar` stated in the same header rather than silently ignored. *Half built 2026-08-12 and the half that is owed is the deciding one. The **file's** side is done and ticked on the line above; the second clause is an **engine-side** statement — `src/gltf/Camera.h` says only that glTF's `[-1, +1]` is not `core/Mat4.h`'s reversed-Z and that converting is the renderer's business, which is the right division and not the answer. Nothing converts one to the other, because nothing consumes the reader*
- [ ] `pbrMetallicRoughness` factors, `emissiveFactor`, `doubleSided`, `alphaMode`; `baseColorTexture` as PNG only at first, because a second image decoder buys no comparison
- [ ] **`KHR_lights_punctual` refused as the light channel** — REFUSED, and this is the load-bearing decision of the section. Blender's glTF importer converts light intensity through a lumens-per-watt factor that is **683** and is under an open, Khronos-PBR-TSG-endorsed proposal to become **177** (glTF-Blender-IO issue #2554, open at the time of writing): a factor of 3.86 sitting inside the oracle's importer, in exactly the quantity rung 3 exists to measure. The light is declared beside the glTF in W/m² and applied by script on both sides, so nothing about it crosses the glTF boundary. **The reason above was read in the source afterwards and is narrower than it was written**: the factor is applied only in mode `SPEC`, which is merely the default — see the `RAW` line above, which is why this refusal is a simplification and not a necessity
- [ ] Skinning, morph targets and `animations` out of scope — we carry our own animation shape already (`clients/Animation.h`, glTF's two-table form with two declared deviations)
- [ ] **Rung 1, coverage**: both sides to a binary mask, compared by IoU **and** by the boundary-distance distribution (for each boundary pixel, distance to the nearest boundary pixel of the other mask; p50/p95/p99 in pixels). IoU alone cannot see a half-pixel camera offset on a large subject; the distribution can, and it localises it
- [ ] Rung 1's failure signatures declared with the metric, which is what makes a difference **attributable**: a constant offset is the camera origin or principal point · a radial trend is the focal length or a projection convention · a uniform scale is the `yfov`/aspect interpretation · a shear is a row/column order or a handedness. Four distinguishable shapes, one metric
- [ ] Rung 1 acceptance: **boundary p95 ≤ 0.1 px** at 1280×720, both sides at one sample per pixel with the narrowest reconstruction filter. *Tightened 2026-08-12 from the 0.5 px this line first carried — right instrument, 5× too loose for a subject with no sub-pixel geometry, where 0.5 px is the size of the convention error the rung exists to catch. IoU is reported beside it and enforces nothing (§ below).*
- [ ] **Rung 1 becomes the acceptance criterion for the SDL_GPU port** (§ I.19), which today has none: the same glTF through the old backend and the new one must give the same mask. It is the cheapest port gate that exists and it is a picture claim rather than a counter claim
- [ ] **Rung 2, depth**: linear view-space range both sides, compared inside the intersection mask, p99 ≤ 1e-4 relative. A bias attributes to the near plane or the reversed-Z convention; growth with distance is the float32 floor and is published as the instrument's own floor beside the result
- [ ] **A scene-referred linear float readback ahead of `ExposureStage`** — the one thing rung 3 needs that does not exist. `render/Renderer.h:59` `ReadPixels` is *"already sRGB-encoded"* and there is no other colour tap, so the physics of this renderer is currently unreadable by anything, including us (the bug tasks in `board/`)
- [ ] **The linear tap priced, because it is the whole blocker between rung 2 and rung 3 and it is small.** It is a *second reader of a texture that already exists* — the offscreen HDR scene target (`render/Gpu.h:13 HdrFormat`, RGBA16F) — not a new pass, not a new format and not a new pipeline state. Cost: one copy-to-buffer of **W·H·8 B = 7.37 MB at 1280×720** on the frames a test asks for, one staging buffer, and one method shaped exactly like `ReadDepth` — poll, `ReadState`, no waiting variant. **Zero cost on a frame nobody asks**, so it does not touch the 720p60 floor and needs no quality tier. It lands as its own step **between § I.25's studio scenario and the glTF work**, and it is the cheapest of rung 3's three prerequisites
- [ ] What the tap makes decidable the day it exists, before any Blender comparison runs: whether `render/stages/SceneScale.h:17 kSceneExposure = 11.0` is an exposure or a physics scale. Its own stated derivation terminates in *"the value whose sRGB output is 0.70"* — a display code on the path of every physical quantity (the bug tasks in `board/`) — and no reader in this tree can currently see the quantity it scales
- [ ] The tap is `device`-tier and native, so it costs no browser gate and no second toolchain (§ I.20)
- [ ] **Rung 3, direct diffuse**: one directional light at declared irradiance in W/m² perpendicular to the beam · Blender's `Diffuse BSDF` at roughness 0, which is **exactly Lambertian** by the Blender manual, and explicitly **not** the Principled BSDF, which at metallic 0 still carries a specular lobe at IOR 1.5 (F0 = 0.04) and whose diffuse lobe becomes energy-preserving Oren-Nayar above diffuse-roughness 0 · constant albedo, no texture, no sky, no indirect · both sides written as linear OpenEXR
- [ ] Rung 3 acceptance: median relative difference ≤ 1 % against the closed form `ρ·E·cos θ / π` on unshadowed facets, with the **sign** of the residual reported, and Blender's own residual against the same closed form published beside ours — the oracle states its error before it judges ours
- [ ] Rung 3's residual shape read as an attribution: a constant factor says the scene scale is doing physics' work · a `cos θ` or `sin(elevation)` dependence says the irradiance convention — perpendicular-to-beam against on-the-horizontal, the commonest single error of its class, and `IrradianceStage` already names both (`sunDirectNormalY`, `skyDiffuseHorizY`) · a per-channel difference says the three channels are scaled apart somewhere
- [ ] What rung 3 settles, stated before it is run: whether `render/stages/SceneScale.h:17 kSceneExposure = 11.0` is an **exposure** — legitimate, and belonging in the exposure stage — or is doing physics' work, which nothing in this tree can currently decide because its own derivation anchors it on *"the value whose sRGB output is 0.70"*
- [ ] **Rung 4, shadow and indirect**: reported as a **bias curve and never a pass/fail**, because a raster engine and a path tracer disagree there by construction. The product is a number of the form *our screen-space occlusion removes 0.6× of what one Cycles bounce removes over this geometry* — actionable, where a red is not
- [ ] The pinned set, both sides, published with every comparison: linear Rec.709/sRGB primaries · OpenEXR float32 output, which **ignores the view transform** by Blender's own colour-management rule and thereby deletes AgX, Filmic and our ACES fit in one move · Blender `Standard` view transform for any PNG a human looks at, since AgX is the 4.0+ default and is a heavy S-curve · `film.exposure = 1.0` · 1280×720 at 1 spp with the narrowest filter · fixed Cycles seed, declared sample count, **denoising off** · declared bounce count per rung · camera set by script from the glTF `yfov`, never by the importer · `use_auto_smooth` off so Blender does not re-derive a second geometry · `scale_length = 1.0` · **the Blender version recorded on the row**, because the oracle's version is part of the measurement
- [ ] Our own sky exported as an equirectangular EXR and set as Blender's world, which makes the environment identical by construction and isolates the surface response — the only way an atmosphere comparison measures an implementation rather than which model each side chose (Blender's Nishita sky is a different model with different aerosol parameters)
- [ ] What a Blender comparison **cannot** judge, declared in the same header so no round reports an unactionable red: anti-aliasing and reconstruction (Cycles integrates the pixel footprint, we resolve jittered samples — different everywhere at a silhouette) · texture filtering away from 1 texel per pixel · indirect light, which we do not have by design and where the comparison measures the size of a known absence · our TAA, impostors, LOD selection and grass field, which have no counterpart · anything about display beyond the working space
- [ ] **That list re-decided against the source, because three of its seven entries dissolve and the rest get sharper.** *Anti-aliasing* — **dissolved for coverage**, `BOX` at 0.01 px makes it the same predicate, floor 0.005 px; **survives for the shaded image**, where our TAA resolves eight jittered frames and the rung is therefore run with TAA off. *Indirect* — **dissolved as a confound**, `diffuse_bounces = 0`; **retained as the subject** at rung 9. *Display* — **dissolved**, EXR float32 ignores the view transform and our side reads the linear tap, so AgX never enters. *Texture filtering* — **survives, narrowed to a scene constraint**: valid at 1 texel per pixel, undefined away from it. *Our atmosphere* — **survives**, and gains an exact substitute: Blender's factory world is a *uniform* environment of a known radiance, which is closed-form on both sides, so an ambient comparison is possible even though a sky-model comparison is not. *TAA, impostors, LOD, grass* — **survive**, no counterpart, off for every rung. The one entry the list was missing is ours and not Blender's: **our ambient is a hemisphere over geodetic up with two bounce constants**, so it cannot match a full-sphere environment except where `n·up = 1` — measured at rung 6 rather than assumed
- [ ] The comparison is a **test in the suite** (§ I.20), tiered `device`, with the Blender binary taken from the environment and **refused if absent** rather than skipped — a silent skip is the defect class this repository keeps finding
- [ ] The reference `.glb` files are generated by a script in this tree, never authored — principle 2 applies to a test fixture exactly as it applies to a texture, and a hand-modelled cube is a file nothing can recompute
- [ ] The first comparison to run is **rung 1**, because it is *decidable* in this file's own sense — no light model has to agree for it to mean something — and because every later rung is confounded by its failure. It costs the reader and one mask difference
- [ ] The first comparison that **settles** something is rung 3, and what it settles is whether this engine's light transport is right at all: the first correctness-class number in the archive, against 100 % consistency-class ones today

**The ten scenes above are ten fixtures, not the ladder.** *Superseded 2026-08-12 by the twenty-one-rung
ladder below, on the owner's ruling that the ladder is built from concrete assets and ordered easy to
hard. Nothing above is withdrawn: every one of the ten is still a fixture we generate, and every bullet
above that says "scene N" means **fixture N**, which the ladder's own table maps to its rung. The four
rungs of the comparison — coverage, depth, direct radiance, shadow and indirect — are unchanged and are
what a rung is judged **on**; the ladder is what is judged, in order.*

## The frame fraction, measured — and the rule that sets it is honest about a sphere, not a subject

*The owner asked for objects scaled to the render target where scale does not affect the test. The request
already lives here — the frame-fraction line above — and it is grown rather than filed beside.*

**THE FRAMING RULE ALREADY FITS; IT FITS THE WRONG THING.** `src/gltf/Framing.h` carries
`kFramingFill = 0.6 [SET]`, documented as *the subject's bounding sphere spans 60 % of the frame's
vertical extent*. **That is a statement about a sphere's height, and the quantity this line requires is
the subject's own coverage.** A real subject fills a fraction of its bounding sphere, the frame is 16:9
while the fill is vertical, and the two together turn 0.6 into single digits.

**[MEASURED] over every case carrying an oracle — the covered fraction from the oracle's own alpha
channel, which is exact coverage and touched no lighting:**

| | |
|---|---|
| cases measured | **19** |
| **median coverage** | **5.23 %** |
| under 10 % | **13 of 19** |
| over 50 % | **1** (`materials/emissive-strength`, 68.56 %) |
| the floor | `texture/four-texels-per-pixel` **0.44 %**, which is 4096 of 921 600 **by derivation** |

**Every one of these declares `camera.source: manifest`**, and their own derivation prose cites this rule —
`a-beautiful-game`'s says *the framing rule of `board:0083` applied to this subject's own bounds*, and it
covers **3.86 %**. **So the rule is being applied and the parameter it names is not the quantity anyone
wanted.**

## Why this is an instrument and not tidiness, in one number

**The picture bound is a max over channels, and a background pixel agrees trivially.** At a median 5.23 %
coverage, **roughly 95 % of every render decides nothing** — the tail is chosen from the twentieth of the
frame that carries the subject. **Framing the subject to the fill the rule already names would multiply
the deciding population by about eleven at the same target size.**

**And it is the same lever as `board:1181`.** Eleven times the deciding pixels at one target, or the same
signal at **√11 ≈ 3.3× smaller linear target** — 1280×720 → about 384×216, which is **eleven times fewer
bytes per raw dump.** `board:1169` already made that trade by hand, declaring **320×180** for a 31-frame
grid because 1280×720 would have been 2.29 GB for one case. **A framing rule that fitted the subject would
make that a derivation instead of a judgement call**, and the two arguments — more signal, less corpus —
are one change.

## The exemption is REQUIRED, never a default with an opt-out

**Some cases *are* scale**, and for them auto-framing destroys the subject:

- **`texture/four-texels-per-pixel`** exists to put **exactly 4 texels per pixel by derivation**; reframing
  it deletes the case.
- **`foliage/beech`** declares `pixelHeightFrac` as a generator parameter — the subject's projected size is
  a **declared input** to what is grown.
- Anything whose dependent variable is texels-per-pixel: `board:1130`'s whole thread, and `board:0110`'s
  LOD subset case when it exists.

**[MEASURED] those are the only two manifests in the tree today that name a scale-dependent quantity**
(`texels per pixel` · `pixelHeightFrac`), so the exemption list is short and checkable rather than
guessed.

- [ ] **Every case declares which it is, and the declaration is required rather than defaulted.** A
  default with an opt-out puts the burden on the author of the exceptional case, who is the one least
  likely to be thinking about it; **a required declaration puts it on every author once**, which is where
  this tree puts every other such burden — the manifest is *a delta over declared defaults* precisely so
  that a decision nobody made cannot pass
- [ ] **The two arms are named quantities, not a boolean** (`Enum.2`): *framing is derived to a declared
  fill* · *framing is declared because the subject's projected scale is the measurement*. **The second arm
  carries its reason in the manifest**, so a case that exempts itself says what it is measuring
- [ ] **A case in the second arm still publishes its frame fraction**, computed by the runner as this
  line already requires. **Exempt from being reframed is not exempt from being measured** — `board:1135`'s
  0.44 % is a load-bearing number for that case and would be a silent one under a blanket exemption
- [ ] **`kFramingFill` gains a derivation or keeps `[SET]` with a reason that names the subject**, not the
  sphere. A parameter whose documented meaning and measured effect differ by an order of magnitude is a
  magic number wearing a comment

**`board:0034` is the other half of the same rule and stays separate**: it is *five camera manifests aim
at a point their own stated derivation does not produce* — an **aim** defect, where this is a **fit**
defect. Same rule, two failures, and conflating them would put two causes behind one number.
