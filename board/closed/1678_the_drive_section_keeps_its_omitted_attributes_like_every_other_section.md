Type: bug
Area: scenario
Tags: layer merge, omission keeps, 1674 follow-up

# The drive section keeps its omitted attributes like every other section

1674 established omission-keeps at attribute level and ApplyLayer carries seven singleton
sections through the non-resetting door. Six of the seven honour it. The drive reader does
not: src/scenario/ScenarioRead.cpp:588-592 reads `drive.Num("fromLat", 0.0)` — literal
zeros, not the standing values — so the re-parse over the base copy ZEROES what the layer
omitted.

Proven (standalone repro against ScenarioRead+ScenarioLayer):

    base:  <drive fromLat="48.1" fromLon="11.5" toLat="52.5" toLon="13.4" zoom="14"/>
    layer: <drive zoom="15"/>
    after ApplyLayer: fromLat=0.0 toLat=0.0 zoom=15

Worse, the trace lies: ScenarioLayer.cpp:170 lists drive in the sections table, so the merge
writes "layer 'mod' merged into the drive -- omitted attributes keep the base's values"
while wiping four coordinates. A route to lat 0 lon 0 is the Gulf of Guinea, loudly wrong at
sea, silently wrong in the declaration.

Demanded: ScenarioRead.cpp drive block defaults to `into.Driven.*` like world/lighting/
player/clock/physics do (struct defaults in include/outshine/Scenario.h:257-264 are already
0.0, so a fresh read is unchanged), and the unit twin
test/unit/scenario/ALayerOverridesAnEarlierOneById.cpp gains a partial-drive case that
would have caught this.

---

Closed: the drive keeps like every other section -- its five attributes default to the
values standing, so zoom-only keeps the route's four coordinates. Proven: base
Munich->Berlin route + <drive zoom="15"/> keeps 48.1/52.5 and takes the zoom; the trace's
"omitted attributes keep the base's values" is true for the last section it lied about.
