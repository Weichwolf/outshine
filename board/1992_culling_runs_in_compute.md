Type: feature
State: open
Parent: 1995
Area: render
Tags: benchmark, target, gpu-driven

# Culling runs in compute, and the CPU issues no work per cluster

**Benchmark** — Unreal: Nanite culls instances and then clusters in compute, using a two-phase HiZ occlusion test — draw last frame's visible set, build HiZ from it, then test everything against that depth. RAGE: visibility is resolved before the per-item CPU work, into a list the device consumes. **Taking Unreal**, whose two-phase scheme is the part worth copying because it needs no CPU readback and therefore no stall. **Reference**: Haar & Aaltonen, *GPU-Driven Rendering Pipelines*, SIGGRAPH 2015 — two-phase occlusion culling with a hierarchical depth buffer, and why testing against last frame's depth is both cheap and conservative enough.

## What

Frustum, occlusion and LOD selection are decided on the GPU, per cluster, in a compute stage. The
CPU dispatches that stage and nothing else.

## Why

A CPU that decides visibility per object holds a term that grows with the scene. That is the
invariant board:1943 states, and it is the reason both benchmarks moved this work.

The DAG already carries what the decision needs — a bounding sphere and an error bound per
cluster (board:1991) — and the instance buffer is where the decision writes its answer
(board:1989). This item is the stage between them; without either it has nothing to read and
nowhere to write, which is why it depends on both rather than being attempted first.

**Occlusion is the half that pays for itself in a city.** A frustum test rejects what is behind
you; an occlusion test rejects the building behind the building, which is most of a street.

## How

1. a compute stage reads the instance buffer and the DAG, writes a visible-cluster list
2. the HiZ pyramid is built from last frame's depth — no readback, no stall
3. two phases: draw what was visible last frame, rebuild HiZ from that, then test the rest
   against it. Conservative because last frame's depth is never deeper than this frame's for
   anything that did not move toward the camera

- [ ] a compute stage decides per-cluster visibility and writes the surviving set
- [ ] the HiZ pyramid comes from last frame's depth, and no stage reads GPU memory back to the CPU
- [ ] the CPU issues a CONSTANT number of dispatches over a drive, whatever the scene holds
- [ ] a scene with an occluder draws fewer clusters than the same scene without it -- the number
      that shows the occlusion half works at all

## MEASURED BEFORE A LINE WAS WRITTEN, and the item is smaller and sharper than it was filed

**The culling is already written and it already selects per cluster. It runs on the CPU.**
`src/compositor/GroundPatchwork.cpp:106` walks every cluster of every tile in the ring, calls
`DagSelect(cluster, eyeInTile, FocalPx, Tau, Up)` on each, and pushes the surviving indices into a
CPU vector that is then uploaded. That is exactly the predicate below, on the wrong processor:

    for (const DagCluster &cluster : built.Clusters) {
      if (!DagSelect(cluster, eyeInTile, over.FocalPx, over.Tau, over.Up)) { continue; }
      ++out.ClustersDrawn;
      for (uint32_t step = 0; step < cluster.Count; ++step) {
        out.Index.push_back(first + built.Idx[cluster.First + step]);
      }
    }

So this item is not "write a culling algorithm". board:1991 wrote and proved it. It is: **the
clusters reach the DEVICE, a compute stage runs the selection the CPU runs now, and the CPU stops
walking them.** That is a much narrower change and it has a reference implementation one directory
over -- its own.

**Three things measured, each of which moves this item:**

1. **`src/render/` names neither `ClusterDag` nor `DagCluster`, anywhere.** The cooked form stops
   at the compositor; only the SELECTED indices cross to the device. A compute stage cannot cull
   what it cannot see, so the first predicate is a residency question before it is a compute one.
2. **The counters exist and nothing outside the compositor reads them.** `Out::ClustersHeld` and
   `Out::ClustersDrawn` are computed every frame and no engine code, no door verb and no case
   reads either. The before/after number this item owes is already being calculated and thrown
   away -- CLAUDE.md's commonest defect, one more time.
3. **Only the GROUND is culled per cluster.** The subject path has no cluster selection at all, so
   "the CPU issues no work per cluster" is trivially true for subjects and false for terrain, and
   the predicate has to say which it means.

**What the next step is, and it is not the compute stage.** Publish `ClustersHeld` and
`ClustersDrawn` through the door so this item has its before-number, then move the clusters to the
device, then dispatch. Building the stage first would leave the win unmeasurable, which is what
board:1993's `apps/bench` reading exists to prevent.

## The before-number, and the CPU is already doing the work well

`Engine::State::Composes` publishes them now and `apps/bench` prints them. Over the drive:

    clusters the ring holds     342
    clusters it drew             83

**So the selection already discards three quarters of the clusters** -- the DAG's cut is working
and the win a compute stage buys is not "fewer clusters drawn", it is "the CPU stops walking 342
of them per frame". That is a different claim from the one this item's title suggests, and it is
the honest one: correctness is not at stake, the CPU term is.

Which sharpens the fourth predicate. `a scene with an occluder draws fewer clusters than the same
scene without it` is about OCCLUSION and the selection above is about ERROR -- a cluster is cut
because its screen-space error is small, not because something stands in front of it. There is no
occlusion culling in this tree at all today, so that predicate is a second feature wearing the
same item's number, and the HiZ predicate beside it is its real name.

## RE-MEASURED, and the item was asking about the wrong geometry

**THE ENGINE MAY NOT KNOW "TERRAIN".** board:1995 already decided it -- *terrain is as much a subject
as car* -- and a first pass at this item wrote a terrain exception anyway, on the strength of
`CookTile`'s comment that a tile needs no DAG because the pyramid is already the LOD. That argument
is TRUE and it is the PRODUCER's to make, never the engine's to know. Geometry in, picture out.

**AND THE TERRAIN MESH IS THE SMALL PART.** What the cluster count was measuring is a hundredth of
what stands on a block:

    place        terrain clusters   streets   footprints   instances   sitting on them
    Rothenburg               100      2 159        5 499       3 242          10 900
    Shibuya                   88     12 480       69 014       2 078          83 572

Four places read 100/100, 100/100, 100/100 and 88/88 clusters held and drawn -- one per tile, every
one kept. From that a first pass concluded "there is nothing to cull", which is true of the GROUND
and false of the frame: 83 572 objects at Shibuya never enter the cluster path at all. The buildings
arrive as ONE part of 812 079 triangles, the streets as another, the water as a third.

**So the item's question was aimed at the hundredth that does not matter.** That is why the numbers
looked like a dead end and were not one.

## Does everything go through the DAG, and may something opt out

**Benchmark** — Unreal: `bEnableNanite` is a per-ASSET opt-in decided at build; a mesh that does not
take it carries an AUTHORED LOD chain and the base pass draws it. RAGE: every drawable has an LOD
chain and small props share `slod` tiers; the chain is authored rather than derived. **They agree on
the shape and it is the answer to the question**: the CUT is per-GEOMETRY and DECLARED, never
per-category and known by the engine.

**Taking that**, which gives the form:

    Cut::Built   the engine builds a cluster DAG. The default, and Nanite's opt-in read the other
                 way round -- a producer that says nothing gets one
    Cut::Given   the producer supplies the levels and their errors. A terrain tile says this and its
                 reason is exactly `CookTile`'s: zoom z-1 IS the simplification of zoom z, made once
                 by whoever made the tiles. Cesium's quantized-mesh is literally this

One selection then reads one cluster table whose rows came from either source.

## TWO DECISIONS, IN ORDER, AND THEY ARE NOT THE SAME ONE

    generators (terrain, osm, vegetation)  ->  DAG  ->  render
    deliver geometry BY DISTANCE               by SCREEN ERROR

**A paragraph here proposed merging them and it was wrong.** It said a tile's cluster should carry
its PARENT TILE's error instead of `kDagRootErr`, so that one cut would choose between zoom levels as
well as between DAG levels and the cascade would stop being a second mechanism. That pulls a
PRODUCER's decision below the seam -- the same category error as letting the engine know "terrain",
mirrored.

They answer different questions and neither can do the other's:

  **DISTANCE decides what EXISTS.** Which zoom of tile is streamed, whether a footprint is full
  architecture or a prism or part of a block mass, whether a plant is a mesh or a card. It bounds
  the work before any geometry is built, and it is an IO and memory question. Over a 240 km ring
  nothing else can bound it: streaming the finest tile everywhere so the DAG can cut it is the one
  answer that is certainly unaffordable.

  **SCREEN ERROR decides what is DRAWN.** Given the geometry that arrived, which clusters of it
  reach the raster. It is a bandwidth and fill question and it cannot reach back and un-stream
  anything.

So the cascade is stage one and `DagSelect` is stage two, and board:1995's "one route" means one
route from `Geometry` to pixel -- not one decision. The distance tier sits ABOVE the seam with the
generators, exactly where the parent's own diagram puts it.

## What is measured and stands

**`DagSelect` is called in exactly ONE file**, `GroundPatchwork.cpp`. The subject path calls it
nowhere.

**`outshine::Cook` is reached by NOTHING.** board:1991's landed cooker -- one cooker, one cooked
form, the cut chosen per cluster -- has two callers in the whole tree and both are cases:
`geo/ScoreWhatACutCostsASubject` and `geo/ScoreWhatOneCookerDoesToASubject`. The subject path
flattens geometry into a `SubjectScratch` per frame and uploads it whole. That is this tree's named
commonest defect standing between board:1991 and this item.

The item's own "342 held, 83 drawn" predates the runtime DAG's removal from `CookTile` and is struck.

- [ ] a `Geometry` DECLARES where its cut comes from, `Built` or `Given`, and the engine names no
      category

## THE TEST, in the owner's own words: a terrain tile is a very large OSM roof

That is the whole predicate and it is falsifiable by grep. If a tile and a roof are the same thing to
the code below the `Geometry` seam, there is no terrain concept left; if any path names them apart,
there is one.

MEASURED, and they are already closer than the argument suggested. BOTH end up as parts of one
`Geometry`:

    src/engine/Picturing.cpp:571   ground.addPart("ground", ringSurface)
    src/engine/Picturing.cpp:675   ground.addPart("roofs",  roofSurface)

Two calls, one function, no branch. So the seam is honoured where the geometry crosses it. **What
differs is on the other side of it**: a tile arrives carrying `std::vector<DagCluster>` --
`TileMeshes.h:16` and `Patchwork` -- which the compositor consumes and throws away, and a roof
carries none at all. Neither is culled per cluster in the end, so today the asymmetry costs nothing;
it is the shape that is wrong, and it is the shape this item has to fix.

A roof is a small geometry with no cut and a tile is a large one whose cut dies in the compositor.
When both take `Cut::Built` or `Cut::Given` and one selection reads one table, the sentence above
stops being a target and becomes a description.
- [ ] the engine COOKS what says `Built` -- `Cook` reaches the subject path, which is board:1991's
      work becoming reachable rather than new work. **BLOCKED ON board:1949, and the reason is
      exact**: `Cook` takes an `outshine::Geometry` and the whole subject path carries a
      `Gltf::Subject`. `Live::Shown()` returns one and `Held_.Geometry()` -- a method whose NAME says
      Geometry -- returns one too. So the cooker is not merely unwired, it takes a value that never
      appears on the path it would have to cook. Giving it a second overload would be the second
      entrance board:1995 forbids, so the two values have to become one first
- [ ] the generators deliver BY DISTANCE and the DAG cuts BY SCREEN ERROR, in that order, and
      neither is asked to do the other's job
- [ ] the OSM structures and the vegetation go through it too: 10 900 objects at Rothenburg and
      83 572 at Shibuya, against 100 and 88 terrain clusters
- [ ] `DagSelect` runs on all of it on the CPU first, so the before-number belongs to the thing being
      moved
- [ ] only then the compute stage
