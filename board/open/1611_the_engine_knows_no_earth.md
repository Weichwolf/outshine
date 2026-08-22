Type: issue
Area: core
Tags: scope

**The engine knows no Earth**

The owner's ruling (2026-08-22): a world is a SPHERE with gravity, shaped by height data.
Driving and walking EMERGE from gravity and surface through the physics -- on a smaller sphere
with less gravity you jump higher and driving works badly, and nothing in the library says so
explicitly, because it follows. Sky, clouds, sun, moon and stars are EARTH-SPECIFIC
declarations of the scenario. Walking on the Moon or Mars changes the scenario (gravity,
radius, sky) and the height-data providers -- and nothing else. The outshine library must not
care which planet it is standing on.

## The audit at HEAD (each either becomes a declaration or justifies itself as provider-specific)

| constant | where | verdict |
|---|---|---|
| g = 9.80665 | src/pilot/Pilot.h, src/corridor/SpeedProfile.h, src/sim/DriveTick.cpp, src/sim/CorridorLay.cpp, unit tests | **declaration** -- the world declares its gravity; the speed plan, the crest bound sqrt(g/h'') and the rig all take it from there |
| Earth radii 6371000 / 6378137 + WGS84 e2 | src/core/Camera.h, src/core/Geodesy.h, src/ground/tiles/TileGeodesy.h, src/clients/GltfStudio.h (ECEF anchor) | split: the SPHERE RADIUS is the world's declaration; the WGS84 ellipsoid and Web-Mercator tiling are PROVIDER geodesy -- an Earth DEM provider declares them the way a Mars provider declares its own grid |
| atmosphere (Rayleigh/Mie/ozone) | Medium struct | already a declaration -- correct by construction |
| sun/moon/stars/ephemeris | scenario lighting, star provider, Ephemeris | declarations and providers -- correct in shape; Ephemeris must say it computes EARTH's sky from declared elements, not the engine's |

## What must become true

- [ ] the scenario declares the world's sphere: `<world radiusM=... gravityMs2=...>` with Earth's
      values in the driver's declaration, derivation cited
- [ ] no `9.80665` outside a declaration file or a test's own fixture; the plan, the rig and the
      pilot take g from the declaration path
- [ ] provider geodesy (WGS84, Mercator band) moves under src/data or the provider's own
      declaration -- named as the PROVIDER's shape, not the world's
- [ ] the proof: a scenario declaring moon gravity rides the synthetic unit-gate road with the
      crest speeds sqrt(g/h'') scaling exactly as declared -- no code change, only declaration
