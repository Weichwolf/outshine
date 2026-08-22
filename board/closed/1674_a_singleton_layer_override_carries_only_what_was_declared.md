Type: issue
Area: scenario
Tags: mods, layering, loud-failure

**A singleton layer override carries only what was declared**

The wholesale replacement 1671 closed on has two holes, and the winter fixture hides
both:

1. **A layer inhales ambient engine state.** `Engine::ReadInto` seeds EVERY layer
   fragment with the engine's ambient extent (src/clients/Engine.cpp:193
   `fragment.Render.Frame = S_->Frame;`), `ReadRender` defaults `widthPx`/`heightPx`
   to that seed, and `MergeLayer` replaces wholesale
   (src/scenario/ScenarioLayer.cpp:134-137). Base declares `widthPx="1920"`, a mod
   declares `<render fps="30"/>` → the merged frame is 1280×720 — a number NO
   declaration spelled, neither the base's nor the layer's. That is not "the layer's
   whole section", it is the engine's ambient default wearing the layer's name.

2. **Omission inside a declared singleton zeroes the base.** A mod that dims the light
   with `<lighting><key lux="3000"/></lighting>` replaces `Lit` entire
   (ScenarioLayer.cpp:126-129): ElevationDeg, BearingDeg and Environment fall to the
   struct's zeros (ScenarioRead.cpp:207, include/outshine/Scenario.h:82-86). The
   canonical winter proof only passes because the fixture restates `elevationDeg="8"`
   (test/unit/scenario/ALayerOverridesAnEarlierOneById.cpp:68) — the trap is
   untested, not absent. The house's own template rule reads the other way: "setting
   replaces, removal is named, omission keeps the template's value" (CLAUDE.md). Two
   override mechanisms in one grammar with OPPOSITE omission semantics is a decision
   nobody recorded.

Demanded: adjudicate delta-vs-wholesale for the singleton sections against the
template rule and write the verdict down; whichever wins, a layer fragment is never
seeded with ambient engine state (the seed belongs to the BASE read at
Engine.cpp:171, not to fragments); the proof pins either base-field survival (delta)
or the documented, traced loss (wholesale) — for lighting AND for render's frame.

---

Closed, adjudicated by the house's own template rule: OMISSION KEEPS, AT ATTRIBUTE LEVEL.
The singleton sections no longer replace wholesale -- a layer's section re-parses ONTO a
copy of the base through the reader's own semantics (ReadScenarioInto, the non-resetting
door), where every singleton attribute now defaults to the value already standing (the
readers stopped defaulting to zero); only the sections the layer declared carry over. The
ambient-frame seeding is gone with the fragment seeding itself -- no number a declaration
never spelled can reach the result. Proving test: ALayerOverridesAnEarlierOneById -- a mod
spelling only lux keeps the base's elevation 55, only fps keeps 1920 wide, only view keeps
who the player is. 137/137.
