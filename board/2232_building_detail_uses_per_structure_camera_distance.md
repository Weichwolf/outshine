Type: defect
State: active
Parent: 2105
Depends:
Area: engine, generators, world
Tags: streaming, lod, geometry

# Building detail follows each structure's camera distance

## Problem

Structure baking assigns one OSM tile distance to every building in that tile. The tile containing
the camera therefore bakes distant buildings at `Fine`; the focused floor-contact Place left four
large bake jobs unfinished after 15 s. A tile centre is not a rendering input and does not represent
the nearest point of an individual footprint.

## Decision

Capture the geographic camera position with each bake request. For every footprint, derive the
nearest point of its latitude/longitude bounds to that snapshot and use its local metric distance
for screen-space detail selection. Keep the nearest visible-distance lower bound. Remove the
tile-centre distance API; no generator may use it as a proxy for geometry visibility.

The exact-eye stale check is incorrect for a moving camera: a paced Hockenheim run rendered all
1800 ticks in 30 s, but refined readiness was absent in every frame and only 1 of 101 posted
structure bakes landed. An unfinished bake remains valid for up to 64 m geodesic camera motion;
the generator coarsens only beyond the usual projected-error threshold plus a 128 m guard. A
distance-to-footprint changes by at most camera travel, so the guard covers the reuse radius even
under coordinate approximation. Beyond 64 m schedule a replacement, retaining the last accepted
geometry. Reservation ownership depends on source/field revision, not camera position; a discarded
job must release its own reservation. Source, height and focal changes remain strict invalidators.

## Acceptance

- A near and a distant footprint in one raw tile receive different detail levels from one camera
  snapshot; distant massed geometry does not invoke the detailed mesher.
- A moved camera inside 64 m does not starve an unfinished bake; a move beyond 64 m invalidates it.
- Camera motion cannot strand a tile reservation; changed height/vector inputs still reject stale
  output. A paced Hockenheim segment increases landed bakes and measures refined readiness.
- The floor-contact Place retains its geometry contract and improves or preserves its 15 s
  residency result without lowering quality; focused tests and lint pass.

## Measurement

The bounded scheduler and current floor-contact Place are green. Close this WI only with the
direct mixed-distance LOD and moved-camera invalidation controls above; aggregate residency timing
does not prove which detail each structure received.
The first paced Hockenheim 30 s run landed 1/101 bakes and refined 0/1800 frames. With the eye
radius/guard and reservation fix, the same run landed 100/101 and refined 336/1800 frames;
p99 advance+render was 12.175 ms, 5 frames exceeded 16.667 ms. Refined readiness still drops
while a new ground candidate is pending. Keep this WI active until the floor-contact Place and
moving-camera LOD transitions pass visual review; WI 2260 owns the remaining moving-ground proof.
