Type: proof
State: active
Architecture: ready
Parent: 2260
Depends: 2281
Priority: P0
Area: generators, engine, client, road
Tags: hockenheim, contact, geometry, streaming

# Published road surface proves route contact at station

## Evidence and contract

`Engine::sampleRoute` returns a valid alignment pose without checking the
published road mesh. `RoadSurfaceBuilder` retains ordered per-station spans,
two triangles per span and the native geometry after publication. The 220-s
Hockenheim camera lap has no per-frame proof that its eye is above a continuous
render/contact surface. A screenshot or a midpoint height test cannot certify
interior curvature, an edge seam or a missing published mesh.

`generators/road` owns a bounded, allocation-free query on a matching
`RoadAlignment` and `RoadSurface`. It resolves the station in O(log spans),
transforms the alignment point and signed lateral offset into the surface's
render frame, and barycentrically intersects only the span's two actual
triangles. Positive lateral means left when looking forward. Return a finite
surface position, unit upward normal and source span; reject invalid station,
offset outside the usable half-width, missing/degenerate triangle and
source/terrain revision mismatch. No fabricated interpolation across a gap.

`Engine` exposes a format-free, read-only route-contact value query, tied to
the same current source revision and immutable publication as `sampleRoute`.
The client motion trace samples centre and both near-edge tracks at each tick,
recording missing contact and eye clearance in metres. A missing contact is a
failed proof, not a fallback DEM height. This query is an inspection/contact
primitive; vehicle dynamics and collision response remain WI 2261.

## Abnahme

- Analytic straight, curved, seam and closed routes: every selected point lies
  on the corresponding native triangle within float-quantization tolerance;
  removed span and changed source revision reject. Outside-width and NaN
  inputs reject without allocation.
- Pinned Hockenheim/real DEM: sample the full route at <=1 m station spacing,
  including every edge boundary and both ±0.45-width tracks. Report the
  largest clearance and first failing edge/station; do not weaken a failing
  case to green. Public Engine test distinguishes pending and published mesh.
- Paced full-lap trace includes per-frame contact/clearance, missing count,
  p50/p95/p99 and peak memory. Open route-mark PNGs. Format, focused suites and
  `LINT_JOBS=2 make lint` pass.
