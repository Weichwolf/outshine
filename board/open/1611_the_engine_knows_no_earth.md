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
      declaration -- named as the PROVIDER's shape, not the world's
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
