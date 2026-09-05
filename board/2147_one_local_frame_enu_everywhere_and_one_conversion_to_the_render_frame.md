Type: bug
State: active
Area: base, world, generators, engine, render
Tags: architecture, owner, determinism, audit

# ONE local frame -- ENU in every geographic struct, ONE named conversion to the render frame, and the degree constants right

**Benchmark** -- Cesium: a local frame is East-North-Up, right-handed, and nothing else;
glTF and Filament: the render frame is +Y up, +X right, -Z forward, right-handed; Unreal is
Z-up left-handed and says so ONCE. **All agree** that a tree has one geographic frame, one
render frame and one place where they meet. Measured by the convention audit of 2026-09-05:
this tree carries SIX frames in flight (ECEF, ENU, an East-SOUTH 2D pair, the render frame
(E, Up, S), a Z-up tile frame in the lattice shader, glTF Y-up) and converts between them
with about TWENTY hand-written sign flips in nine files.

## Where it stands, measured 2026-09-05

```
  South-positive structs      RoadStation/RoadGate.SouthM (RoadMesher.h:18,32),
                              GroundMesher.h EastSouth (:61) and Yields (:66-84),
                              Drape::EastSouth (Drape.h:34), three in Corridors.h
  sign flips by hand          Corridors.cpp x10, Laying.cpp x4, Ribbon.cpp x3, RoadMesh.cpp,
                              HeightSheets.cpp x2, TangentFrame.h x2 -- none behind a name
  the cost, paid twice today  the corridor's yield sampled the drape MIRRORED across the
                              east axis (Laying.cpp before 6446a07a; 60-90 m fills), and a
                              junction body's normal seeded up and summed across creases
  buildings mesh in           Z-up ENU (BuildingMesh.cpp:141-171); roads in (E, Up, S)
                              (RoadMesh.cpp:37); the lattice in Z-up tile-local (msl:54)
  kMPerDeg = kMPerDegLon      used for LATITUDE at TangentFrame.h:55, Geodesy.h:159,
                              Tile.cpp:54/75/79, Forest.cpp:131 -- 0.17 % north, 188 m/deg
  metres per degree           four literals: Units.h:41, Structures.cpp:39, StructureBake.cpp:45
                              (111320), StructureBake.cpp:155 (111000), Corridors.cpp:712
  kWgs84A                     re-declared at Geodesy.h:113
  Asking.cpp:284-296          55.0 (a field of view), 720.0 (a frame height) and 40075017.0
                              (the equator) as literals beside the constants that exist
```

## The solution

- every geographic struct speaks ENU metres (`EastM`, `NorthM`, `UpM`); `SouthM` and
  `EastSouth` cease to exist; `Yields`, `RoadStation`, `RoadGate`, the drape's key follow
- ONE conversion, `RenderFrame::Of(EastNorthUp)` -> (x = E, y = Up, z = -N), in the content
  tier, with a static_assert on its determinant and a case that round-trips; every mesher
  emits through it and nothing negates an axis by hand
- `kMPerDegLat` for latitude, `kMPerDegLon * cos(lat)` for longitude, ONE source each, the
  aliases and the literals deleted; the north error measured before and after at every
  reference place (a building's footprint against the DEM's slope, a road against a bridge)
- `kWgs84A` from Wgs84.h only

## What will be true

- [ ] `grep -rn "SouthM\|EastSouth" src include` reads 0, and a claim holds it there
- [ ] `grep -rn "\-.*NorthM\|-.*SouthM" src` finds only `RenderFrame::Of`
- [ ] the nine references move by the north correction, each counted, placed and looked at:
      a picture that does NOT move is a place where the wrong constant never reached a body
- [ ] Negative control: `RenderFrame::Of` with the sign of z flipped mirrors OldTown and
      the round-trip case goes red

## Landed 2026-09-05: the degree constants, first

The cheaper half with the larger error went first: `kMPerDeg` (the longitude figure) stood
for a degree of LATITUDE at six sites, and four literals restated a constant that existed.
Every site names `kMPerDegLat` or `kMPerDegLon * cos(lat)` by what it means; the alias, the
two `kMetresPerDegree` copies and the bare `111000` are gone; WGS84's `a` and `e2` live in
`Units.h` under "The Earth's shape" and `Wgs84.h` and `Geodesy.h` derive from them; the
engine's `55.0`, `720.0` and `40075017.0` are `kFovUnsaidDeg`, `kFrameUnsaidHighPx` and
`Data::kMercatorGirthM`. A generated footprint that was square in DEGREES (the same half
side in latitude and longitude) is square in metres.

Measured 2026-09-05 against 351583c6's pictures, the north correction alone: OldTown 3.6 %
of pixels, Heidelberg 1.7 %, Shibuya 1.6 %, CentralPark 1.0 %, Venice 3.4 %, Jura 0.3 %,
ZurichPlan 0.2 %, Kaiserberg 0.15 %, Koehlbrand 0.35 %. Looked at OldTown around (516, 422):
the church tower and the far roofs stand a pixel or two further north, the near roofs where
they were -- a shift that grows with the distance from the anchor, which is what a 0.17 %
error in the metres of a degree of latitude does. Buildings were placed through the wrong
constant and the terrain through the right one; they agree now. Pure black 0 everywhere.

## Landed 2026-09-05: East-North everywhere, and the render frame named once

`SouthM` and `EastSouth` are gone from `src`, `include` and `test`; every geographic struct
carries `EastM`/`NorthM` (the door's `EastNorth`); the renderer's z is made by
`RenderFrame::ZOfNorth` / `RenderFrame::Of` (`src/base/math/RenderFrame.h`) and nowhere else,
with static assertions that state the frame (east +x, up +y, north -z, right-handed, the
conversion its own inverse). The three `-NorthM` left in the building generator are in-plane
rotations of a footprint's axis, not frame conversions. The nine references bit-identical
after the rename, so the rename moved nothing.

- [x] `grep -rn "SouthM\|EastSouth" src include test` reads 0 (the claim that holds it there
      is still to write)
- [x] every ENU-to-render negation is `RenderFrame::ZOfNorth`
- [x] the north correction, its counts and its look are above
- [ ] the mirrored-picture control (a flipped sign in `RenderFrame::Of` mirrors OldTown) is
      not run; the static assertions refuse the flipped sign at compile time, which is a
      control of the definition and not of the picture
