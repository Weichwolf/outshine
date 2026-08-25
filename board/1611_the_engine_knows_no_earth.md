Type: issue
State: open
Area: core
Tags: scope, world
Supersedes: 1827

# The engine knows no Earth, and a frame is declared

A world is a SPHERE with gravity, shaped by height data. Driving and walking EMERGE from
gravity and surface through the physics — on a smaller sphere you jump higher and drive badly,
and nothing in the library says so, because it follows. Sky, sun, moon and stars are the
SCENARIO's declarations. Two audits stand open.

**The constants.** `src/base/geo/Geodesy.h:10` hard-wires `const double a = 6378137.0, e2 =
6.69437999014e-3;` inside `GeoToEcef`, taking no sphere as a parameter; `src/world/data/Wgs84.h` is a
file named after an Earth datum. Gravity and the sphere radius are declarations now
(`<world radiusM=... gravityMs2=...>`); provider geodesy — the WGS84 ellipsoid, the Mercator
band — is the PROVIDER's, and belongs under `src/data` where an Earth DEM declares it the way a
Mars provider declares its own grid.

A third site, measured 2026-08-25: `struct alignas(16) Medium {` (src/render/stages/
ParticipatingMedium.h:11) defaults `float BottomRadiusKm = 6360.0f;` (:12),
`float TopRadiusKm = 6460.0f;` (:13), `float RayleighScaleHeightKm = 8.0f;` (:14),
`float OzoneCentreKm = 25.0f;` (:22) and `float GroundAlbedo[3] = {0.10f, 0.13f, 0.07f};` (:27).
That is Earth's atmosphere and Earth's ground, in a RENDER STAGE, which the layer table forbids
twice over — a renderer may spell no content noun at all. The atmosphere is the sphere's
declaration; the stage takes it as a value it is handed (board:1877 grades the numbers).

**The frame.** ECEF is right as a KIND — body-centred body-fixed cartesian is correct for any
rotating body — and wrong as a NAME and as a constant in the core: `Ecef` appears 89 times
across 20 files including `src/core/`, `src/render/` and `src/ground/`. The generic name is
BCBF, and the frame is a declaration with (radius, flattening, rotation rate) in it.

**The template.** Earth ships as `src/assets/scenarios/earth.xml` and a scenario INSTANCES it
one level deep, carrying only deltas: setting replaces, removal is named, omission keeps the
template's value, an orphaned override refuses LOUDLY at Declare, and the reader walks template
then deltas through the SAME assembly API — no merger, no patch language. The template alone
must hold 720p60.

## What will be true

- [ ] No planet name and no planet number outside a declaration, a template or a provider —
      `Geodesy.h`, `Wgs84.h` and `ParticipatingMedium.h` are the three known sites.
- [ ] `GeoToEcef` takes the sphere it is converting for, and the core spells no datum.
- [ ] `earth.xml` exists, a scenario instances it, and an orphaned override refuses by name.
- [ ] `Ephemeris` says it computes an EARTH sky from declared elements — the scenario's, not
      the engine's (src/world/sky/Ephemeris.h:9-11 bounds a sphere the engine may not name).
