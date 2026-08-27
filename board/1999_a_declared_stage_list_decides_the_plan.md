Type: bug
State: active
Parent: 1995
Area: render, scenario
Tags: declarative, gpu-driven

# a declared stage list DECIDES the plan, and the engine's default stands only where none is declared

**Benchmark** — Unreal: passes are selected in C++ from `ShowFlags` and the view family's
settings; a scene does not author its pass list. RAGE: the render phases ARE data -- phase lists
and render targets are declared in the settings files the game ships, not chosen in a function
body. **Taking RAGE**, because this tree's invariant is already its own: *scenarios declare, the
engine behaves*, and *a section NOT declared decides nothing -- the engine's own default stands in
its place*. The catalogue is the vocabulary that makes the declaration checkable, and it exists.

## What is measured

`Scenario::RenderPlan::Stages` is filled by `ScenarioRead.cpp:208` and merged correctly by
`ScenarioLayer.cpp:144` -- board:1678..1681 closed the merge semantics. **Nothing reads it.**
`grep -rn '\.Stages' src/` finds the reader and the merge and no consumer at all.

The plan is built instead by `DeclarePlan` in `src/engine/Live.cpp:24`, from four booleans:

```
declaration.Content = {Subjects, Overlay};
if (sky)     { ...push_back(Sky); }
if (shadows) { ...push_back(LightVisibility); }
if (moves)   { Outputs.push_back(SceneVelocity); }
if (carriesGlass) { ...push_back(SubjectsTransmissive); push_back(CompositeTransmission); }
```

So the tree has ONE scenario file (`apps/driver/src/f31.scenario`) and it declares no stages, and
the surface is dead twice over: no writer and no reader. A declaration parsed that changes no
pixel is not read, which is this tree's own words about glTF and applies here unchanged.

**The second half, and it is the one that costs a picture.** `moves` asks whether the glTF
ANIMATES, not whether anything MOVES. The driver's car is re-placed nine rows every frame over a
ground ring that is placed once -- measured by `outshine/door/ScoreWhatAMovingSceneResends` -- and
gets no `SceneVelocity` target, because its asset carries no animation. `TemporalResolve` is never
put in any plan either, so nothing reads velocity even where it exists. This is why board:1998's
remaining predicate cannot be written: a door case declares `temporalResolve` and `sceneVelocity`,
the declaration is ACCEPTED, and `VelTex_` stays null -- because the declaration was never
consulted.

## What will be true

- [ ] A scenario that declares `<stage name="...">` gets those stages, and one that declares none
      gets `DeclarePlan`'s answer -- the `Declared` flag read where the decision is made, not a
      list silently replaced by a function body. An unknown stage name is REFUSED by name.
- [ ] The velocity target follows what MOVES, not what animates: a scene whose placements change
      between frames carries `SceneVelocity`, and `outshine/door/ScoreWhatAMovingSceneResends`
      reads a non-zero moving-pixel count over the drive where it already counts nine re-sent rows.
- [ ] board:1998's predicate closes on top of this: the door publishes a velocity number, it reads
      zero for a still scene and non-zero for the drive, and the negative control is the previous
      placement row -- with `was = now` the drive reads zero while the picture is unchanged.

**The measurement that shows I am wrong**: if making the declared list decide changes any khronos
picture, the default and the declaration disagree somewhere they must not -- 444/444 is the floor,
and a case that moves is the proof the surface is live.
