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

**And the second spelling is the real defect.** `TilePool` builds `TerrainMesh` and its own DAG
inside the streaming path, so the tree has TWO routes from data to drawable geometry: one for
terrain and one for everything else. CLAUDE.md allows two FORMS -- authored and cooked -- and no
third, and two cookers is a third by another name.

## How

The cooker that builds a DAG for a terrain tile runs over a subject's cooked geometry. CLAUDE.md
already names the two forms and where the DAG belongs: *the COOKED form is one-width GPU streams
plus the CLUSTER DAG that carries LOD and culling*. So this is the cooker reaching the second
producer, not a second cooker.

- [x] the renderer names no subject: `Stage::Terrain`, `Buildings`, `Water` and `Models` are
      gone from `RenderCatalogue.h`. They declared resource edges and executed nothing, so they
      were a declaration surface with a subject's name on it.
      proof: --audit-layers and the door suite, unchanged by their removal
- [ ] **a cut is SELECTED and read.** `DagSse` gains a caller and the number of clusters a frame
      draws depends on where the camera stands. This comes FIRST, before either producer is
      re-routed: a mechanism nothing evaluates cannot be proven correct by giving it a second
      input, and the cheap half is what makes the expensive half worth having.
- [ ] **A DEFORMING SUBJECT KEEPS THE PLAIN STREAMS, and that is a decision this item owes.**
      `ClusterDagBuild` rewrites both the vertex and the index buffer -- it SIMPLIFIES -- so a
      subject whose vertices are re-posed every frame would need its DAG rebuilt every frame,
      which is a per-frame cost proportional to the mesh. Unreal shipped Nanite for static meshes
      first for this reason. So the cooked form carries a DAG when the geometry does not deform,
      and the tier chain is unchanged: one cooker, one cooked form, and the DAG is a part of it
      that a deforming mesh leaves empty.
- [ ] terrain reaches the picture as a GENERATOR's `Geometry`, cooked by the one cooker -- not as
      a mesh the tile pool builds on its own
- [ ] a subject at distance draws fewer triangles than the same subject up close -- measured over
      a declared camera move, not asserted
- [ ] the cut is per CLUSTER: two subjects at different distances in one frame select different
      levels, which a per-object ladder cannot do
