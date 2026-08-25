Type: task
State: open
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

- [x] **The corridor sweeps its cross-section into a SOLID** -- `src/corridor/Ribbon.{h,cpp}`:
      carriageway, shoulder either side and a THICKNESS along the surface normal, so a bridge deck
      has a soffit and `board:1518`'s clearance rule has something to measure. Sixteen triangles a
      segment, eight vertices a station. The verge and side slope are not built
- [x] **`Stand()` and the mesh are one statement.** Every vertex of the top surface is placed by
      `StandAt` itself -- not by a copy of the formula. Measured over 401 stations of a banked,
      climbing arc: **0 vertices differ beyond float, worst 9.35e-7 m**, where a float at 500 m from
      the frame origin resolves 3.05e-5 m. What is left is the mesh being float where the physics is
      double, which is a property of the STORAGE
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

## Comments -- what the first sweep measured

800 m of banked, climbing arc at a 2 m step: 401 stations, 3208 vertices, 6400 triangles -- **8
triangles a metre** at a 7.5 m carriageway with 2.5 m shoulders. Over 774 km that is 6.2 million
triangles, which is exactly why the remaining line says *per tile and streams*.

The drawn cross-slope is the declared one to 1e-5 m, and the soffit sits 0.3492 m below a 0.35 m
section -- because the thickness is measured along the SURFACE NORMAL and the road both banks 0.06 rad
and climbs 3 %. On a level road those would be the same number; on this one they differ by 0.8 mm,
and the one a bridge is measured under is the normal.
