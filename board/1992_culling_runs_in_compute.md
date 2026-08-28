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
