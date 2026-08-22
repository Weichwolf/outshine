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

- [x] the scenario declares the world's sphere: `<world radiusM=... gravityMs2=...>` -- gravityMs2
      is in the grammar and the WorldSettings default carries the standard value (CODATA standard
      gravity) as the earth template's seed until earth.xml owns it
- [x] no `9.80665` outside the declaration surface or a test's own fixture (commit f1c48fe3):
      Envelope.GravityMs2 feeds the plan, the rig, the pilot, the tick and the rail/wing forms;
      Stand refuses a world without gravity
- [ ] provider geodesy (WGS84, Mercator band) moves under src/data or the provider's own
      declaration -- named as the PROVIDER's shape, not the world's. Survey at f1c48fe3, the
      slices in working order:
      1. [DONE] src/core/Camera.h kEarthRadiusM + HorizonDipRad: zero callers, deleted
      2. [DONE] Wayfinding's kEarthRadiusM died: ApartM, the free Plan and Network take the
         sphere radius; Journey and LayCorridor pass world.RadiusM; the seed 6371008.8 (IUGG
         mean) lives ONCE in WorldSettings and the reader defaults to the struct
      3. [DONE] the datum lives with the providers: src/data/Wgs84.h holds kWgs84A/kWgs84F/
         kMercatorGirthM named as the shipped providers' shape; ground/tiles imports downward
      4. src/core/Geodesy.h GeoToEcef WGS84 a/e2: the height/imagery providers' datum -- moves
         under src/data; consumers (ground fields, generators, clients) take ECEF products or a
         provider handle, never the constants. SURVEY (2026-08-22): the slice is larger than a
         file move -- kMPerDeg (Units.h, 111320 [SET], the equirectangular per-degree) is woven
         through TangentFrame, Region, Forest, the planar family and their tests, and the
         GENERATOR layer itself speaks lat/lon (Region spans, Forest jitter, BuildingMesh/Shape
         call GeoToEcef): the real cut is the generator coordinate model -- params arrive in
         METRES in the region frame, the datum stays with the providers. One move, not two
         half-moves; needs its own sitting
      5. [DONE] the studio anchor carries its origin: a [SET] stand at equatorial ECEF
         magnitude, no planet implied
      6. [DONE] EarthSunPos/EarthMoonPos say whose sky they compute (Meeus/NOAA short
         series, arcminute class); another sphere declares its own; the reserved-identifier
         header guard died in the same touch
- [x] the proof (commit 5cde3c05): ASpeedPlanScalesWithTheDeclaredGravity holds the crest's
      closed form sqrt(g/h'') to 1e-9 and the moon/earth scaling to 1e-12, declaration only;
      ARigRefuses proves 1.62 m/s2 reaches the envelope and g<=0 refuses

---

**Owner refinement (2026-08-22): Earth ships as a template.** The engine knows no Earth -- but
outshine MAY ship declaration TEMPLATES in which Earth stands correctly configured: all
generators, the live providers (OSM, weather), the star catalogue, correct time. Earth is the
central place for most games, so the template earns its place in `src/assets` as content the
scenario selects (`<world template="earth">` or equivalent composition) -- a declaration like
any other, overridable field by field, and the Moon flight (1612) needs nothing from it but a
second template. The rule stands: templates are DATA the catalogue offers; the library still
compiles no planet.

**And the general form (owner, same day): `Planet(params)` as a convenience FACTORY.** The
helpers live as their own component BESIDE the generators and the data providers -- a thin
layer of declaration factories that return a composed Scenario VALUE (Earth's template filled,
or any sphere from its params), which then walks through the normal Declare/Assemble door. The
engine core stays scenario-agnostic; convenience is a library citizen with its own folder, not
a facade verb. The template study (running) decides the file shape.

---

**Learned from the template study (2026-08-22; Cesium, Kopernicus, UE config layers, Godot
scene inheritance, X-Plane/MSFS scenery stacks, the Rails doctrine).** The decided shape:

1. **Earth lives at `src/assets/scenarios/earth.xml`** (moon.xml beside it) -- declared data in
   the assets catalogue, referenced by clients and scenarios, never by the engine (Cesium's
   ion-asset shape: the factory wraps a generic provider with a curated data handle).
2. **`<world template="earth">` is Godot-style instancing, one level deep**: the scenario
   INSTANCES the template and carries only deltas. NOT a patch language (ModuleManager is the
   documented tar pit -- one author per scenario needs none of it), NOT a multi-layer stack
   (Unreal's eleven layers need their own diagnostics; MSFS and X-Plane cannot even agree on
   list direction).
3. **No merger machine -- the parity law twice**: the XML reader walks the template through the
   assembly API, then the deltas through the same API on top. Setting replaces; REMOVAL IS
   NAMED (`<atmosphere none/>`, Kopernicus's removeAtmosphere) -- omitting never deletes.
   Per-field merge semantics are declared: scalars replace, lists (providers, generators) take
   explicit operators or whole-list replacement, never implicit merging (X-Plane's
   replace-vs-stack lesson).
4. **`Scenario Earth()` is a factory returning a declaration** -- no behaviour, no engine verb,
   living beside the client exactly as Terrain.fromWorldTerrain() lives beside
   CesiumTerrainProvider; the Moon is the same factory over a different file.
5. **Contract rules from the pitfalls**: exactly two layers; the EFFECTIVE declaration is
   queryable field-by-source (the Carried()/WhyNot() family); an override against a field the
   template no longer carries is a LOUD error at Declare (Godot's silent orphan is the
   counterexample); and the template must hold 720p60 with zero overrides -- a default that
   requires overrides is a form, not a default (Rails: substitutions possible, never required).

---

**Sharpened (review 2026-08-22, evening): the geodesy survey has four blind spots.**

- src/sim/Journey.cpp:186 and :268 spell `40075017.0` (Earth's Mercator circumference) inside
  the SIM layer — tile ground size and coordinate quantum. Same family as slice 3, worse
  address: not even the ground layer, the drive orchestration.
- src/ground/TerrainLoader.cpp:215 spells the SAME circumference as `40075016.686` — two
  spellings of one constant in one tree; whichever declaration ends up owning it, there is one.
- src/ground/tiles/TileGeodesy.h:30-31 declares `kEarthRadiusM = 6378137.0` AND
  `kWgs84A = 6378137.0` — one number, two names, in the very file slice 3 moves.
- the standard-gravity seed is spelled twice: include/outshine/Scenario.h:37 (member default)
  and src/scenario/ScenarioRead.cpp:164 (`Num("gravityMs2", 9.80665)`). One can drift from the
  other silently; the seed lives in ONE place until earth.xml takes it.


**Sweep (2026-08-22, commit follows the blind-spot sharpening):** the reviewer's four blind
spots are repaid -- the dead horizon helper is deleted with its 6371000; the Mercator girth is
ONE derived constant `World::kMercatorGirthM = 2 pi kWgs84A` (TileGeodesy.h, the tiling's own
datum) consumed by Journey's tile maths and GroundStream::PostM, killing 40075017.0 (twice,
rounded) and 40075016.686; kEarthRadiusM's duplicate name in TileGeodesy died into kWgs84A;
the 9.80665/1.2250 seeds are spelled ONCE in WorldSettings -- the reader defaults to the
struct's own values. Slices 2-6 (Wayfinding radius from the declaration, GeoToEcef under
src/data, studio anchor naming, Ephemeris naming) remain.
---

Reviewer sharpening (2026-08-22, evening round): the purge commit 0b4acae claims "40075017.0
(twice) and 40075016.686 are gone" -- provably false at src/ground/World.cpp:31, where
`static const double kEarthCirc = 40075016.686;` still stands and feeds the tile ground-size
maths at World.cpp:90, in a file that already includes TileGeodesy.h and therefore has
kMercatorGirthM in reach. One-line repayment; the commit's claim is corrected here.
