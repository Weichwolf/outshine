# Visual target — the bar, the budget, and what they force

> Owner, 2026-08-05, in conversation: *„Witcher 3, Fallout 4."* · *„ich würde sogar auf 720p gehen und
> für einen cinematischen look sorgen."* · *„eher in anti aliasing investieren. kanten sind der optik
> killer bei niedrigen auflösungen."* · *„schön anzusehen ist auch wichtiger als korrekt."*

**This file is the overarching goal, not a pass description.** What the image must look like, what it may
cost, and how that is judged. Every render pass has a file of its own under [`stages/`](stages/); the
three cross-cutting subjects that are not passes are [`classification.md`](classification.md),
[`vegetation.md`](vegetation.md) and [`lod.md`](lod.md).

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
and **`gpu_walk` is already the tree's frame oracle**. What is missing is the matched-pair harness and
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

**TAA is the keystone and it earns its cost twice** — but only under conditions the literature states
explicitly, and they are not free. It resolves foliage aliasing, and it makes *„keine Geometrie darf
poppen"* cheap. The published form of the argument is Karis's, for Nanite: *„if we only draw clusters
that are less than 1 pixel of error they are imperceptibly different and temporal antialiasing smoothes
out any change. TAA is built to blend subpixel differences over time. It does our work for us **so long
as the error is subpixel**."* (Karis, Stubbe, Wihlidal, *A Deep Dive into Nanite Virtualized Geometry*,
SIGGRAPH 2021 Advances in Real-Time Rendering,
[PDF](https://advances.realtimerendering.com/s2021/Karis_Nanite_SIGGRAPH_Advances_2021_final.pdf).)

**Read the conditional.** TAA does not dissolve an arbitrary LOD transition; it dissolves a *subpixel*
one. Nanite therefore needs neither geomorphing nor cross-fading — it keeps the error under τ instead.
Below τ, dithering has nothing left to hide. The three conditions the LOD ladder must respect are in
[`lod.md`](lod.md) `## Knowledge`.

The cost is that 30 fps leaves longer between samples, so motion vectors must be correct everywhere —
including on instanced, wind-animated foliage, where they are easy to get wrong, and where the primary
sources are silent ([`lod.md`](lod.md) `## Gaps`).

### 2.1 2015 is the QUALITY bar; WebGPU is the means, and it is to be used fully

> Owner, 2026-08-06: *„du sollst keine 2015er Technik verwenden sondern WebGPU voll ausschöpfen. Die
> Annahme ist, dass du mit modernen Mitteln die Qualität 2015er Titel erreichen solltest."*

Two separate statements, and conflating them was the earlier error in this file:

| | |
|---|---|
| **Witcher 3 · GTA 5 · Fallout 4** | the **quality** bar — what the image must look like |
| **WebGPU** | the **means**, to be exhausted — not the methods of 2015 |

**The only real exclusion list is what WebGPU provably cannot do**, not what a 2015 title did not do.

#### What the quality bar actually cost in 2015

Only GTA 5 has a public frame-level teardown. What is listed here is read off it; the other two titles
are **unsourced in this file** and are named as such (`## Gaps`).

**GTA 5 (PC, April 2015)** — Adrian Courrèges, *GTA V — Graphics Study*, 2015,
[adriancourreges.com](https://www.adriancourreges.com/blog/2015/11/02/gta-v-graphics-study/):

| Stage | What the study shows |
|---|---|
| shading | deferred, MRT G-buffer, HDR throughout |
| shadows | CSM, **4 cascades in one 1024×4096 texture**, dither-sampled then depth-aware blurred; an ⅛-scale „early out" texture skips the blur where it cannot matter |
| ambient occlusion | SSAO at **half resolution**, noisy then blurred |
| volumetrics | light-shaft map at **half resolution**, ray-marched against the sun shadow map |
| reflections | realtime environment cubemap at **128×128 per face**, converted to a dual-paraboloid map |
| water | scene re-rendered upside-down into a **240×120** texture, 650 draw calls |
| tonemap | Hable/Uncharted-2 filmic operator (Duiker's 2006 curve) |
| **anti-aliasing** | **FXAA** — a post filter. **Not TAA** |
| **LOD transition** | **alpha stippling**: a dither pattern makes an opaque mesh look part-transparent while it crosses between LODs. Courrèges reports it as *visible* — „Do you notice some pixels missing? It's especially visible for the trees" |
| vegetation LOD | **distance-declared**: „beyond a certain distance the grass or the flowers are never rendered" |

**Three corrections to what this file previously asserted**, all from that one source:

1. **GTA 5 had no TAA.** It had FXAA. The 2015 bar was reached *without* the technique §2 calls the
   keystone — so TAA is our lever, not theirs, and it is a genuine advantage rather than parity.
2. **Dithered LOD transitions predate TAA and were visible without it.** GTA 5 shipped exactly the
   mechanism [`lod.md`](lod.md) names, and the public teardown calls out the stipple. Dither is not what makes a
   transition invisible; keeping the error subpixel is (§2, Karis 2021).
3. **GTA 5's vegetation LOD is distance-declared**, which is the formulation [`lod.md`](lod.md) rules out. The bar
   title does the thing this spec forbids — so matching its *look* does not mean copying its *rule*.

#### What WebGPU gives, measured and specified

Two sources, and they are **different devices** — neither is the A18 Pro that §1 names the budget:

* the W3C specification's *default* (guaranteed-everywhere) limits — [W3C WebGPU](https://www.w3.org/TR/webgpu/), §Limits
* [`doc/webgl-webgpu-report.txt`](../webgl-webgpu-report.txt), generated 2026-07-22 on an **Intel HD Graphics via ANGLE/D3D11**, 2560×1440, 4 cores

| Limit | Spec default | `webgl-webgpu-report.txt` | §1 claims for Xbox Edge |
|---|---|---|---|
| `maxStorageBufferBindingSize` | 128 MiB | 1 GB | 2 GB |
| `maxBufferSize` | 256 MiB | 1 GB | — |
| `maxComputeWorkgroupStorageSize` | 16 KiB | 32 KiB | 32 KB |
| `maxComputeInvocationsPerWorkgroup` | 256 | 1024 | 1024 |
| `maxComputeWorkgroupsPerDimension` | 65535 | 65535 | — |
| `maxColorAttachments` | 8 | 8 | — |
| **`maxColorAttachmentBytesPerSample`** | **32 B** | 128 B | — |
| `maxStorageBuffersPerShaderStage` | 8 | 64 | — |
| `maxStorageTexturesPerShaderStage` | 4 | 64 | — |
| `maxBindGroups` | 4 | 8 | — |
| `maxTextureDimension2D` | 8192 | 16384 | — |

**`maxColorAttachmentBytesPerSample` = 32 B by default is the one that bites.** A 2015-style fat G-buffer
does not fit in it: albedo+AO (4 B) + normal+roughness (8 B) + specular (4 B) + emissive (4 B) + depth
already crowds it, and anything richer needs the raised limit that is *not* guaranteed. This is an
argument for a **visibility buffer** (one 32-bit ID + depth, materials evaluated in a second pass) over a
fat deferred G-buffer — the same argument Karis 2021 makes on bandwidth grounds, arriving here from the
portability side instead.

**Present in core WebGPU** (spec-guaranteed, no feature flag): compute shaders · storage buffers and
storage textures · `drawIndirect` / `drawIndexedIndirect` / `dispatchWorkgroupsIndirect` · reversed-Z via
`depth32float` · `rg11b10ufloat` and `rgba16float` render targets · read-write storage textures (WGSL).

**Optional features, present on the report device**: `timestamp-query` (profiling — this is how
`mods/bench/` gets its numbers), `shader-f16`, `float32-filterable`, `indirect-first-instance`,
`bgra8unorm-storage`, `depth-clip-control`, `texture-compression-bc`, DP4a.

**Optional and ABSENT on the report device**: `subgroups` · `dual-source-blending` · `clip-distances` ·
`float32-blendable` · `texture-compression-astc` and `-etc2` (that device is desktop Intel; ASTC is the
mobile format and the A18 Pro would have it — **unmeasured on our actual targets**).

#### What WebGPU provably cannot do

Searched in the W3C WebGPU and WGSL specifications; each of these terms has **zero occurrences**:

| Absent | Consequence here |
|---|---|
| **hardware ray tracing** (no acceleration structures, no ray queries) | no RTGI, no ray-traced shadows or reflections. Screen-space and compute-marched methods only |
| **mesh / task shaders** | cluster expansion must run as an ordinary compute dispatch feeding indirect draws |
| **geometry and tessellation shaders** | displacement and expansion are compute-side or vertex-side, never fixed-function |
| **multi-draw indirect** | one indirect draw per batch; the count comes from the GPU but the calls do not |
| **bindless / `binding_array`** | material and texture indexing goes through atlases or texture arrays, not a descriptor heap |
| **64-bit integers and 64-bit atomics** | see below — this is the load-bearing one |

**The 64-bit hole, exactly.** WGSL §6.2.8: *„atomic<T> — T must be either u32 or i32."* And the note in
its host-shareable-types section: *„WGSL does not have a concrete 64-bit integer type."*
([W3C WGSL](https://www.w3.org/TR/WGSL/)). There is a **draft proposal**, `atomic_vec2u_min_max`
(gpuweb/gpuweb [issue #5071](https://github.com/gpuweb/gpuweb/issues/5071), proposal file
`proposals/atomic-64-min-max.md`, created 2025-09-15), which would add exactly `atomicStoreMin` /
`atomicStoreMax` on a `vec2u` surrogate, storage buffers only, no return value. Its own requirements
section records that **Metal restricts 64-bit atomics to min/max and to feature family Apple 9** — which
is the A17 Pro/M3 generation and later, so an **A18 Pro can do it in hardware; WebGPU just does not
expose it yet.** Vulkan coverage cited there: `shaderBufferInt64Atomics` 87.7 % on Linux, **31 % on
Android**.

Why that one gap matters more than the rest is [`lod.md`](lod.md)'s subject, and it is worked through there.

#### Where a modern method beats the 2015 method, with its source

| 2015 method | Modern replacement, WebGPU-feasible | Evidence |
|---|---|---|
| SSAO / HBAO ([`stages/ao.md`](stages/ao.md)) | **GTAO** — closed-form horizon integral, matches a ray-traced ground truth | Jimenez et al., *Practical Real-Time Strategies for Accurate Indirect Occlusion*, SIGGRAPH 2016 Course. Reports **0.5 ms on PS4 at 1080p** for GTAO+GI at a half-res occlusion buffer — cheaper than what §1 budgets, on weaker hardware. Pure compute, no ray tracing |
| CSM, fixed cascade split ([`stages/shadow.md`](stages/shadow.md)) | **Virtual shadow maps** — paged, cached, resolution follows screen-space need | Nanite's companion system (Karis et al. 2021 and the UE5 documentation). Feasible in principle by compute + indirect draw; **no WebGPU implementation found** (`## Gaps`) |
| baked GI / ambient + sky | **radiance cascades** — probe cascades with distance-proportional angular resolution, no ray-tracing hardware | Osborne & Sannikov, *Radiance Cascades: A Novel High-Resolution Formal Solution for Multidimensional Non-LTE Radiative Transfer*, [arXiv:2408.14425](https://arxiv.org/abs/2408.14425), 2024. **Caveat, stated because it matters:** the paper is astrophysical radiative transfer, not a game-GI paper. The formulation is primary; the game application is not published there |
| baked GI / ambient + sky | **DDGI** — irradiance probes with per-probe visibility, no hardware RT required if the trace is a compute march | Majercik et al., *Dynamic Diffuse Global Illumination with Ray-Traced Irradiance Fields*, JCGT 8(2), 2019 |
| FXAA / SMAA | **TAA** — and §2 has the sourced conditions under which it works | Karis, *High-Quality Temporal Supersampling*, SIGGRAPH 2014 Advances |
| alpha-cutout foliage with a fixed threshold | **hashed alpha testing** — object-space-anchored hash instead of a fixed 0.5 threshold; distant foliage stops dissolving | Wyman & McGuire, *Hashed Alpha Testing*, I3D 2017. Measured cost at 1920×1080 on a GTX 1080: European Beech 386 k tris **0.39 ms → 0.50 ms** vs. traditional; UE3 FoliageMap 3 000 k tris **2.52 ms → 2.86 ms**. Stochastic (unhashed) is 3–4× worse: 1.69 ms and 11.42 ms |
| discrete LOD ladder per asset class | **cluster-DAG virtualised geometry** (Nanite class) | Karis et al. 2021 — judged in [`lod.md`](lod.md), because it is a decision, not a menu item |

### 2.2 Procedural first; Blender only where procedure will not reach

> Owner: *„wo nötig in Blender modellieren und Texturen erzeugen. aber was immer geht prozedural."*

A strict order of preference, and a producer must justify stepping down a rung:

1. **Grown or generated at load/build time from parameters** — trees, grass, ground cover, perennials,
   bark, leaf venation, extruded buildings, terrain detail. Resolution-independent, diffable, and one
   parameter change restyles a whole species. This is the default and it covers most of the world.
2. **Procedurally baked in headless Blender**, checked in as glTF — where growth rules do not describe the
   shape: a vehicle, a specific landmark, a piece of furniture. Still a script with named, sourced
   dimensions, never a hand-pulled mesh. **Version the recipe, never the cake**: what is committed is
   the script, the dimensions it was produced from and their sources; a `.glb` is a build output and
   stays untracked.
3. **Authored by hand** — only where neither reaches, and named as an exception when it happens.

The same order applies to textures: generated in the shader or baked from a generator beats a painted
image, because a generator carries its own provenance and rescales for free.

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
The rule, its threshold and the mechanism that must not be re-invented per layer are in
[`lod.md`](lod.md).

### 4. The world is procedural; OSM is an overlay

> Owner: *„Wir müssen überall erfinden. OSM daten haben nur vorrang. Ohne OSM haben wir eine Welt in der
> Steinzeit mit dichter Vegetation. Mit OSM haben wir gebäude, strassen, felder."*

The default state of the world is **nature**, and OSM *overlays* civilisation where it is mapped. The
fallback for missing data is not emptiness and not a white tile — it is wilderness, which is also the
correct answer for most genuinely unmapped land.

**Aerial imagery is not a visual source.** A photograph carries baked lighting, season, shadows and
parked cars; it cannot be relit, and time of day and weather are declared per mission. Imagery is used
**only as a coarse albedo hint at DEM resolution**. Everything visible is generated.

One consequence worth stating: procedural detail is resolution-independent, where imagery runs out.

**How that world is actually produced has four files of its own**, because none of it is a pass:

| Subject | File |
|---|---|
| the chain before the first pass — albedo + position + OSM → class, and the same structure for buildings from geo-coordinate, base albedo, epoch and decay | [`classification.md`](classification.md) |
| the ground as a **material** rather than a colour, and the stand it carries as a fragment term | [`stages/terrain.md`](stages/terrain.md) |
| the 256 templates, the 0–40 m stack, the species | [`vegetation.md`](vegetation.md) |
| continuous LOD: screen-space error, the Nanite judgement, FLIP — it binds **every** pass | [`lod.md`](lod.md) |

One document per render pass sits under [`stages/`](stages/); the orchestrator and the pass topology are
[`renderer.md`](renderer.md).

### 6. Acceptance

| Contract | Anchor |
|---|---|
| It holds the budget | 720p30 sustained on A18 Pro **after thermal throttling**, not at peak |
| Nothing pops | a scripted descent from 12 000 m to the deck, captured; no visible mesh swap in any frame pair |
| Edges are quiet | the same descent over dense foliage; no crawling on alpha-cutout leaves |
| The look is the bar | side-by-side against a Witcher 3 / Fallout 4 reference frame, judged by the critic pair, at the same altitude regime |
| Pretty beats correct | where the two conflict, the record says which was chosen and why — a decision, never a drift |

## State

**Of the bar and the budget in this file, nothing is built.** There is no governor, no bench mod, no
ground-truth differential, no thermal measurement and no anti-aliasing of any kind — so neither half of
the contract „hold 720p30, then spend everything left on quality" has a mechanism.

What *is* built is passes, and each one carries its own honest state: the atmosphere chain is the
furthest advanced ([`stages/atmosphere.md`](stages/atmosphere.md)), which is also the part §3 says carries
the high-altitude image, so that is fortunate rather than planned. The low-altitude regime §3 names —
vegetation, buildings, materials, shadows, ground detail — is where the empty `## State` sections are.


## Gaps

- **„Cinematic" has no anchor.** §2 states it in words. Which grade, which grain, how much depth of field
  — undecided, and it is the kind of thing that drifts into taste unless a reference frame is pinned.
- **The ground-truth differential (§1.3) does not exist.** The pieces do — headless Blender runs for
  assets, `gpu_walk` is already named the frame oracle — but nothing renders a matched pair or measures
  the distance between them.
- **The floor is unknown.** §1.3 rests on measuring the engine-vs-Blender gap at *maximum* quality first.
  That number has never been taken, and if it is large, the governor's contribution disappears inside it.
- **The governor does not exist**, nor do the knobs it would turn. Today quality is fixed and the frame
  rate floats, which is exactly backwards.
- **`mods/bench/` does not exist**, so §1.2's one comparable number — the step at which the governor
  broke — has never been taken on any commit.
- **No thermal measurement.** §1's numbers are all peak or nominal. What the sustained clock is after ten
  minutes of continuous rendering is unmeasured, and Twitch means continuous by definition. Every frame
  budget here rests on it.
- **§1's WebGPU limits and `doc/webgl-webgpu-report.txt` are from different devices and neither is a
  target.** §1 attributes 2 GB `maxStorageBufferBindingSize` to Xbox Edge; the checked-in report shows
  1 GB on an Intel HD Graphics machine via ANGLE (2560×1440, 4 cores). Both may be true of their own
  device. **Nothing in this file is measured on the A18 Pro, which §1 names as the budget.** The spec
  *defaults* in §2.1 are verified against the W3C text and are the only numbers here that hold
  everywhere.
- **ASTC on the actual targets is unmeasured.** The report device is desktop Intel and reports BC only.
  Texture compression availability decides whether §1's bandwidth argument survives contact.
- **Witcher 3 and Fallout 4 are unsourced in this file.** §2.1 documents GTA 5 from Courrèges' frame
  study and deliberately asserts nothing about the other two. Attempts to reach CD Projekt RED's
  REDengine 3 / landscape GDC material and Bethesda/NVIDIA's Fallout 4 volumetrics material failed
  (dead GDC Vault mirror; the search engines reachable from here return CAPTCHA challenges). **Their
  anti-aliasing in particular is unknown to this file** — GTA 5's being FXAA is established, and
  generalising from one title to three is precisely the error being corrected. Until a primary source
  is read, §2.1's bar rests on one title.
- **The fixed-function raster path is *less* specified than the shaders**, and it lands exactly on §2's
  priority investment: edge coverage „not defined", the **multisample resolve algorithm is not specified
  at all**, and alpha-to-coverage is „platform-dependent and can vary for different pixels" and not
  monotonic in alpha. Alpha-cutout foliage is the worst case in the scene and three of those four
  sentences aim at it. See [`gpu-determinism.md`](gpu-determinism.md), and
  [`stages/tonemap.md`](stages/tonemap.md) for the pass it lands in.
- **No WebGPU virtual-shadow-map implementation was found.** §2.1 lists VSM as the modern replacement
  for CSM on the strength of Epic's system; nothing establishes it is reachable within this budget.
  [`stages/shadow.md`](stages/shadow.md) carries what is built instead.

## Knowledge

Derivations, measured constants and the conditions this file's Spec rests on. Every entry names its
origin; where none exists the entry says so instead of supplying one. The derivations that belong to one
pass or to one cross-cutting mechanism live in that file's own `## Knowledge` — the screen-space-error
threshold and the TAA/dither conditions in [`lod.md`](lod.md), the soil-albedo measurement in
[`stages/terrain.md`](stages/terrain.md), the exposure constant in
[`stages/tonemap.md`](stages/tonemap.md).

### WebGPU: verified spec defaults, and the one hole that matters

Verified against [W3C WebGPU](https://www.w3.org/TR/webgpu/) §Limits — §1's „WebGPU default" column is
correct: `maxStorageBufferBindingSize` 134 217 728 B (128 MiB), `maxComputeWorkgroupStorageSize` 16 384 B,
`maxComputeInvocationsPerWorkgroup` 256. Additionally `maxBufferSize` 268 435 456 B (256 MiB),
`maxColorAttachments` 8, **`maxColorAttachmentBytesPerSample` 32 B**, `maxBindGroups` 4,
`maxStorageBuffersPerShaderStage` 8, `maxStorageTexturesPerShaderStage` 4, `maxTextureDimension2D` 8192.

**The 64-bit hole, in the specifications' own words:**

* WGSL §6.2.8 Atomic Types: *„atomic<T> — Atomic of type T. **T must be either u32 or i32**."*
* WGSL §Host-shareable types: *„Note: **WGSL does not have a concrete 64-bit integer type**."*
* Atomics are further restricted to *„variables in the workgroup address space or … storage buffer
  variables with a read_write access mode"* — **no texture atomics**, so a visibility buffer must be a
  storage buffer regardless.

**Draft remedy:** `atomic_vec2u_min_max` (gpuweb/gpuweb `proposals/atomic-64-min-max.md`, created
2025-09-15, [issue #5071](https://github.com/gpuweb/gpuweb/issues/5071)), adding `atomicStoreMin` /
`atomicStoreMax` on a `vec2u` surrogate, storage buffers only, **no return value**. Backend coverage the
proposal itself records:

| Backend | Requirement | Coverage it cites |
|---|---|---|
| Vulkan | `VK_KHR_shader_atomic_int64` / Vulkan 1.2 `shaderBufferInt64Atomics` | 87.7 % Linux, **31 % Android** |
| Metal | 2.4; **min/max only**, feature family **Apple 9** | Apple 9 = A17 Pro / M3 and later — so an **A18 Pro has it in hardware** |
| D3D12 | SM 6.6 `Int64ShaderOps` | — |

So the constraint is the **API**, not our hardware, and it is dated rather than permanent.

**Absent from the W3C WebGPU and WGSL specifications entirely** (zero occurrences of each term):
ray tracing / acceleration structures · mesh and task shaders · geometry and tessellation shaders ·
multi-draw indirect · bindless / `binding_array`.

### Sources

| # | Source |
|---|---|
| 7 | Karis, Stubbe & Wihlidal, *A Deep Dive into Nanite Virtualized Geometry*, SIGGRAPH 2021 Advances in Real-Time Rendering — https://advances.realtimerendering.com/s2021/Karis_Nanite_SIGGRAPH_Advances_2021_final.pdf |
| 11 | Karis, *High-Quality Temporal Supersampling*, SIGGRAPH 2014 Advances — http://advances.realtimerendering.com/s2014/epic/TemporalAA.pptx |
| 13 | Jimenez et al., *Practical Real-Time Strategies for Accurate Indirect Occlusion* (GTAO), SIGGRAPH 2016 Course |
| 14 | Majercik et al., *Dynamic Diffuse Global Illumination with Ray-Traced Irradiance Fields*, JCGT 8(2), 2019 — https://jcgt.org/published/0008/02/01/ |
| 15 | Osborne & Sannikov, *Radiance Cascades*, arXiv:2408.14425, 2024 — https://arxiv.org/abs/2408.14425 |
| 16 | Courrèges, *GTA V — Graphics Study*, 2015 — https://www.adriancourreges.com/blog/2015/11/02/gta-v-graphics-study/ |
| 20 | W3C, *WebGPU* and *WGSL* specifications — https://www.w3.org/TR/webgpu/ · https://www.w3.org/TR/WGSL/ |
| 21 | gpuweb, *64 Bit atomics (storage buffers)* draft proposal — https://github.com/gpuweb/gpuweb/issues/5071 |

**The numbering is shared across `doc/render/`** and this file started it: a number means the same paper
in every file. The rows that are not here went with the claims they support — 1–6, 8–10, 12, 17, 19,
22, 23 to [`lod.md`](lod.md), 18 and 25–29 to [`stages/terrain.md`](stages/terrain.md), 24 to
[`stages/tonemap.md`](stages/tonemap.md), 30–33 to [`vegetation.md`](vegetation.md), 13 additionally
to [`stages/ao.md`](stages/ao.md).
