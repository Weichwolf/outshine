Type: feature
State: active
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
