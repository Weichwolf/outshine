# Broken hip profile composition

Owner: [#469](https://github.com/Weichwolf/outshine/issues/469),
[PR #470](https://github.com/Weichwolf/outshine/pull/470).

[CGA Shape, SIGGRAPH 2006](../siggraph/2006-procedural-modeling-of-buildings.pdf),
p. 4 optically checked, Figure 7 explicitly includes gambrel and mansard forms;
the adjoining text describes composable mass/roof shapes and component surfaces.
[Held/Palfrader 2016](../geometry/2016-weighted-skeleton-roofs-and-terrains.pdf),
p. 6 optically checked, Figures 3/4 show weighted offsets and their roof.
Our construction reuses an unweighted existing hip field and applies one
continuous piecewise height map; it does not implement general weighted events
or infer a measured historical roof from provider data.

Fresh local Ghidra exports on SDK 3.4.12206, core SHA-256
`850a1b10182e9793f67791438989835de08a9facc2bd0d4f12c26c9a8be5ab45`:

- `Processor::roofHip`, `0051b1a0`, 46 instructions: calls `roofSkeleton` and
  dispatches mode/parameters, then a warning on failure. This is not a ready
  two-slope/mansard recipe.
- `splitXYZ`, `004f0900`, 1,588 instructions: native vertex/edge/mesh cut dispatch
  and scope/asset-box handling. Direct calls do not prove Python/Esri triangle,
  normal or UV equality; indirect/unresolved callees and inferred types remain
  outside the evidence.

Both bounded exports completed with headless exit 0 under the direct installed
Ghidra launcher. Original JSON/C/logs are local in
`/tmp/outshine-buildings-broken-roof-esri-1bd34d148/`. The existing versioned
`esri_ghidra` script supplied the bounded symbol/body exporter; only these two
targets were selected. Binary references remain external.

The initial separate split/scale/emit composition passed analytic, concave and
courtyard controls but failed an actual Paris source with an unmatched interior
edge. Direct original-triangle surface cuts avoid those separate scope
roundtrips. Existing `mesh_section` also omits an entire ring when its plane
coincides with a profile knot: tests therefore use an independent native solid
section at that exact knot, plus unchanged original sections on either side.
No diagnostic failure is used to relax the closed-surface/coverage requirements.
The improved full logical split/emit followed by the same global height map
passes all three actual source controls and remains the valid equal-work
baseline. It is distinct from the rejected separately scaled scopes. Exact
closed-solid sections/volume, UV interpolation and face ownership are checked;
the direct path avoids its logical reconstruction/extra shape results.

Frozen source/wheel controls, six opened actual 1920×1080 component images and
the whole Paris comparison are recorded in the PR/experiment README. All 988
Paris sources remain, with 210 morphology-selected profiles; the Rome image
is a forced component test only. The first city process ended 143 after writing
the mesh/report. A controlled fresh domain call returned and explicitly flushed/
exited 0; all arrays/dtypes match the first trial. This proves that bounded
construction result, not normal backend/destructor cleanup or native/device
admission. Broader roof/dormer/attic/architecture and image-LOD work stays open.
