Type: task
Parent: 1498
Area: generators
Tags: scope

**The corridor is geometry, and the wheels stand on the same triangles the eye sees**

The windowed half of the goal is blocked here and not on the drive. `Engine::Declare` stands up one
glTF subject -- it can show the F31 -- but there is no road under it, because the corridor is a
`ReferenceLine` plus a `Stand()` query and not a surface anybody can draw.

**The goal forbids exactly the shortcut that would unblock it fastest:** *a road the physics agrees
with but nobody can see, or one that is drawn and cannot be driven, is two roads and a defect.* So a
window showing the car over nothing is not a step towards the goal, it is the thing the goal names.

## What must be true

- [ ] **The corridor sweeps its cross-section into a SOLID**: carriageway, shoulder, verge and side
      slope, with a thickness, so a bridge deck has a soffit and `board:1518`'s clearance rule has
      something to measure
- [ ] **`Stand()` and the mesh are one statement.** Today `Stand()` computes a height and a normal
      analytically; the mesh must be evaluated FROM that same function, or the two will differ and
      the car will drive on a surface nobody drew
- [ ] **The mesh is generated per tile and streams**, because 774 km of road at any useful resolution
      is not one buffer -- and `board:1529` says the same about the terrain it deforms
- [ ] **Lane markings are on the surface and derived from the same lane count** the pilot drives by,
      so what the eye reads and what the car holds are the same declaration
- [ ] **A frame at 720p60 holds while it is driven**, which is what makes the windowed mode a test of
      the engine rather than a viewer

## Comments

**This is the item the goal's central sentence is about**, and everything else in the driver has been
built so that it can be written honestly: the reference line, the profiles, the lane geometry, the
grade shaping and the cut and fill are all functions of station and offset, which is exactly what a
sweep needs.

`tools/driver/parts/Journey.h` already exposes `Corridor()` and `Carried()`, so the windowed driver
needs no new access -- it needs the road to exist.
