Type: bug
State: open
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
