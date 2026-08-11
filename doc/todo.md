# Todo

What is next, in order. A step names what must be true when it is done, not how — short enough to scan.
**A step that is done leaves this file**: it is then in the code, and its measurements are in `git log`.
What survives a finished step is only what a later step needs to know.

Order is not preference: 1 and 2 are preconditions for anything that places anything, and 4 is the gate
that makes 5 onwards enforceable rather than intended.

**Quality work does not start until this file is empty.** The skeleton is best practice first; after that
everything further is generators and shaders, and step 4 is what makes that true — a generator that
includes the renderer is then a compile error. Until then a picture finding is recorded and ranked, never
graded against the references: the answer is known and the round is spent. Then, in two stages:

| | |
|---|---|
| **1** | one scenario, scenes at places the owner knows well. Render, look, pull generators and shaders after it, loop until the quality stands. **Judged by eye, not by metric** — a number decides whether the frame floor holds, never whether it looks right. Ortho­graphic diversity matters here and nowhere earlier: a structural step only has to prove it did not move the picture, and one meadow shows that |
| **2** | the webcam scenario, several cameras, times of day and night, weather. Against those the whole lighting and weather model is fitted. It renders at the declared size and downscales **both** sides to the comparison rung; camera poses are resected against buildings, and `git log` holds six already fitted |

## Where it stands

**The nine structural steps are done.** Classification off the render thread · the height oracle on the
drawn surface · the misfiled moved and the dead deleted · the server target and the fall of the checker ·
`generators/` · the forest as a generator · one geometry stage · regionalisation · buildings and water.
What each one measured is in `git log`; what it built is in the code.

**The list below is what is left, and it is a different kind of list.** The steps were structure and
their acceptance was a number. These are defects and absences, ranked by what destroys the impression
fastest or costs the most to leave — and the picture verdict is no longer suspended, because the
structure stands.

## 1 — One vegetation stratum against the reference's three. This is what "lush" means.

KCD's forest area is **canopy + undergrowth + grass, superposed from one declared preset**, plus
mushrooms and herbs — one Area Filler, three layers. Ours is one `Stem` and a stands-per-m² per class.

**Our density is right and the picture is still empty.** The densest declared class is 0.033/m² =
**330 stems/ha**, and a near-natural beech stand at Serrahn measures **263 stems/ha ≥ 7 cm dbh**, basal
area 33 m²/ha. The number is correct *for the canopy* — and everything below 7 cm dbh is neither in that
count nor in our world. The near crown reading as flakes, the ground being a shader and nothing else, and
`osmDefault` growing 700 blades/m² on Death Valley's floor are **one gap, not three**.

**No authored dependency.** The Area Filler preset is a declaration and the distribution is random; this
transfers as JSON, not as a substitute.

The shape matters as much as the content: the stratum table is **required per class with no global
default**, so `osmDefault = "meadow"` becomes *unspellable* rather than a written-down defect — an
unclassified place then grows nothing, which is correct. And strata are a declared list, not three named
members: a closed enumeration here is the "adding a creature means editing seven files" failure
`vision.md` names.

Done when: `subject-meadow` writes a non-zero `fillPct` — today all 57 frames are bare substrate and the
bench has no herb geometry path at all · a forest-floor subject fills within the band its class declares ·
and **overdraw p95 is published**, because three strata is exactly where a forest stops being free.
**Passes: 0.**

## 2 — A stand appears in exactly one rank per frame, and the far rank is one card

The recorded one-second killer: a zigzag/bow-tie with right-angle corners at five to six crowns, and
15 995 stands that vanish seen from directly above.

**The reference cannot produce either failure.** KCD's UBERLOD — Warhorse's own, not CryEngine's — bakes a
tree's last LOD to **one plane with alpha forbidden**, merges every plane in a **64 m cell into a single
draw**, and expands each element's corners from **UV channel 2** in the vertex shader so they face the
camera *individually*. There is never a second quad, so there is no cross to see edge-on. Switching
carries **16 m of hysteresis**, a **16 m minimum observer movement before anything updates at all**, and a
**per-frame update budget**.

Deviation with its reason, and it is the camera envelope rather than taste: **hemi-octahedral, not
single-view, because Outshine has a bird's eye and KCD does not.** The top cell is the one that must
exist. What transfers whole is the *discipline* — hysteresis, an update budget, a merge per fixed spatial
cell.

**No authored dependency.** Their bake is an offline tool over an authored mesh; ours bakes at load from
our own grown prototype — the cache of a computable function, principle 2, admissible.

Done when: a counter proves **no stand is submitted at two ranks in one frame**, exactly, not
statistically · a counter proves **no sampled impostor cell lacks a bake** · `demo/ortho`'s non-substrate
coverage is within a stated fraction of the oblique view's, where today every stand vanishes · and **a
moving capture** decides the bearing-band prediction: with uniform yaw a cross presents its edge-on aspect
in a coherent band, so if the band survives, the cause is the cell seam and not the cross. A still cannot.
**Passes: 0 in frame.**

## 3 — The water level puts most bodies under the ground

Measured twice by two instruments: over sixteen regions **all nine** outlines report the level below the
ground at their own centroid, worst **6.5 m**; over 43 rings around the demo, **34 of 42**. The mechanism
has two parts. `floor(0.05·(n−1)) = 0` for any ring under **22 points**, so for a farm pond the "fifth
percentile" **is the minimum** — 28 of 43 rings — and the minimum of *n* DEM samples with σ ≈ 2–4 m is
biased low by ≈1.5σ at n = 9. And the ring is the **bank**, not the shore: worst-case spread across one
ring is 11.65 m. The model is exact where it was calibrated — the four Weser polygons at 152…187 points
answer 0.00 — and wrong everywhere small.

`Tessellate` emits the surface at `level + 0.15 m`, so for those bodies the water plane lies **under the
drawn terrain**: a fringe at the median, fully buried at the tail. Water is the strongest tonal element in
a landscape at the comparison rung.

The published answer is **hydro-flattening** (USGS *Lidar Base Specification*): a lake polygon is
flattened to a constant elevation at or just below the surrounding terrain, and a **river** polygon
carries a monotone downstream gradient rather than a constant — which the engine already enforces for
water *lines* twelve lines above the polygon branch in the same file. Two moves: carve the region's ground
patch to the level under the outline so the two models cannot disagree, and give a polygon a gradient
where its ring's spread exceeds the DEM's vertical noise.

## 4 — Nothing in the frame occludes between 1 m and 20 m, which is the whole of a tree

| | radius it can serve |
|---|---|
| `AoStage`, `kAoRadiusM = 0.9`, half-res | ≤ ~1 m — contact shading |
| shadow cascade 3, 4 × 1024 over 600 m | ≈ **1.2 m per texel** — cannot self-shadow a crown |
| **the gap** | **1 m … 20 m** |
| the reference's cone max length | **8 m** |

A crown at LAI 4.5–5.1 must read as **one mass with a lit top and a shadowed underside**. Density is half
of that and item 1 owns it; the other half is that **no term in our frame can darken a crown's interior or
the ground under a canopy**, and the reference's answer to exactly that is an 8 m cone — not a shadow map
and not SSAO. Crytek ship `e_svoTI_SSAOAmount` to scale SSAO *down* when the cones are on: they are a
declared scale pair, cones own the metres and SSAO owns the centimetres.

Build the cheap candidate first: **coarse world-space sky visibility over the cluster DAG's own bounds**,
per vertex — zero new passes, zero new full-screen targets, bounded work, reusing geometry the frame
already has. Voxel cone tracing **in the AO pass's existing slot** only if that demonstrably cannot produce
the term; costed at ≈1.6 ms scaled from Crytek's published 2.5–5 ms on Xbox One, plus CPU voxelisation,
which is the class of cost item 7 is already fighting.

Done when: sky visibility at 1.5 m under a closed canopy falls into the band an LAI of 4.5–5.1 implies ·
and the AO span does not grow — which needs `gpuFrameMs` first, so **this is blocked on an instrument, and
that is a cost rather than a limit**. **Passes: 0 net.**

## 5 — Vegetation does not take the terrain's colour with distance

A per-instance tint toward the ground class colour, ramped over range. The reference ships it as
`e_vegetationUseTerrainColorDistance`, 50…80 m. **It is the single mechanism that makes a distant foliage
field read as one mass rather than speckle**, and it is a vertex-stage multiply — the cheapest item on
this list and it removes a symptom items 1 and 2 both describe.

Done when: at 200 m the canopy's chroma variance against its own class colour drops by a stated factor,
measured on a mask **frozen on one frame** and applied to both sides. **Passes: 0.**

## 6 — The ground has one colour per class and no high-frequency term

The reference's rule, stated as a rule rather than a habit: *high frequency into the detail material, low
frequency into the base*, with the detail explicitly **greyscale** and cut at a declared range (theirs
300 m), and the layer-to-layer blend **height-driven** rather than a linear fade, so pebbles poke through
dirt at a boundary instead of cross-dissolving.

It maps onto our constraint almost too neatly: low frequency is class + place, which we have; high
frequency is a **noise function**, which is the only legal form a detail map can take here and is
Ebert/Musgrave/Perlin/Worley's entire subject; and the height-driven blend is buildable from what
`Ground` already delivers — **class, edge distance and runner-up class, which nothing consumes for this**.

Done when: the class boundary's mixing width in pixels at the comparison rung meets a stated target
instead of being a line · and luminance standard deviation in a 10 m near-ground patch rises off the
floor. **Passes: 0.**

## 7 — Nothing evicts, against a heap fixed at 296 MiB

`BuildingField`'s prints and verts, `WaterField`'s surfaces, courses and levels, and `OsmField` grow
monotonically for the length of a walk, and no eviction path exists. `architecture.md` is explicit — the
streamer needs a byte budget and evicts against it, and every pool reports its bytes or it is a leak with
a name. They report; nobody acts. **A fixed heap plus monotone growth is a maximum walk length**, and a
world sandbox has none. The number is one long `demo/ring` run with a column that already exists.

## 8 — The night

**A lamp is placed, not fetched, and the endpoint was never the shape of it.** Two independent
refutations: the served vector schema carries **no street lamps** — the `pois` layer of central Berlin's
z14 tile has 32 keys and `highway` is not among them (versatiles Shortbread, which `tiles/src/tilesrc.c`
fetches) — and a tile-server endpoint producing a *derived* light list would be `fb-tiles` delivering
something principle 6 does not list. Lamps come off the **street centrelines**, which infrastructure has
now made available.

Captured and looked at, `demo/night`, 120 frames, a full turn, sun −21.14° and moon +11.36°: **it is not
a night.** The ground is a flat, chroma-full green meadow with no direction and no moon shadow; trunks
read bright grey; the road is a clean legible band. The sky is pure black with white star dots that clip
(`maxY ≈ 1.0` every frame) — no moon glow, no horizon lift. **Nothing emits anywhere.** Frame mean display
luminance 0.233…0.312 over the whole turn, largest frame-to-frame step 0.0047, and `run irradiance` reads
`skyRGB = 0,0,0`: the only thing lighting the ground is a **constant display crutch** in `SurfaceLight.h`.

So the night is two things and neither is an endpoint: **emission on placed lamp geometry** — `Material`
already has the field and `SurfaceState::Emits()` already derives from it, so the emitters need **no new
pass** and go in the geometry stage that exists — and a **night radiometry that is currently a constant**,
where the moon is a light source with a phase and the sky has a mesopic response. The bow-tie crowns are
at their most legible here, as black wedges against the star field.

## 9 — A declared environment track over the day, and a weather state that blends

Ours: Bruneton, ACES-Narkowicz with **no free parameter**, auto exposure from irradiance. Theirs: a
physical sky whose parameters are **hand-keyed hour by hour**, a film curve whose toe, midtone and
shoulder are keyed per time of day, and a weather **preset picked every four hours and blended into over a
declared interval**.

**This is the one place where copying the reference would make us less physical**, so the entry is narrow
on purpose: key only what has **no** physical answer — the tone curve's shoulder, the fog's radial lobe,
the weather transition length — and **never the sky's radiance**. `core/Keyframes.h` is already the
evaluator and knows none of its consumers; a track is a scenario declaration, which is principle 1 working
as designed.

Done when: one scene at 06:00/12:00/18:00/22:00 gives four pictures whose sky-to-ground ratio agrees with
the model's own prediction · and a declared weather change takes its declared duration rather than one
frame, reproducibly. **Passes: 0.**

## 10 — Material rows with no origin, against a published first-party table

Warhorse published a measured specular/gloss table for exactly our biome's surfaces — grass 50 sRGB /
gloss 38, dry soil 48 / 20, dry leaves 45 / 32, rough stone 50 / 42, thatch 56 / 77, rough wood 48 / 28.
Non-metal specular is 45–65 and never above 80. **The ordering and the ratios transfer directly**; the
mapping from their 0–255 glossiness to our GGX perceptual roughness is documented nowhere and must be
**derived, with the derivation beside the number**.

Worth taking with it: their gloss map lowers gloss where normal variance is high, **to kill specular
shimmer** — an antialiasing technique, not a look choice.

Done when: the count of material rows whose values carry no derivation, measurement or `[SET]` is zero.

## 11 — Fog shadows

`e_VolumetricFog` with `r_FogShadows`, plus keyed volumetric shadow darkening and range. Shafts through a
canopy in mist is the most recognisable forest image the reference has. Our Koschmieder-derived haze is
better founded than their keyed fog and **nothing shadows it**.

It goes **inside a stage that already reads the HDR target** — `TaaStage` does — rather than becoming pass
eight, and their own guidance is that samples per view ray do not grow with range; the range shrinks.
**If it cannot be made to fit inside an existing stage it does not get built**, because 0.246 ms of pure
traffic is its floor before it shades anything.

## 12 — The crossing's remaining 2.4 ms is unexamined, not unattributed

A crossing costs +6.51 ms at p50 over its own neighbourhood; `collectMs` is flat and `populateMs` is 0.055
there. The profile carries seven more columns that were never checked, and **`buildingMs` is the obvious
candidate**: `World.cpp` puts the vector build, the water ingest, the building build **and** the building
DAG in one span, with the DAG recorded at 33.0 ms of a 50.9 ms frame for one dense tile. Redo the
neighbourhood excess over **every** column — one run, no code — and publish `frameMs − Σ(spans)` as its
own column so "unattributed" is measured rather than subtracted by hand.

**And the pool's byte step is still a localisation, not a diagnosis** after two rounds of paying for it:
p50 +1.09, p99 +7.54 ms between 23.2 and 28.0 MiB, one binary, capacity the only variable. Not cache, not
extra work. What remains is an allocator size class or heap pressure changing tile eviction, and the
`evicted` column already exists.

## 13 — Interfaces that write down what they claim to carry

`RegionPool::Extent::Reached` is **never read** — the header says the body budget follows it while the
count is computed by the caller · `FbGroundBlock::Nodes_` is a raw pointer into an LRU slot valid until
the next call into the oracle, enforced by a comment; a generation counter makes it a refusal instead of a
corruption · the oracle's single-threadedness is on the header and is still a sentence, not a type ·
`Buildings::Over` discards a `[[nodiscard]]` in three places, so an invariant is split across two
functions and bridged by a cast · the OSM layer names are spelled in three files · `DrawSink::Add` returns
a `bool` with `Full()` beside it and `ForestDraw.cpp:18` truncates a region silently · **the winding is
hard-coded at seven sites** and gates the first generator draw product: cull mode is pipeline state fixed
at `Configure`, so the shape is a declaration at registration beside the rank.

Two bases for one building: `FeatureTop` takes `BaseM` from the ring's lowest corner while `Buildings::At`
derives its base from the patch at the bbox centre, so on a slope the queried prism floats relative to the
drawn one — ≈1.5 m at 10 % over a 15 m house. `BaseM` is already computed; carry it. · `Sim::Settle` sets
`RoofChecked_` inside the resolved branch, so before the ground lands every pass pays a full `GroundAt`,
outside both `populateMs` and the one-per-turn budget. · `verify-types`' negative gate passes on **any**
compile error; one `grep -q` fixes the class for all three gates.

## Not on the list, and named so it is not mistaken for an oversight

**Ambient specular in enclosed places has no procedural substitute.** Under a closed canopy, in a gorge,
indoors: the reference hand-places a baked probe — measured appearance of an authored scene, which
principle 2 forbids. In the open the sky LUT *is* the correct substitute and is better founded than a
baked probe. Whatever solves item 4 is the only thing in our frame that will know an enclosure exists.

**Their leaf albedo is an authored alpha and colour.** Ours is geometry plus a colour, and at 320×180 that
is not a gap — it becomes one at the top rung where venation and translucency variation speak, and it is
**unsolved**.

**Three of their techniques must not be copied**: CPU coverage-buffer occlusion with authored occluder
meshes (authored *and* CPU-bound, the wrong direction on wasm32); baked environment probes (principle 2);
and the authored detail atlas. The *idea* of a cheap single-material shadow proxy is right and free,
because our LOD ladder already produces one.

**And one place where the house shape is ahead of the reference:** their `TexGenType = World` exists so an
object's material can be made to agree with the terrain's mapping. Our vertex layout declares **uv in
metres, never 0..1**, which makes that agreement structural rather than a per-material opt-in. Do not
trade it away.

## The acceptance instruments

**Four steps below accept on a distribution and two instruments are refuted.** These are not limits;
they are missing tools with a cost, and each blocks a specific "done when".

**The counterbalanced block design is not an instrument.** ABBA's arm contrast is `(+1, −1, −1, +1)` —
exactly the quadratic orthogonal-polynomial contrast over four ordered positions. It removes linear drift
and aliases *curved* drift into the treatment effect at unity gain, so **more blocks make a false positive
more significant**. Measured: two identical arms, 12 blocks, p99 difference −1.86 ms at p = 0.043, and the
p99 means by position (34.10 / 36.36 / 35.71 / 34.26) *are* the effect. Fix, one line: randomise order
within the block — 20 randomised blocks put all three quantiles at |t| < 0.8. Two things go with it:
publish the resolvable floor beside every result (≈1.0 ms p50, ≈2.1 ms p99 at 20 blocks), and **stop using
a run's p50 as the statistic** — inside one 240-frame run `frameMs` climbs from ~11 to ~25 ms as the world
streams in, so it mostly measures how far streaming got. Frame-index-matched on a declared path halves the
noise for nothing.

**The per-pass GPU timer does not partition the frame, natively as well as in the browser.** Native:
compute 0.43 + shadow 3.11 + scene 7.61 + ao 5.72 + taa 6.43 = 23.3 ms against p50 9.38. A begin/end
timestamp pair measures a span on the GPU timeline; pipelined passes overlap and must not be summed. It
licenses exactly one statement — *did pass P's span move between two builds of the same declared scene* —
and never "pass P costs N ms". **Step 7 accepts on "the scene pass stays flat within noise" and cannot be
evaluated with it.** The missing tool is one column: a single pair spanning the whole encoder,
`gpuFrameMs`. Then `Σpass / gpuFrameMs` says which regime holds — above 1 means overlapping spans and
attribution is forbidden. Two query slots, no new code path. Also `FrameTelemetry.cpp:66-72` publishes each
pass as a **mean** against `CLAUDE.md`'s "never a mean", and compares it to a frame percentile.

**Nothing compares the picture across the two clients.** `verify-clients` proves both are entry points
over one scene builder and no more; measurably they differ, and byte-identity is dead as an instrument
here as everywhere. The tool, and it is cost rather than a limit: **the wasm client must execute a `runs`
block and return the still and the depth**, the product `SceneRunner` already writes natively — `fb-sim`
collects log and telemetry over HTTP, so the sink exists and the missing piece is a readback and a POST.
Both sides pinned: declared size, `jitter` pinned, `settleFrames` declared, same instant, same scene.
**Publish the self-noise floor first** — native against native and browser against browser — because a
tolerance without a zero point decides nothing. The statistic is **coverage, not colour**: the fraction of
pixels whose sky/not-sky classification differs against a mask frozen on one side, which separates a
wedge of thousands from TAA edge noise of hundreds by an order of magnitude. And it runs in **motion**: a
seam that appears and vanishes as the camera walks is a cell-seam pop, one welded to a stand is a card
defect, and one frame cannot tell them apart.

**Geometry has invariants, and they are decidable without any reference at all.** Not consistency, not
plausibility, not taste — true or false, and cheap. Nothing checks them today, and a wrong normal makes
the shading wrong everywhere without ever looking like a shading defect. What a mesh check answers, per
mesh, on the generator's own output:

| | |
|---|---|
| **normals** | unit length within tolerance · agreeing in sign with the triangle winding · agreeing with the geometric normal within a stated angle. A vertex normal that disagrees with every face it belongs to is a defect regardless of how it looks |
| **welding** | no two vertices at one position carrying the same attributes — a split vertex is legitimate **only** where a seam is declared (uv, material, hard crease), so the check is against the declaration, not against the count |
| **closure** | every edge shared by exactly two triangles where the body **declares itself closed**. A building is closed; a terrain patch, a leaf card and a water surface are not. So closure is a **declared property of the yield**, and the check enforces what was declared rather than one rule for everything |
| **degeneracy** | zero-area triangles, NaN or infinite coordinates, indices past the end, a winding that flips within one surface |

It belongs to the generator contract: the yield declares `Closed`, and the check runs on what a generator
produced — which is where it is cheapest and where a violation names its author. Until then it can run on
what exists: the terrain mesh, the building footprints, the bark.

**The drawing steps have hard instruments and none of them is taste.** What decides whether many trees
and houses draw fast is measurable, and most of it is not measured here yet:

| | What it answers | Today |
|---|---|---|
| **screen-space error, measured** | the ladder declares τ pixels; nobody has measured the actual pixel deviation between the chosen cut and the finest. Render both, difference the silhouette. It decides whether the ladder is too tight (triangles wasted) or too loose (popping) — and popping is then a *predicted* defect rather than a discovered one | not measured; the tool is two renders and a difference |
| **triangle size distribution** | the number for foliage. A triangle smaller than a 2×2 quad wastes three quarters of the shading, so a scatter that looks cheap in triangles can be ruinous in fragments. p50/p95 of projected triangle area in pixels | not measured |
| **overdraw** | fragments shaded per output pixel. Alpha-tested cards shade many times what they keep, and this is where a forest actually costs | not measured |
| **culling yield** | primitives submitted against primitives that produced a visible fragment, per stage — frustum, backface, small-triangle, occlusion. A stage submitting what it cannot see is doing arithmetic for nothing | not measured |
| **early rejection** | fragments killed before the shader runs. Its counterpart is the depth order the passes are submitted in | not measured |

Each is a number a GPU or a readback answers, none needs a reference outside the tree, and none is a
matter of opinion. **Appearance is the exception and it is judged from the screenshot, at the image** —
not from a metric standing next to it.

**When performance work happens is a trigger, not a schedule.** At 720p60 nothing is optimised. When
720p30 can no longer be **held** — the floor, p99 under 33 ms, not the mean — it is optimised back up to
720p60. The steps below that carry millisecond acceptance numbers are architecture and run in order
regardless; the trigger governs work that is *not* on this list.

## Later

- GPU emitter for scattering, with the C++ generator as its oracle. **Not before 8** — without the
  per-region number every move is guessed.
- Split the tile loader: the cache and height half is server-side, the mesh and DAG half is picture-side.
- **The wasm link's optimisation level is an artefact, not a decision.** Everything else builds at the
  higher level; the browser link and one translation unit do not, with no reason written anywhere.

## Traps if they wait

- The tone-mapping slot in the pass enumeration is empty since the fold. A dead slot is where a new pass
  hides without the count moving.
- The comment on the vegetation row claims a size the structure no longer has, and it is uploaded verbatim
  with its field meanings pinned against the shader.
- `scenarios/` is the decided name; the tree still says `mods/`.
- Comment density is far above the rule, and the worst file is more than half prose.
- **A pinned binary does not reproduce its still.** Bounded: two of three states are the same picture to
  within one code, the third differs in two pixels of 921 600 on four scanlines at one silhouette edge.
  Byte-identity dies as an instrument; tolerance comparison does not. It is a determinism violation —
  the result is coupled to tile arrival order. **Likeliest cause, and cheap to test:** a temporal pass
  whose history length depends on pace. Shoot once with that pass off; if byte-identity returns, the
  coupling is the history.
- **A bright untextured kite behind a near crown**, reading as a hole in the sky. Two adjacent frames
  cannot say whether it is standing or temporal, and the two hypotheses separate on a capture that
  already exists: does it **rotate with the camera** — an impostor card sampling an empty atlas cell — or
  stay **welded to the stand** — the same stand submitted at mesh rank *and* impostor rank in one frame?
  Right in either case: a stand appears in exactly one rank per frame, and an impostor cell that has no
  bake is never sampled.
- **Seen from directly above, all 15 995 stands vanish.** `demo/ortho` shows sparse dark dots where a
  forest stands — camera-facing cards seen edge-on. The same representation that gives the bow-tie from
  the side gives nothing from above, and a world sandbox has a bird's eye. Step 6.
- **`subject-meadow` writes its product and the product is empty.** All 57 frames are bare substrate,
  grid and grey card, `fillPct=0` in both clients. `SubjectBench::Select` sets `Bucket_` and `Kind_ =
  Herb` and `Bucket_` is never read again: there is no herb geometry path in the bench. `subject-beech`
  fills 17.2 %, so the rig is sound and the subject is missing.
- **The bow-tie now has a falsifiable prediction.** Until step 6 every yaw in the forest lay in
  **[0, 0.088°]** — `h >> 20` on a 32-bit hash yields 0…4095 — so 16 000 stands were one unrotated clone.
  With uniform yaw a cross-quad presents its edge-on aspect in a **coherent bearing band**, not
  sporadically. If the zigzag band survives the new yaw unchanged, the cause is the impostor cell seam
  and not the cross. One moving capture answers it.
- **The mid-distance crowns read as hourglass / bow-ties, and this is the one to fix first.** Silhouette
  is the only currency at the comparison rung and a bow-tie is not a shape a tree has — the impression
  dies in well under a second. Signature: a two-quad cross seen near edge-on, or a card whose alpha cut
  leaves the waist. What is right: the cross never survives to the range where its own geometry is
  legible; SpeedTree practice and Guerrilla's *Horizon* hand that band to a camera-facing or octahedral
  impostor first. Step 6.
- **The near crown reads as scattered dark flakes rather than one mass.** A beech stand carries LAI
  4.5–5.1 m²/m²; at any value in the measured 1.7–7.5 range a crown occludes essentially all sky through
  its own depth and must read as one mass with a lit top and a shadowed underside. Separated dark
  elements mean card coverage an order of magnitude under the species' own leaf area — density and
  self-shadowing, not detail.
- The foreground sward is a tonal field inside 10 m, and at the comparison rung that is legitimate:
  blade structure does not speak there. Recorded, not owed.
- **The mid-distance impostor trees are not tree-shaped.** Looked at directly in `demo/walk` at 1280×720:
  five to six of them carry a large angular zigzag of foliage — folded-ribbon or bowtie silhouettes with
  right-angle corners, not a crown with a bite out of it. An octahedral-impostor cell seam, in both
  clients. It is by a distance the most damaging thing in the frame at one second of looking. Recorded,
  not worked: the vegetation goes through the generator cut at 6 and 7 anyway.
- `GpuTimer::Pass::Cloud` is the enumeration's dead slot.
- **"Pending" and "hole" are the same `double`, and the two clients resolve it oppositely.**
  `kFBElevationUnresolved = -1e9` with `ElevationResolved(m) { return m > -1e8; }` — and **six** bare
  literals test it by hand (`WaterField.cpp:57,94`, `BuildingField.cpp:54`, `TreeField.cpp:81,89`).
  Natively the oracle blocks, so pending never occurs; in the browser a pending stand is **silently
  dropped and never retried**, indistinguishable from "nothing grows here". The shape that fixes it is a
  return type: `GroundSample { enum class State { Resolved, Pending, Hole }; double AslM; }` — then a
  caller cannot place on a sentinel, deferral must be written deliberately, and the six literals cannot
  be spelled.
- **`World::Center` puts every tile centre at `alt = 0`**, so `WantSplit`'s distance at a standpoint of
  altitude *A* is at least *A*: a pedestrian at 2688 m gets a coarser tile under his feet than one at
  −75 m. `AddWork` already prefers the node's real ECEF origin when a mesh exists — the quantity is not
  missing, it is used in one of three places. Same function, same class of error as the next line, and
  they are one fix: **`WantSplit`'s distance is measured to a point that is not on the terrain, and its
  focal length is not the projection's** — for `demo/ortho` the true on-screen tile edge is
  `SpanM × Height/orthoM`, the ladder computes `SpanM × 443.4/2500`, so it stops splitting at ≈2.4 ×
  `kEdgeTau`. Under `orthoM > 0` the honest metric is distance-free.
- **The height bound is stated in two places with no bridge.** `ChunkSurface.h`'s
  `kSurfaceAgreementM = 9.17e-4` and `tools/surface_budget.py`'s `sum(envelope())` are one statement;
  nothing compares them, so step 7 moving `kGrid` or the ladder makes the header lie silently. Three
  lines of Python: regex the constant out of the header, compare, non-zero exit.
- **`TerrainLoader.h:49` still says the DEM pointer is "owned by the byte cache — do NOT free".** It is
  now `fbs_hold.data()`, valid only until the next `fbs_size`. One caller today and it copies
  immediately; 4.5 adds callers to this island.
- **The chain is faithful to its source; the error is in the tile data.** `fb_world` against the z14
  terrarium texel at the same point: demo +0.09 m · Ardèche +0.70 · Preikestolen +1.96 · Badwater −0.03.
  So the gap to the published 604 m and the surveyed −85.5 m is the **source's** vertical accuracy, not
  the engine's — and Badwater is the sharp one, because a salt pan has no smoothing alibi: **10.9 m on
  flat ground**. Two confounds first: whether the declared scene coordinate is the landmark, and what
  the server's native maximum zoom is at each place (`/t/terrain/15/…` returned non-PNG at both, so z14
  may be the finest served).
- **Water depth can be negative and the type permits it.** `LevelM` is the 5th percentile of the ring's
  own **shore** samples, so inside a gorge the DEM sits above it and Ardèche answers −4.371 m. Depth
  should be non-negative by construction, with "the level model and the DEM disagree here" a state the
  return type carries rather than a negative number a caller must remember to check. Step 9.
- **`Sim &Simulation()` is non-const and makes a new mistake spellable.** `App.Simulation().Look(s)`
  compiles and moves the eye without the camera basis and scene state that `Outshine::Look` sets. All 20
  call sites are reads; drop the non-const overload.
- **Three names for one sentinel** — `World::kNoSurfaceAslM`, `Standpoint::kNoRoof`, and bare `-1.0e30`
  in `WaterField`, `BuildingField` and `TreeField`. The producers cannot name the constant because it
  sits in `World.h` and peers do not include peers; it belongs in `core/`, or it dies the way its
  elevation twin does — as a state in the return type.
- **The negative gate does not assert why it fails.** Any compile error passes `verify-generators`; one
  clause fixes the class: `2>&1 | grep -q "file not found"`.
- **`core/ClusterDag.h:75` reads `FB_TAU` from the environment into a function-local static.** The
  picture depends on an undocumented environment variable, and the value carries no origin at the call
  site. Principle 7 — if the environment decides the result, the coupling is a bug.
- **The transmittance LUT cannot answer how much blue the atmosphere removes on the path to a lit
  surface.** Tapped for that purpose it returns a 4.7 % blue lift where the same model's own extinction
  gives 19.6 % — the wrong instrument for the question rather than a mis-tuned one. March instead, when
  the deck base gets a source.
- **`osmDefault` is one global `"meadow"`.** Unmapped land anywhere on Earth grows 700 blades/m²: Death
  Valley's floor is a sward, and there is no arid template among the thirteen. The fix is a per-place
  default and it belongs to the vegetation generator.
- White limestone and rock patches read as snow at 36 N in August.
- `WaterField.cpp:57,92` and `BuildingField.cpp:54` test the unresolved-elevation sentinel with a bare
  `<= -1e7` while `core/ElevationProvider.h` declares `ElevationResolved()` as `> -1e8` — one predicate,
  three literals, two thresholds.
- The near-field ground is a shader and nothing else — no ground cover is built at all, so "no blades" is
  an absence, not a defect. It belongs with undergrowth after 9.
- German comments from earlier rounds. The history stays; what is touched gets translated as it is
  touched.
- **Naming needs a pass of its own.** A name that needs a comment is the wrong name. Borrowed jargon and
  magic sentinels where the type system has an answer are the two patterns. New identifiers are held to
  this as they are written; the existing ones are a separate sweep.

## Open, owner's decision

- A persistent server is declared in `vision.md` and not built. When it is, either the web host absorbs it
  or a third container appears — both touch a principle that says everything runs in the client.
