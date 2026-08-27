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

- [x] A scenario that declares `<stage name="...">` gets those stages, and one that declares none
      gets `DeclarePlan`'s answer. `Declaration` carries the list from `Declaring.cpp` and takes
      part in the equality that decides whether the plan is rebuilt; `DeclarePlan` resolves each
      name through `Compiled::StageByName` -- which stood in the tree with ZERO callers -- and
      refuses an unknown one with the name in the reason.
      proof: outshine/door/ScoreWhatADeclaredStageListDoes reads
      `THE DEFAULT PLAN RUNS 5 stage(s)` beside `A DECLARED LIST RUNS 3 stage(s)`, and the
      refusal quotes 'terrain'; khronos/glTF 444/444, because no corpus case declares stages and
      the default path is untouched.
      negative control: `if (false && !stages.empty())` makes the declared list read 5 stages too
      and BOTH checks go red -- the count check and the refusal, because an ignored list is also
      an unchecked one.
- [x] The velocity target follows the CATALOGUE, not the file. `subjects` declares
      `SceneVelocity` among its writes unconditionally, so the plan keeps it whenever `subjects`
      is in the content -- the `if (moves)` that asked whether the glTF ANIMATED is gone, and with
      it the `moves` parameter, which the compiler then reported as unused.
      proof: outshine/door/ScoreWhatAMovingSceneResends reads
      `57600 pixel(s) moved, the furthest by 0.695012 ndc` over the drive, where it read -1
      before -- the driver's asset carries no animation track; khronos/glTF 444/444.
      negative control: restoring the condition on `file.Animations()` makes that line read -1
      and the case goes RED.
- [ ] **board:1998's predicate needs a STILL CAMERA and this tree has none.** The door now
      publishes the velocity numbers, but the drive cannot isolate a placement's own contribution:
      with `was = now` forced in `HandPlacements` the same case reads the same 57600 px and the
      same 0.695012 ndc, because the camera drives too and its motion sets both. A green negative
      control is a false proof, so the claim about the previous row was taken OUT of that case
      rather than left standing. What is needed is a scenario with a fixed eye over a subject
      whose placement moves -- then the moving-pixel count is the subject's own silhouette and
      `was = now` drops it to zero.

**The measurement that shows I am wrong**: if making the declared list decide changes any khronos
picture, the default and the declaration disagree somewhere they must not -- 444/444 is the floor,
and a case that moves is the proof the surface is live.
