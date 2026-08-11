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

## 1 — The water level puts most bodies under the ground

Measured twice, by two instruments: over sixteen regions **all nine** outlines report the level below the
ground at their own centroid, worst **6.5 m**; over 43 rings around the demo, **34 of 42** with a centroid
inside, median +0.32 m. The mechanism is visible and has two parts. `floor(0.05·(n−1)) = 0` for any ring
under **22 points**, so for a farm pond the "fifth percentile" **is the minimum** — 28 of 43 rings here —
and the minimum of *n* samples of a DEM with σ ≈ 2–4 m in this relief is biased low by ≈1.5σ at n = 9. And
the ring is the **bank**, not the shore of a level body: worst-case spread across one ring is 11.65 m.

The model is exact where it was calibrated — the four Weser polygons at 152…187 points answer 0.00 — and
wrong everywhere small.

`Tessellate` emits the surface at `level + 0.15 m`, so for those bodies the water plane lies **under the
drawn terrain**: a fringe at the median, fully buried at the tail. Water is the strongest tonal element in
a landscape at the comparison rung; a specular sheet reads at any range.

The published name for the answer is **hydro-flattening** (USGS *Lidar Base Specification*): a lake
polygon is flattened to a constant elevation at or just below the surrounding terrain, and a **river**
polygon carries a monotone downstream gradient rather than a constant. The engine already enforces
monotone-downhill for water *lines* twelve lines above the polygon branch in the same file. Two moves:
carve the region's ground patch to the level under the outline so the two models cannot disagree —
deterministic, per region, no storage — and give a polygon a gradient where its ring's spread exceeds the
DEM's vertical noise. `core/WaterDepth.h` stays right, and `LevelBelowGround` becomes what it should be:
rare and diagnostic.

## 2 — The mid-distance crowns are not tree-shaped

The one-second killer, recorded through four steps and never worked. Five to six crowns carry a large
angular **zigzag / bow-tie** silhouette with right-angle corners — a two-quad cross seen near edge-on, or
a card whose alpha cut leaves the waist. Step 6 gave it a falsifiable prediction: until then every yaw lay
in [0, 0.088°], so the forest was one unrotated clone; with uniform yaw a cross presents its edge-on
aspect in a **coherent bearing band**. If the band survives, the cause is the impostor cell seam and not
the cross. **One moving capture answers it**, and `demo/ring-pop` already writes every frame.

Beside it, the same subject: **seen from directly above all 15 995 stands vanish** — camera-facing cards
seen edge-on, and a world sandbox has a bird's eye. And a **bright untextured kite behind a near crown**,
reading as a hole in the sky: does it rotate with the camera (an impostor card sampling an empty atlas
cell) or stay welded to the stand (one stand submitted at mesh rank *and* impostor rank in one frame)?
The two separate on a capture that exists. Right in either case: a stand appears in exactly one rank per
frame, and an impostor cell that has no bake is never sampled.

What the references do: the cross never survives to the range where its own geometry is legible —
SpeedTree practice, and Guerrilla's *Horizon* vegetation hands that band to an impostor first.

## 3 — The near crown is not one mass

A beech stand carries LAI 4.5–5.1 m²/m², and litter collection over eleven temperate deciduous stands
spans 1.7–7.5. At any value in that range a crown occludes essentially all sky through its own depth and
must read as **one mass with a lit top and a shadowed underside**. Ours reads as separated dark flakes on
bare twigs, which means card coverage an order of magnitude under the species' own leaf area. Density and
self-shadowing, not detail.

`subject-meadow` writes 57 frames of bare substrate — `SubjectBench::Select` sets `Bucket_` and
`Kind_ = Herb`, and `Bucket_` is never read again. There is no herb geometry path in the bench.
`subject-beech` fills 17.2 %, so the rig is sound and the subject is missing.

## 4 — `Sim::Features()` scans everything decoded so far, per region, on the render thread

Measured from the tile server: the demo's 25-tile neighbourhood holds 4 918 footprints and 24 174 ring
points; **one** z14 tile over Berlin Mitte holds 1 115 and 10 527. The whole demo accumulation is 2.3× one
central-Berlin tile, `Prints_`/`Surfaces_` are never pruned, and the scan is already the same order as the
16 641-read lattice that step 9 just made 21× faster — and unlike it, unbounded.

Refuted by that step's own insight: **one region is one tile**, `OsmField::Feature::Tile` already groups
features contiguously and they are appended in tile order, so a `tile → [from, to)` range exists by
construction and a region's features are a **slice, not a scan**. Roughly the same ten lines. Do it before
infrastructure, because streets, sites and street polygons go through the same scan and multiply it.

## 5 — Nothing evicts, against a heap fixed at 296 MiB

`BuildingField`'s prints and verts, `WaterField`'s surfaces, courses and levels, and `OsmField` grow
monotonically for the length of a walk, and no eviction path exists. `architecture.md` is explicit — the
streamer needs a byte budget and evicts against it, and every pool reports its bytes or it is a leak with
a name. They report; nobody acts. **A fixed heap plus monotone growth is a maximum walk length**, and a
world sandbox has none. The number is one long `demo/ring` run with a column that already exists.

## 6 — Infrastructure, and the night

`/t/lights` and its 587-line producer are gone with a client half that had no caller. There is no light
list, no emissive path in the pass enumeration, no placement generator. OSM street lamps are genuine
vector data under principle 6 and all three references have a night. Owed twice now, and the largest
missing capability in the tree.

`osmDefault` is one global `"meadow"`, so unmapped land anywhere on Earth grows 700 blades/m² — Death
Valley's floor is a sward and there is no arid template among the thirteen. A per-place default belongs to
the vegetation generator, which does not exist yet: the forest is the only one.

## 7 — The crossing's remaining 2.4 ms is unexamined, not unattributed

A crossing costs +6.51 ms at p50 over its own neighbourhood; `collectMs` is flat and `populateMs` is 0.055
there. The profile carries seven more columns that were never checked, and **`buildingMs` is the obvious
candidate**: `World.cpp` puts the vector build, the water ingest, the building build **and** the building
DAG in that one span, with the DAG recorded at 33.0 ms of a 50.9 ms frame for one dense tile. Redo the
neighbourhood excess over **every** column — one run, no code — and publish `frameMs − Σ(spans)` as its
own column so "unattributed" is a measured quantity instead of a subtraction done by hand.

**And the pool's byte step is still a localisation, not a diagnosis** after two rounds of paying for it:
p50 +1.09, p99 +7.54 ms between 23.2 and 28.0 MiB, one binary, capacity the only variable. Not cache, not
extra work. What remains is an allocator size class or heap pressure changing tile eviction, and the
`evicted` column already exists.

## 8 — Interfaces that write down what they claim to carry

`RegionPool::Extent::Reached` is **never read** — the header says the body budget follows it while the
count is computed by the caller, so it is an argument the constructor ignores · `FbGroundBlock::Nodes_`
is a raw pointer into an LRU slot valid until the next call into the oracle, enforced by a comment; a
generation counter on the slot makes it a refusal instead of a corruption · the oracle's
single-threadedness is now on the header and is still a sentence, not a type · `Buildings::Over` discards
a `[[nodiscard]]` in three places because `Passes()` already guarantees the top, so the invariant is split
across two functions and bridged by a cast · the OSM layer names are spelled in three files ·
`DrawSink::Add` returns a `bool` with `Full()` beside it and `ForestDraw.cpp:18` truncates a region
silently, though the collection's refusal is loud · **the winding is hard-coded at seven sites** and now
gates the first generator draw product: cull mode is pipeline state fixed at `Configure`, so the shape is
a declaration at registration beside the rank, and `Configure` runs after the units are known.

## 9 — Two bases for one building, and a budget that escapes itself

`FeatureTop = BaseM + HeightM` takes `BaseM` from the ring's lowest corner while `Buildings::At` derives
its base from the patch at the bbox centre, so on a slope the queried prism floats relative to the drawn
one and the reported height is short by the ground difference across the footprint — ≈1.5 m at 10 % over a
15 m house. `BaseM` is already computed; carry it. · `Sim::Settle` sets `RoofChecked_` inside the resolved
branch, so before the ground lands every pass pays a full `GroundAt` — outside `populateMs` and outside
the one-per-turn budget. · `verify-types`' negative gate passes on **any** compile error; one `grep -q`
clause fixes the class for all three gates. · Comment density on the newest files is 25–50 % prose against
a rule that says one line of local why.

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
