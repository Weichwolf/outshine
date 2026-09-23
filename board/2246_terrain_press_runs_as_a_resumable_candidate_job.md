Type: defect
State: active
Architecture: ready
Parent: 2234
Depends:
Priority: P0
Area: generators, terrain, engine
Tags: earthworks, realtime, determinism

# Terrain pressing runs as a resumable candidate job

## Defect and measured workload

The original one-shot `PressGroundEarthworks` processed 2887 Malcesine sheets,
5788 building pads and 16942 corridor pieces. It spent 120.646 ms gathering
node coordinates/heights, 0.654 ms bucketing stamps, 101.245 ms globally
rejecting excessive cuts/fills, 90.761 ms applying accepted stamps and
107.002 ms converting/writing heights; floor diagnostics took 0.527 ms.
These are one candidate's phase samples, not maxima or a platform budget.
The shot's p95 was 673.56 ms with 37/153 frames above 16.67 ms. The staged
8192-node run measured 2406 frames, p50/p95/p99 2.08/4.28/617.75 ms, 36
over 16.67 ms, 13.528 ms longest earthwork slice and 852 MB heap peak. Other
candidate phases still dominate the slow frames; this WI does not waive them.

Wien later showed 103,293 building pads assembled synchronously before the
press job, with a 27.672-ms earthwork slice. `BuildingStampJob` now owns pad
construction in 2,048-unit slices, including per-ring and seam cursors; it
checks vector generation and source dimensions without retaining OSM spans.
The 2026-09-23 Wien shot remains `2fc0aec4`: p99 9.81 ms, 8/4464 late
frames, longest earthwork slice 6.450 ms and whole phase 15.807 ms. Malcesine
remains `07ca3a25`: p99 10.25 ms, 0/1373 late, earthwork slice 2.154 ms.
The remaining start cost is lake-stamp construction and corridor-yield merge;
measure and split them before claiming a strict per-unit budget.

## Decision

Make terrain pressing a candidate-owned generator job, with resumable phases:
gather sheets in stable order; build the spatial stamp index; reject excessive
stamps over every node; only then apply accepted stamps over every node;
convert changed nodes back to geodetic sheet heights; calculate pad/corridor
diagnostics and finish. Never publish or classify a partly pressed patchwork.
Keep the source yields and patchwork alive with the candidate; cancellation
discards the job and leaves the published world intact. A revision mismatch
starts a new job, never splices work from the old source snapshot.

The rejection pass is global: a stamp discovered invalid at a late node must
be excluded at every earlier node in the apply pass. Preserve sheet/row/column
order, bucket ordering, `Pressed::Inside` claim ordering, `DecidedBy` indices,
seam handling and all geometry/diagnostic values. Preserve the existing
one-shot `PressTerrain` as an independent oracle until the staged path proves
equivalent; do not implement two diverging terrain algorithms. Use shared
per-node operations and explicit phase cursors. Keep GPU and renderer types
outside the generator.

## Implementation and acceptance

1. Put owned yields, positions, original/working heights, sheet-node sources,
   stamp index/rejection flags, decisions, claim list and phase cursors in one
   move-only job. Derive its peak bytes from owned capacities; release it after
   completion or cancellation. No borrowed span may outlive its candidate.
2. Bound gathering by sheet and node passes by a measured node count or time
   budget. The Malcesine samples suggest work units well below 16.67 ms, but
   verify their p50/p95/p99 and worst frame rather than treating the estimates
   as guarantees. Keep preparation and finalization bounded too. Extend the
   client's fully measured Refined-capture window; its former cap was 3072
   frames. A temporary stage probe found Koerbersee still building earthworks
   after 2560 checks; 6144 fully measured frames let it publish at frame 4475.
   Feldkirch published at frame 2685 in the same run. The temporary probe also
   saw `terrain coverage missing` until publication; this is not evidence of a
   missing download. Preserve the observed frame counts and diagnose any future
   cap failure by candidate stage and readiness blocker before raising it again.
   Never hide work after timing.
3. Compare one-shot and staged results for byte-identical sheet nodes, moved/
   held/refused counts, deepest/raised cut, pad/corridor floor diagnostics and
   ordered claims/decisions. Include overlapping pad/basin/corridor stamps,
   a late globally rejected stamp, empty/invalid sheets and differing slice
   sizes. Interruption must not change the result.
4. Run paced/preload equivalence with and without NDEBUG, Floor/Lattice
   integrations, Malcesine capture and visual comparison. The mixed-sheet,
   late-rejection and A→B→C projection-change tests cover native equivalence,
   interruption and a stale revision during pressing. Readiness must reject
   the old projection until C publishes; only A and C may publish. Retain the
   separate late-failure publication test. Report full frame distributions and
   CPU/GPU memory peaks. Capture pixel variance with identical terrain-sheet
   digests belongs to WI 2230. `make format` and `make lint` must pass. Close
   only when no earthwork unit monopolizes a frame.
