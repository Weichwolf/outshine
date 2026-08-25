Type: bug
State: open
Area: door, scenario
Tags: measured, declarative

# A declaration flag the door ignores is not a declaration

`Engine::Declare` copies the lighting out of a scenario without ever asking whether the scenario
declared one:

```
declared.KeyLux         = scenario.Lit.Key.Lux;          Engine.cpp:559
declared.KeyElevationDeg = scenario.Lit.Key.ElevationDeg;         :560
declared.KeyBearingDeg   = scenario.Lit.Key.BearingDeg;           :561
for (int at = 0; at < 3; ++at) declared.Environment[at] = ...;    :562
```

`Lit.Declared` is written in one place, `ScenarioRead.cpp:205`, and read in one place,
`ScenarioLayer.cpp:158`, where the layer merge uses it to decide which fragment wins. The door
reads it nowhere. So a scenario a client assembles in code carries whatever sits in the
`Lighting` struct whether or not it ever declared a light -- measured: the same key at -80 deg
with the flag left false renders byte for byte the same picture as the same key declared.

`Render.Declared` is worse: `grep -c 'Render.Declared' src/clients/Engine.cpp` is 0, so the
render declaration flag reaches the door not at all. `Ground.Declared`, `Motion.Declared` and
`Time.Declared` are each read once and are not in question.

An engine that is DECLARATIVE cannot have a declaration flag that decides nothing. Either the
flag gates the section at the door -- and a section not declared leaves the engine's own default
standing -- or the flag is a second spelling of "this struct is non-empty" and goes.

Proving test: `harness/outshine/door/ScoreWhatTheKeyLuxDoes`, its last CHECK, standing RED and
declared in `EXPECT_FAIL` with its count. Negative control: the same case's other four CHECKs,
all green -- the key's direction reaches the picture, a hundredfold key change reaches it, the
common scale of key and environment is exactly invariant, and a picture with no subject differs
from one with a subject. The case cannot pass by ignoring lighting.
