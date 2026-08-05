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

This makes *„alles im Shader, minimal Texturen"* the correct engineering choice rather than a saving:
procedural shading spends ALU to save bandwidth, which is exactly the trade a mobile GPU wants.

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
- **The 256 templates do not exist**, nor does the quantisation, nor the latitude/elevation filter.
- **No thermal measurement.** §1's numbers are all peak or nominal. What the sustained clock is after ten
  minutes of continuous rendering is unmeasured, and Twitch means continuous by definition. Every frame
  budget here rests on it.
- **Building extrusion and roofs** are named in the goal and absent from the tree.
