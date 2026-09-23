Type: debt
State: active
Area: base, generators
Tags: architecture, owner
Supersedes: 2116
Parent: 2188

# Shared geometry primitives need shared semantics, not just similar code

## Current finding, 2026-09-23

The 2026-09-04 copy-count table was obsolete. `GroundYield.cpp` no longer exists;
its claimed `LayCutFace`, `Refine` and `EdgeKey` functions are absent from
`EarthworkPress.cpp`. The cited Wayfinding line now indexes graph cells, and
`WaterField::Tessellate` was removed. Those claims cannot justify a shared library.

One real overlap remains: `RoofSurface.cpp` has a local `EarClip`, while water
surfaces use the pinned Mapbox Earcut adapter in `generators/water`. They have
related purposes but different inputs, hole behavior and failure contracts.
Do not replace either until a native polygon contract and negative controls
prove that a common implementation preserves the required semantics.

Point-in-ring, ring area and edge-key code also need a semantic audit before
consolidation. Coordinate frame, boundary inclusion, winding, holes, degenerates
and numeric tolerance are part of each contract. Identical syntax alone is not
evidence that two functions are interchangeable. `Refine.h::Divide` and its
`EdgeKey` have no identified duplicate in the current earthwork module.

## Ownership decision

`src/base/` owns a primitive only when multiple real consumers need the same
coordinate-independent contract. Generators own domain policy, materials and
presentation. Keep one native geometry model; adapters convert external formats.
No umbrella geometry utility, compatibility aliases or speculative extraction.

A candidate primitive needs an analytical test, boundary/degenerate cases and
an independent oracle where available. Migrate all consumers together and
compare affected Place PNGs; pixel equality alone does not prove correctness.
Measure CPU cost and allocations before moving a hot-path implementation.

## Executable slices

- `TriangleBvh` validation: reject invalid indices and nonfinite vertices before
  build or refit mutation. Failed refit preserves triangles and bounds. Preserve
  valid ray hits/heights and compact hierarchy without frame-path allocation.
  Prove analytic planes/rays, multi-leaf traversal and rollback with negative
  controls. Reference: locally pinned PBRT if consulted; no web lookup.
- `FeatureField` input boundary: reject nonfinite coordinates/heights, invalid
  form/kind enums, negative ribbon width and overflowing index ranges. Compute
  bounds as a separate phase. Preserve valid area/ribbon and forest consumers;
  return an explicit error instead of collapsing all failures to null.
- Roof/water triangulation: specify native polygon rings, winding, holes and
  degenerates, then test both consumers against analytical polygons and an
  independent oracle. Extract only the proven common operation, if there is one.

## Acceptance

- [ ] Each extraction names its actual consumers and common semantic contract.
- [ ] Invalid input fails locally and leaves the previous valid product intact.
- [ ] Analytical and negative tests cover coordinates, topology and rollback.
- [ ] `make format`, focused suites, Place PNG review and `make lint` pass.
