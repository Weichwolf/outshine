Type: defect
State: active
Parent: 2105
Depends: 2231
Area: engine, generators, world
Tags: streaming, lod, geometry

# Building detail follows each structure's camera distance

## Problem

Structure baking assigns one OSM tile distance to every building in that tile. The tile containing
the camera therefore bakes distant buildings at `Fine`; the focused floor-contact Place left four
large bake jobs unfinished after 15 s. A tile centre is not a rendering input and does not represent
the nearest point of an individual footprint.

## Decision

Capture the geographic camera position with each bake request and include it in stale-input
validation. For every footprint, derive the nearest point of its latitude/longitude bounds to that
snapshot and use its local metric distance for screen-space detail selection. Keep the nearest
visible-distance lower bound. Remove the tile-centre distance API; no generator may use it as a
proxy for geometry visibility.

## Acceptance

- A near and a distant footprint in one raw tile receive different detail levels from one camera
  snapshot; distant massed geometry does not invoke the detailed mesher.
- A moved camera invalidates an unfinished bake before publication.
- The floor-contact Place retains its geometry contract and improves or preserves its 15 s
  residency result without lowering quality; focused tests and lint pass.

## Measurement

2026-09-17: the direct floor-contact executable remained unprepared after 16.62 s, with 27/31
tiles baked and four jobs holding 376 structures. The prior tile-wide LOD choice is removed, but
the bounded-bake scheduler/residency bottleneck remains in 2231; this WI cannot close on that
measurement alone.
