# Visual target — the bar, the budget, and what they force

> Owner, 2026-08-05, in conversation: *„Witcher 3, Fallout 4."* · *„ich würde sogar auf 720p gehen und
> für einen cinematischen look sorgen."* · *„eher in anti aliasing investieren. kanten sind der optik
> killer bei niedrigen auflösungen."* · *„schön anzusehen ist auch wichtiger als korrekt."*

Spec-first. `## State` is what exists; almost nothing does.

## Spec

### 0. What is being built, and what it is judged on

Not a game — **something worth watching.** The 2026 deliverable is the AI playing while a person watches,
closer to a stream than to a screensaver. Nobody aims, nobody reads the far distance, nobody needs a
crisp HUD at 40 metres. **That single fact pays for most of the decisions below.**

The player comes later, at AAA. Until then every visual trade goes to *looking good in motion*.

### 1. The budget, measured rather than assumed

| Platform | Role | Measured |
|---|---|---|
| **A18 Pro** | development | the working target |
| **Xbox Series S/X, Edge, WebGPU** | delivery | WebGPU present; **„etwas mehr power als der A18 Pro"** |

Edge on Xbox runs in app mode, not game mode — it sees a small slice of a console that is five times an
A18 Pro raw. **So the phone is the budget, and there is no pleasant surprise waiting at the end.** Plan
mobile-class throughout: bandwidth-poor, ALU-rich, thermally throttled within minutes. Twitch means long
sessions, so the sustained clock is the real one, never the peak.

**The numbers, measured on the target rather than estimated:**

| | Value | Compared to |
|---|---|---|
| memory budget | **4 GB** — Edge's app allowance on Xbox; a MacBook Neo's 8 GB shared leaves about the same after OS and browser | PS4 gave games ~5 GB of 8. Same order, at a quarter of the pixels |
| **memory bandwidth** | **60 GB/s** (LPDDR5X-7500, 64-bit bus, unified) | **PS4 had 176 GB/s — roughly 3×.** This, not compute, is the weak axis |
| compute | ~2.5–4 TFLOPS | *above* PS4's 1.84. The phone out-computes the console it is being compared to |
| CPU | 6 cores, fast | far above PS4's 8 Jaguars at 1.6 GHz |

**Read that table before optimising anything.** The instinct is to save ALU; the measurement says ALU is
the surplus and bandwidth is the shortage. Every trade goes the same direction: *spend arithmetic to
avoid traffic.* That is what makes „alles im Shader, minimal Texturen" the correct engineering call and
not merely a frugal one — and it is why geometry must be **generated on the GPU rather than uploaded to
it**, because upload traffic is traffic too.

Reference points: PS4 shipped Witcher 3 and Fallout 4 at **1080p30**, not 720p (Xbox One ran Witcher 3 at
900p). The 720p30 target is therefore *less* than what that hardware delivered — with more compute and a
third of the bandwidth.

**WebGPU limits on Xbox Edge, all above the specification's defaults:**

| Limit | WebGPU default | Measured |
|---|---|---|
| `maxStorageBufferBindingSize` | 128 MB | **2 GB** |
| `maxComputeWorkgroupStorageSize` | 16 KB | **32 KB** |
| `maxComputeInvocationsPerWorkgroup` | 256 | **1024** |

So the GPU-driven pipeline needs no structural compromise: large buffers bind whole, workgroups are big
enough for sensible culling tiles, and 32 KB of workgroup storage holds a terrain tile with its
neighbourhood in fast memory instead of re-reading it from global — which saves exactly the bandwidth
that is short. **We are throughput-bound, not structure-bound**, which is the better of the two problems:
the architecture does not change, only the budgeting.

And the browser is not the handicap it would be under WebGL. WebGPU is built around command buffers and
bind groups, close to Metal in shape; with a single pass and instancing the per-call cost amortises, and
compute shaders remove the CPU from the geometry path entirely. WASM SIMD is available but is not
expected to matter — the CPU is the surplus here too.

### 1.1 The mod asks; the engine decides

> Owner, 2026-08-05: *„das Mod gibt vor, was es gerne zeichnen möchte, die Engine muss entscheiden, wie
> sie das Maximum bei 720p30 herausholen kann. Wenn das Mod 3 Millionen Dreiecke darstellen will, macht
> die Engine vielleicht nur 100 000 mit annähernd gleicher Qualität."*

**This is a boundary, not an optimisation**, and it is the same boundary as everywhere else in this tree:
a mod declares *intent*, never *mechanism*. It says what should be visible and how much it matters. It
does not say how many triangles to submit, which LOD to pick, or how far the impostors start.

| The mod declares | The engine decides |
|---|---|
| what exists, where, and its importance | LOD bias, instance density, impostor distance |
| that this forest is dense | how many of its trees are geometry this frame |
| that this target must stay readable | shadow resolution, TAA samples, cloud march steps |

**The engine's contract is fixed and the quality is what floats:** hold 720p30, then spend everything
left on looking as good as possible. Not the other way round. A frame that misses 30 is a defect even if
it is beautiful — the 2026 product is something watched continuously, and a hitch is more visible than a
missing tree.

The 3 000 000 → 100 000 example is not a compromise, it is the whole craft. A forest at two kilometres
resolves to impostors and aggregates that nobody can distinguish from the geometry, and the ratio *is*
the engineering. **Triangles are not the currency — perceived difference is.**

### 1.2 `mods/bench/` — the load test as a mod

> Owner: *„ich würde ein `mods/bench/` bauen mit einer Referenzszene, die immer anspruchsvoller wird als
> Belastungstest."*

A reference scene that escalates in steps until the governor can no longer hold 30. **The number that
comes out is the step where it broke**, and that number is comparable across commits, machines and
rounds — which no frame-rate anecdote ever is.

**Forest is the benchmark because every constraint peaks there**: most instances → most bandwidth ·
alpha cutout → worst aliasing · wind animation → hardest motion vectors for TAA · most LOD levels → most
visible transitions · self-shadowing → expensive and easily wrong. What holds a forest holds everything
else. L-system trees give this hardest case an exact reference (§1.3).

It belongs in `mods/` and not in `sim/test/` for a reason that is not filing: it must go through the
**same** path a real scenario goes through. A bench that reaches past the mod boundary would measure a
renderer nobody ships.

| | |
|---|---|
| shape | **forest-centred**, N escalating steps: instance count, overdraw, shadow casters, view distance |
| verdict | the highest step held at 720p30 **after thermal soak**, plus the quality settings the governor chose to hold it |
| why both numbers | a step held by dropping every knob to minimum is not the same result as one held at full quality, and a single number would hide the difference |

### 1.3 Ground truth: how „near-equal quality" becomes a number

> Owner, 2026-08-05: *„du kannst einen Referenzrenderer implementieren oder Blender verwenden."*

This is the same move the whole tree runs on — **an expectation is a datum, not a matter of taste** — and
it converts §1.1's perceptual trade from an opinion into a measurement.

**Synthetic first.** A fractal has a closed form, so it has exactly one right answer — any deviation is
our error, with no interpretation gap. Blender's floor is contaminated by legitimate differences (shading
model, tone curve, sampling); a Mandelbrot set's is not.

| Probe | Tests |
|---|---|
| 3D turbulence vs. its exact 2D reference | sampling, filtering, precision at range |
| band-limited noise vs. its analytic band limit | mip and LOD selection, aliasing |
| static fractal, TAA on | convergence, ghosting |
| same fractal at governor quality vs. full | §1.1's trade, against an exact reference at both ends |

Cheap: no assets, no scene, no lighting. And it makes `mods/bench/` synthetic too — arbitrarily
escalating difficulty **with an exact reference at every step**. One artefact, load test and correctness
probe at once.

Blender comes after, for what a fractal cannot reach: real materials, real light transport.

Render the same scene twice: once **offline with no budget at all** (Blender Cycles, path traced, minutes
per frame), once through the engine at whatever the governor chose. Compare the images. **The distance is
the quality loss, and the governor's job is to minimise it under the 30 fps constraint.**

Two numbers come out, and both are needed:

| | What it measures | Why it matters |
|---|---|---|
| **the floor** | engine at *maximum* quality vs. ground truth | how far our shading model is from reality at all — a property of the renderer, not of the governor |
| **the governor's cost** | engine at chosen quality vs. engine at maximum | what the frame-rate contract actually cost this frame |

Measuring only the total would confuse the two: a large gap might be a bad LOD decision or might be a
shading model that was never close. Separating them says which.

The pieces already exist and were built for other reasons: **headless Blender** runs the asset pipeline,
and **`gpu_native` is already the tree's frame oracle**. What is missing is the matched-pair harness and
the distance metric.

### 1.4 Style: pairwise choice, not taste

> Owner: *„wenn du meine präferenzen brauchst mach eine webseite wo ich immer zwischen zwei versionen
> wählen kann."*

Fidelity gets ground truth (§1.3). **Style gets the owner.** Two variants, one choice, accumulated —
grade, grain, depth of field, fog. An anchor, not an opinion.

Built when there are frames to compare, not before.

### 1.5 Greenfield

No legacy. Concretely: WebGPU only, no WebGL fallback · compute-first, GPU-driven from the start, no CPU
path to strand · glTF as the only asset format · reversed-Z, linear working space, ACES/AgX at the end —
designed in, not retrofitted.

And experiments are cheap here in a way they never were for a human team: build three, measure, keep one.
`mods/bench/` is where that happens.

**One honest limit.** This measures *fidelity loss*, not *beauty*. It cannot judge the cinematic look of
§2 — grain, grade and depth of field are deliberate departures from ground truth, and a metric that
punished them would be measuring the wrong thing. Those stay with the critic pair against a pinned
reference frame. **Fidelity gets a number; style does not, and pretending otherwise would be the kind of
false gate this tree spent today removing.**

## Spec (continued)

### 2. The frame

| | Decision | Why |
|---|---|---|
| resolution | **720p** | a quarter of 1440p's pixels — 4× the budget for sky and vegetation |
| rate | **30** | cinematic, and it doubles the per-frame budget again |
| look | **cinematic** — grain, gentle depth of field, colour grading, soft upscale | a film look **hides** low resolution instead of fighting it; it reads as intent, not as a defect. Depth of field is aesthetically free here because nobody must read the distance |
| motion blur | **subtle** — short shutter, a hint | owner: it must not smear |
| **anti-aliasing** | **the priority investment** | at 720p, edges are the dominant defect. Alpha-cutout foliage in motion is the worst case in the whole scene |

**TAA is the keystone and it earns its cost twice.** It resolves foliage aliasing, and it makes *„keine
Geometrie darf poppen"* nearly free: LOD transitions rendered as stochastic dither resolve temporally
into a smooth blend. One technique, two requirements. The cost is that 30 fps leaves longer between
samples, so motion vectors must be correct everywhere — including on instanced, wind-animated foliage,
where they are easy to get wrong.

### 3. Altitude splits the problem

> Owner: *„aus 10.000m brauche ich keine bäume sehen aber geometrie darf nicht poppen"* · *„in der höhe
> überwiegen schöne wolken, nebel, himmel"*

| Regime | What carries the image |
|---|---|
| high | sky, clouds, haze, horizon, atmospheric scattering |
| transition | terrain silhouette, large landcover masses, weather |
| low | vegetation, buildings, materials, shadows, ground detail |

These are different subsystems, not one system at different detail. Trees at 10 000 m are not required —
**geometry that pops is**, at every altitude. Continuous LOD is therefore a hard rule: geomorphing on
terrain, dithered temporal transitions on instances, crossfaded impostors. No mesh swap is ever visible.

### 4. The world is procedural; OSM is an overlay

> Owner: *„Wir müssen überall erfinden. OSM daten haben nur vorrang. Ohne OSM haben wir eine Welt in der
> Steinzeit mit dichter Vegetation. Mit OSM haben wir gebäude, strassen, felder."*

The default state of the world is **nature**, and OSM *overlays* civilisation where it is mapped. The
fallback for missing data is not emptiness and not a white tile — it is wilderness, which is also the
correct answer for most genuinely unmapped land.

**Aerial imagery is not a visual source.** A photograph carries baked lighting, season, shadows and
parked cars; it cannot be relit, and time of day and weather are declared per mission. Imagery is used
**only as a coarse albedo hint at DEM resolution**. Everything visible is generated.

Two consequences worth stating: night vision and the radar map need *geometry*, which a photograph cannot
supply — and procedural detail is resolution-independent, where imagery runs out.

### 5. Vegetation: 256 templates from albedo

> Owner: *„wenn dir der grobe albedo reicht kannst du den auf 8 bit komprimieren und bekommst 256
> vegetationstemplates … müssen dann natürlich fliessend übergehen."*

Quantise the coarse albedo triple to an 8-bit index into 256 vegetation templates. A template declares a
species mix, densities, ground cover and clutter — not a texture.

- **Blend the distributions, not the images.** Interpolating species mix and density across a boundary
  leaves no seam; interpolating textures leaves a visible one.
- **Latitude and elevation are a plausibility filter** on the index. Albedo alone is seasonal — a winter
  photograph would otherwise make a deciduous forest permanently snowbound.
- **Growth, not assets.** `~/Git/wasm-tree` is the *idea*: trees grown from JSON parameters as watertight
  meshes with instanced leaf cards, venation normal maps, hierarchical wind and an octahedral-impostor
  LOD ladder. Its sixteen Central European species do not transfer; the growth method does.

### 6. Acceptance

| Contract | Anchor |
|---|---|
| It holds the budget | 720p30 sustained on A18 Pro **after thermal throttling**, not at peak |
| Nothing pops | a scripted descent from 12 000 m to the deck, captured; no visible mesh swap in any frame pair |
| Edges are quiet | the same descent over dense foliage; no crawling on alpha-cutout leaves |
| The look is the bar | side-by-side against a Witcher 3 / Fallout 4 reference frame, judged by the critic pair, at the same altitude regime |
| Pretty beats correct | where the two conflict, the record says which was chosen and why — a decision, never a drift |

## State

Nothing here is built. Clouds and atmosphere landed recently and are the furthest advanced part; they are
also the part §3 says carries the high-altitude image, so that is fortunate rather than planned.

## Gaps

- **„Cinematic" has no anchor.** §2 states it in words. Which grade, which grain, how much depth of field
  — undecided, and it is the kind of thing that drifts into taste unless a reference frame is pinned.
- **Motion vectors for wind-animated instanced foliage** are the known-hard part of TAA and nothing
  produces them today.
- **The fixed-function raster path is *less* specified than the shaders**, and it lands exactly on §2's
  priority investment: edge coverage „not defined", the **multisample resolve algorithm is not specified
  at all**, and alpha-to-coverage is „platform-dependent and can vary for different pixels" and not
  monotonic in alpha. Alpha-cutout foliage is the worst case in the scene and three of those four
  sentences aim at it. See [`gpu-determinism.md`](gpu-determinism.md).
- **The 256 templates do not exist**, nor does the quantisation, nor the latitude/elevation filter.
- **The ground-truth differential (§1.3) does not exist.** The pieces do — headless Blender runs for
  assets, `gpu_native` is already named the frame oracle — but nothing renders a matched pair or measures
  the distance between them.
- **The floor is unknown.** §1.3 rests on measuring the engine-vs-Blender gap at *maximum* quality first.
  That number has never been taken, and if it is large, the governor's contribution disappears inside it.
- **The governor does not exist**, nor do the knobs it would turn. Today quality is fixed and the frame
  rate floats, which is exactly backwards.
- **No thermal measurement.** §1's numbers are all peak or nominal. What the sustained clock is after ten
  minutes of continuous rendering is unmeasured, and Twitch means continuous by definition. Every frame
  budget here rests on it.
- **Building extrusion and roofs are blocked by the tile server's zoom, not by missing data** — measured
  2026-08-06 against a running `fb-tiles`:

  | | `/t/vector/z/x/y` |
  |---|---|
  | z13 Payerne (control) | 14 314 B — `streets` `land` `water` |
  | z13 Sindh (Armored Fist) | **37 172 B**, *more* than the control |
  | **z15, z16, anywhere** | **14 B = `no such route`** |

  So OSM data is present for the foreign theatres; the endpoint serves **z13 only**, and at z13 the
  upstream source carries no `buildings` layer. `tiles/src/raster.c` and `lights.c` both read a
  `buildings` layer, so the pipeline can carry it — it never arrives. **Extrusion needs a zoom the server
  does not serve**, and that is one route, not a data acquisition.

  Two cold-cache traps corrected while measuring: a first request returns 9 B and the second the real
  tile (same behaviour the DEM has), and an earlier note in this tree read „northern Thailand has no OSM
  objects" off exactly that artefact.
- **Foliage** is named in the goal and absent from the tree.
