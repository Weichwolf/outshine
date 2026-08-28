Type: feature
State: active
Parent: 1995
Area: render
Tags: benchmark, target, gpu-driven

# There is ONE cooker and one cooked form, and terrain goes through it like anything else

**Benchmark** — Unreal: Nanite cooks every mesh into a DAG of ~128-triangle clusters, each carrying its own error bound and its parent's, so a cut through the DAG is a valid LOD and the cut is chosen per cluster rather than per object. RAGE: LOD models per map entity, chosen per entity. **Taking Unreal** — a per-OBJECT ladder cannot spend detail where the camera is, and a car half off-screen pays full price for the half nobody sees. **Reference**: Karis, *Nanite: A Deep Dive*, SIGGRAPH 2021 Advances in Real-Time Rendering — the error-bounded DAG and why a monotonic error is what makes the cut valid.

## What

One cooker turns an authored `Geometry` into the cooked form -- one-width GPU streams plus a
cluster DAG -- and everything drawn goes through it. **The engine knows no terrain**: a generator
hands back a `Geometry` and what it depicts is the generator's business.

## Thinking the pipeline backwards, which is what settles it

    720p60  <- present <- tonemap <- SceneHdr
            <- ONE subject pass: GPU streams + placements + visibility
            <- ONE cooked form: one-width streams + cluster DAG
            <- ONE cooker
            <- Geometry, from a reader, a GENERATOR, or a client
            <- a FIELD (height, class, water) for the generator to read
            <- tiles

Terrain enters at the same place a tree does: a generator reads the field and fills a `Geometry`.
Nothing below the cooker knows which generator made it, and nothing above it should.

**Unreal draws Landscape as a `UPrimitiveComponent` in the base pass like any other primitive;
RAGE's terrain is a map entity on the same draw list as everything else.** Neither has a terrain
PASS. This tree had four -- `Stage::Terrain`, `Stage::Buildings`, `Stage::Water`, `Stage::Models`
-- declared in `RenderCatalogue.h` with resource edges and no executor at all. They are deleted:
a subject noun inside the renderer is a finding wherever it stands, and one that also executes
nothing is two findings.

## Why

**The hard part is built and unreachable, and it is MORE unreachable than this item first said.**
Measured: `DagSse`, `DagEdgeSq` and `DagCrossFactor` -- the three functions that turn an error
bound into a decision -- have **ZERO callers** anywhere in `src/`, `apps/` or `test/`. Outside
`TilePool.cpp`, which builds them, nothing reads `Clusters` at all except the byte accounting.
So the DAG is built for every terrain tile, stored, measured for memory, and **never used to
select anything**. This tree's Nanite is not "terrain has it, subjects do not" -- it is a
mechanism that runs its expensive half and skips its cheap one. CLAUDE.md calls the cluster DAG
*"this tree's Nanite, and essential to the frame path"*; today it is essential to nothing.

`src/base/spatial/ClusterDag.h` holds exactly Nanite's shape:

    struct DagCluster { First, Count; SelfCenter[3], SelfRadius, SelfErr;
                        ParentCenter[3], ParentRadius, ParentErr; Level; };

Self and parent error bounds are what let a cut be chosen per cluster without cracks — the
property Karis spends most of the talk on. `grep -rln ClusterDag src/*.cpp` finds ONE consumer:
`TilePool`. Terrain has it; subjects do not.

Without it, culling in compute (board:1992) has nothing to cull at a useful granularity: it can
reject a whole subject or keep it, which is what a CPU frustum test already does.

**"The second spelling" was WRONG IN DETAIL and the measurement says how.** `TileDagBuild` does
not build a second DAG -- it CALLS `ClusterDagBuild`, adding two things a tile needs: the local
up-vector, derived from the tile's own ECEF origin, and the SKIRT appended as a cluster with zero
error and a root parent, so a skirt is never simplified away. That is an adapter, not a cooker,
and the tier chain is intact.

What was actually there was a DEAD DUPLICATE DOOR. `TilePool::Dag(id, soup, nverts, seamAttr,
out)` posted a `Rank::Dag` job that `RunDag` served with its own copy of the same build-plus-
fallback, and **no caller anywhere in `src/`, `apps/` or `test/`**. Its one distinguishing feature
was a seam class -- `kSeam[8]` of `SeamAt<A>`, classifying a vertex by whether attribute A is
negative -- and nothing in the tree ever writes that sentinel, so the mechanism had no caller AND
no input. Deleted: the door, the job rank, `RunDag`, `DagKey`, `Job::Soup`, `Job::SeamAttr`, the
seam pickers and their two ledger counters. `ClusterDagOpts::ClassOf` stays, because a caller with
a real class function is a thing that can exist; eight pickers over a convention nobody writes
are not.

## How

The cooker that builds a DAG for a terrain tile runs over a subject's cooked geometry. CLAUDE.md
already names the two forms and where the DAG belongs: *the COOKED form is one-width GPU streams
plus the CLUSTER DAG that carries LOD and culling*. So this is the cooker reaching the second
producer, not a second cooker.

- [x] the renderer names no subject: `Stage::Terrain`, `Buildings`, `Water` and `Models` are
      gone from `RenderCatalogue.h`. They declared resource edges and executed nothing, so they
      were a declaration surface with a subject's name on it.
      proof: --audit-layers and the door suite, unchanged by their removal
- [x] **a cut is SELECTED and read.** `LayPatchwork` walks the tile's clusters and copies only
      the indices of those `DagSelect` keeps; `Around` carries the eye, the focal length in
      pixels and the threshold, and `Picturing.cpp` fills the focal from the frame height and the
      engine's own 55-degree field. `Patchwork` reports `ClustersHeld` and `ClustersDrawn`, so
      the selection is a number rather than an effect.
      proof: outshine/geo/ScoreWhereACutIsChosen -- one tile of three clusters draws 2 of them
      (4 triangles) from 12 m, 1 of them (2 triangles) from 4000 m, and 3 of them (6 triangles)
      with no focal length at all.
      negative control: `DagSelect` forced to `true` makes the far eye draw 3 clusters and the
      case goes RED on the comparison that matters.
- [x] **the cooker takes a subject's own attribute set, and the premise that it could not was
      WRONG.** This item was about to be worked on the assumption that `ClusterDagBuild` is fixed
      at the terrain's eight-float soup and therefore cannot cook a subject carrying up to seven
      streams. Measured: it takes an arbitrary `stride >= 3`, reads the first three floats as
      POSITION and welds the rest byte-wise as opaque payload, so uv, normal, tangent and colour
      ride through untouched. Nothing needs widening; what a subject needs is its streams
      INTERLEAVED, which is the inverse of what `Generators::Meshed` already does.
      proof: outshine/geo/ScoreWhatOneCookerDoesToASubject cooks 8192 triangles at stride 11 into
      120 clusters over 3 levels; the finest level covers all 8192 and the errors run 0.0285 to
      0.200, so a parent is coarser than its children and `DagSelect`'s pair decides something.
      negative control: one cluster per mesh makes it read `1 cluster(s) over 1 level(s)` and the
      case goes RED -- a cooker that returns one cluster has not clustered.
- [x] **the cooker takes the DOOR's value, not a hand-interleaved soup.**
      `src/base/spatial/Cooked.h` reads a part through `PositionsOf`, `NormalsOf`, `TextureOf`,
      `TangentsOf` and `ColoursOf` -- all already on `include/Geometry.h`, which is the seventh
      capability this session nearly duplicated before checking -- and DERIVES the stride from
      what the part carries. `ClusterDag::Stride` records it, so the cooked form knows its own
      width. A generator, a reader or a foreign program fills one `Geometry` and the cooked form
      comes from that, with nothing in the middle.
      proof: the same case cooks a `Geometry` of 8192 triangles at stride 8 into the same 120
      clusters, finest covering all 8192 -- identical to the hand-interleaved soup.
      negative control: dropping the normal stream makes it read stride 5 and the case goes RED,
      so the width follows the part rather than a constant.
- [x] **A DEFORMING SUBJECT KEEPS THE PLAIN STREAMS, and the guard cannot live in the cooker.**
      The decision stands: `ClusterDagBuild` rewrites both the vertex and the index buffer -- it
      SIMPLIFIES -- so a subject re-posed every frame would need its DAG rebuilt every frame, a
      per-frame cost proportional to the mesh. Unreal shipped Nanite for static meshes first for
      exactly this. The cooked form carries a DAG when the geometry does not deform, and a
      deforming mesh leaves that part empty.
      **Measured while trying to put the refusal into `Cook`**: `Geometry` cannot answer the
      question. It is the snapshot of ONE pose -- the previous positions live in `Gltf::Subject`,
      not in the door's value -- so no `Cookable(Geometry, part)` can exist, and one written
      anyway would have been a guard that always says yes. What knows is `Core::Asset::Moves()`,
      which reads the document's animations, so the refusal belongs where the cooking is asked
      for and not where it is done. That is a fact about the door worth having: the authored form
      describes a SHAPE and never a motion, which is the same reason board:1995 gives for a
      `Geometry` carrying no velocity.
- [ ] terrain reaches the picture as a GENERATOR's `Geometry` rather than as a mesh the tile pool
      builds on its own -- the remaining half, and now a question about the PRODUCER rather than
      about the cooker
- [x] a subject at distance draws fewer triangles than the same subject up close, MEASURED before
      any frame-path change: a 32768-triangle subject cooked by the one cooker draws all 32768
      from 20 m and 21054 from 6000 m -- 64.3%.
      proof: outshine/geo/ScoreWhatACutCostsASubject
      negative control: the near cut must equal the whole mesh, and a cut that selects nothing
      fails its own check first.
- [x] **the simplifier reaches its declared ratio, and one number was the whole of it.**
      `dag::SimplifyGroup` LOCKS every vertex a group shares with another -- Nanite's boundary
      lock, without which neighbouring groups crack at the seam -- so with few clusters per group
      the locked boundary is large against the interior and only interior edges collapse.
      `GroupSize` was **4** where Nanite groups 8 to 32. Swept:

          groups of  4  reach 0.69      groups of 16  reach 0.50
          groups of  8  reach 0.61      groups of 32  reach 0.50

      Sixteen is the smallest that reaches the declared ratio; a larger group costs more locked
      work per level for nothing. The levels now hold **32768 -> 16384 -> 10156** where they held
      32768 -> 22626 -> 21054, and the far cut draws 31.0% of the near one where it drew 64.3%.
      The mechanism saves 69% of the mesh where it saved 36%.
      proof: outshine/geo/ScoreWhatACutCostsASubject, and the sweep stays in the case because it
      is the evidence the number is right rather than a guess.
      negative control: the ratio check stood RED-WHEN-FIXED and went red the day it was fixed,
      which is what such a check is for; it now asserts the ratio IS reached.
- [x] the cut is per CLUSTER, and the sharp form of that is stronger than two subjects: ONE eye
      over ONE subject selects clusters from more than one level. With the eye at a corner of a
      32768-triangle lattice it takes **112 clusters at level 0 and 72 at level 1** -- fine where
      the mesh is near and coarse where it runs away, which a per-object ladder cannot produce at
      all.
      proof: outshine/geo/ScoreWhatACutCostsASubject
      negative control: moving that eye to 1000 km makes the cut take one level only -- 83
      clusters at level 2 -- and the case goes RED, so the check measures the MIXTURE and not the
      cut.
