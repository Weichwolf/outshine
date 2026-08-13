Type: feature
Area: generators
Tags: oracle, instrument

**I.25 Scenario axes, and the scenario with no world**

*Added 2026-08-12 on the owner's ruling: **it must be possible to declare a scenario with no world at
all — one tree, one building, one car — and the engine must be flexible enough to define what a test
needs, like a headless Blender.** The question put was whether* world present or absent *is a third
dimension beside camera × clock.*

**It is not a third dimension, and treating it as a boolean is what produced today's special case.**
Camera (`Fixed` · `Keyframed` · `Driven`) and clock (rate 0 · timeline · rate 1) are axes of the
**observer**. World-or-not is the **subject**, and choosing it *removes fields from the observer's axes*
rather than adding a dimension: a studio scenario has no latitude, because there is no place, and no
civil time, because the light is declared rather than computed from an ephemeris. A boolean would have
kept those fields and left them meaningless — which is measurably today's state, at eighty dead fields
across ten declared scenes (the bug tasks in `board/`).

**A studio stage is a declared `Ground`, and that is the whole trick.** `Ground` is already the entire
interface between the world and every generator — *"height, slope, class with edge distance and
runner-up, water level and the declared tables — resolved values, never a callback"* (§ I.9). A studio
stage declares those values directly instead of resolving them from a tile, so **every generator becomes
benchable with no second code path**, and a lone building comes out of `Buildings` rather than out of a
bench that had to learn what a building is.

- [ ] `Stage` as an enumeration with a record per arm (`Enum.2`, and `I.23`'s *each kind reads its own parameter object rather than a shared flag soup*, which `clients/Scene.h:39-41` already states for `Run`) — `World` carries the standpoint, `Studio` carries substrate, key light, backdrop and subject
- [ ] A `Studio` scenario **has no latitude to declare**, so the Mercator band refusal is not merely unreachable there, it is unspellable — and `SceneRunner.cpp:32 kSubjectGroundAslM = 100.6` has no home
- [ ] A `Studio` scenario has **no civil time and no met wind**: the key light is declared as elevation, azimuth and irradiance, and the wind as a value, because a still life judges form and an ephemeris there is a number nobody chose
- [ ] The declared key light's irradiance is **W/m² perpendicular to the beam**, the same convention `IrradianceStage`'s `sunDirectNormal` already carries and the same one Blender's Sun Strength is (§ I.26) — so the studio light and the oracle's light need no conversion between them, and the commonest single error of this class, perpendicular against on-the-horizontal, has one spelling in the tree instead of two
- [ ] The studio's **ambient is declarable as a uniform environment radiance**, not only as a sky model, because the oracle's default world is one and because it is the only ambient with a closed form. `render/stages/IrradianceStage.h`'s `skyIrr` is *diffuse on horizontal*, so a uniform environment of radiance `L` enters as `π·L` and the conversion is stated once, here
- [ ] The four things a studio must declare, because `SubjectBench` had to invent all four in C++: **substrate** (a ground-material class, or the 18 % neutral), **key light**, **backdrop** (a card, a declared sky, or nothing), **subject**
- [ ] The studio's `Ground` is declared in full — height, slope, class with edge distance and runner-up, water level, and which library tables are in force — so a generator run on a studio stage cannot tell it apart from a region, and a difference between studio and world output is therefore a defect rather than a category
- [ ] The **subject is a generator invocation**, not a species name: generator, seed, and the parameters that generator declares. `clients/Scene.h:77-82 SubjectRun` carries `Template` and `Species` and nothing else, which is exactly why a building or a car has no bench today
- [ ] Exactly one subject stands at the origin, and the count is one because a bench that frames two things is measuring composition rather than form
- [ ] `SceneRunner::BringUp`'s special case is deleted — *"whether a scene needs a world at all is the scene's own statement"* is the comment at `SceneRunner.cpp:150-151` and the code below it asks `Runs().front().What == Kind::Subject`. The stage is the statement
- [ ] `SubjectBench`'s **measurement survives whole**: the full view × light matrix, the three-render depth-buffer `Fill` that separates frame from card from floor (measured 64.7 % against 13.0 % for the colour-difference alternative it replaced), the turn-based readback discipline, and the 30° lens with its reason. What is replaced is exactly the part that made it vegetation-only — `Select`/`SelectTree`, `Stand(lat, lon, 100.6)` and `kSunElDeg` as a C++ constant
- [ ] `fovDeg` declared once — `mods/demo/mod.json` and `SubjectBench.h:62` both say 30 and the C++ one acts (the bug tasks in `board/`)
- [ ] A studio scenario **needs no network and no tile source**, which is what puts a generator test in § I.20's `host` tier instead of its `world` tier — the single largest effect this section has on the suite
- [ ] Camera and clock keep their axes unchanged across both stages: a studio scenario can be a still, a turntable film or an interactive model viewer with no new mechanism, because those are the observer's axes and the stage did not touch them
- [ ] A `World` stage and a `Studio` stage produce the same telemetry schema, so a bench row and a walk row are comparable — a bench is a layer over the system and never a mode inside it (§ I.11)
- [ ] More than one subject kind demonstrated before the section is ticked: one tree, one building, one vehicle, from three different generators, through one declaration
