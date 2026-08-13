Type: feature
Area: render
Tags: instrument
Depends: 0111

**What § I.27 loses: ten content stages become one, and the criterion is the field the catalogue already carries**

*The claim to be checked was that the stage catalogue's guarantees live on the resource graph and not on
stage identity, so the catalogue should enumerate `{compute, fullscreen, geometry} × resource edges`
rather than twenty proper nouns. **It is half right and the other half would cost three properties**,
so the cut below is narrower than the claim and is a real cut.*

- [ ] **What is confirmed:** `TopologicalOrderHolds` (`render/plan/RenderCatalogue.h:314-327`) proves a property of the **read/write edges** and uses the enumeration only as the witness ordering. Five of the six `static_assert`s are graph properties and survive any renaming; only *every row at its own index* is about the enumeration
- [ ] **What the full claim would cost, and it is why it is refused:** with no named row, `RenderPlan::StageByName` has nothing to resolve — a declaration would name a row index or a tuple, which is the **string-keyed resource the catalogue's first line explicitly refuses**; and `Renderer::Executable(Stage)`, the exhaustive switch with no `default:` that makes a new catalogue row **a compile error until the device layer answers for it**, loses its subject
- [ ] **The cut is along `Provenance`, which is already a field of the row** (`render/plan/RenderCatalogue.h:72-75`) — and that the criterion deciding the collapse is the field the catalogue already carries is the evidence that the split was drawn in the right place the first time
- [ ] **The ten `Content` raster rows collapse to ONE `Geometry` row.** `Sky · Sun · Moon · Stars · BenchGround · Terrain · Buildings · Water · Models · Subjects` all read the same set, contribute to the same three attachments and differ only in **vertex layout, shader and draw-list source** — every one of which is now data: `VertexLayout` is a per-draw field (`render/draw/DrawList.h`), pipeline state is `constexpr`-derived from the material (`core/SurfaceState.h`, `StateOf`), and the draw-list source is a compositor. **`ViewLayer::{Background, World, Overlay}` (`render/draw/DrawKey.h`) already carries what `Sky` versus `Terrain` versus an overlay was expressing, and carries it above depth, which is where it belongs**
- [ ] **The `Machinery` rows keep their proper nouns and that is not an inconsistency.** `Transmittance · MultiScatter · SkyView · Irradiance · Tonemap · Present` — and the algorithmically distinct `Content` computes `AutoExposure`, `ShadowMap`, `Occlusion`, `TemporalResolve` — are **not instances of a kind**: no data turns `MultiScatter` into `SkyView`. Collapsing them replaces a name with a tuple and buys nothing
- [ ] **Twenty rows become eleven**, and § I.27's unticked *"`GeometryStage`'s five draws are five independently declarable content stages over one shared LOD cut"* is **superseded rather than deleted**: the five become zero declarable stages and one draw list, which is the same requirement reached by removing the thing instead of splitting it. *The superseding is written here so the line is not read as scope quietly dropped*
- [ ] **The instrument that appears to be lost is not.** Collapsing ten rows loses per-content GPU spans — but the five geometry units **already shared one pass** (`render/GeometryStage.h:42-44`, § I.27), so per-content GPU attribution was never obtainable without splitting the pass and paying for it. What replaces it is a **CPU-side draw-list statistic per compositor** — draws, batches, indices, merge count, all of which `DrawList::Batches()` already carries. *The caveat was sought and it clears*
- [ ] **`SubjectDraw`'s pipeline table is the same mistake one level down and dies with it.** It enumerates layout × facing × alpha mode inside the stage (`render/stages/SubjectDraw.cpp:618-681`) — thirty pipelines built into a fifty-slot array. **The pipeline key is `(VertexLayout, SurfaceState)` and `SurfaceState` is already `constexpr`-derived from `Material`**, so the set of pipelines a plan needs is the set of distinct keys its **compiled draw lists** contain. § I.10's ticked *"No pipeline creation while playing"* is what fixes when: the pipelines are created **when the plan is compiled**, from the compositors' declared key sets, and the count becomes an output of the compiler exactly as the pass count already is

**The projection slice, scoped rather than guessed.** *The renderer takes a projection matrix, never a
projection kind* is not one line. `Renderer.h:111` has `SetOrthoM(double)` and `:152` an `OrthoM` field,
and `MvpCamRel` branches on `orthoM > 0` **inside** — so the renderer holds the kind twice: once as a
field and once as a branch. **`fovDeg` is the other half of the same knowledge**, so a matrix-taking
renderer loses both, and `fovDeg` is read elsewhere for derived quantities — the shadow-ray bias has a
closed form in `cos(yfov/2)`. One caller: `src/clients/GltfStudio.cpp:66`.

**So the slice is: the caller builds the projection, the renderer multiplies view by it, and every
consumer of `fovDeg` either takes the number from the declaration it already has or is shown not to need
it.** Acceptance is the item's own — **no case moves in the picture bound**, since a projection change
that moved one would be a different picture rather than a refactor.

**Not started here.** At the depth this was scoped it would have been begun and not finished, and a
half-applied projection change is the one edit in this item that cannot be left overnight.

**Groomed after two slices landed: the remainder depends on `board:0111`, and it is not more renaming.**

**Done and landed**: five stages became mechanisms (`Medium*`, `LightVisibility`, `AmbientOcclusion`),
and `BenchGround` is gone with `board:0031`. Neither could move a picture, which is why they went first.

**What is left is structural, and two parts of it are blocked rather than merely large.**

- **The five geometry units becoming four surface classes is a reclassification, not a merge.** Merging
  `Terrain`, `Buildings`, `Models` and `Subjects` into one row would give it the **union** of their
  reads — the exact defect that made `Background` a role rather than a node. The right shape is one row
  per **surface behaviour**, with *which content is in the draw list* belonging to the compositor. **That
  needs `board:0111` to exist**, so the dependency is real and not a preference.
- **`Background` dissolving removes `Sky`, `Sun`, `Moon` and `Stars`** into emissive geometry plus the
  medium evaluated to the far plane — and **the medium chain does not execute**. Dissolving now would
  delete the only declaration surface for a sky before its replacement can draw one. It waits for the
  medium stages to be implemented, which is `board:0030`'s territory rather than this item's.
- **The projection matrix is independent of both** and is scoped above: the caller builds it, the
  renderer multiplies view by it, and every consumer of `fovDeg` is shown to take its number from a
  declaration or not to need it. **That slice can be taken whenever a round has room.**
