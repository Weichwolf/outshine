Type: bug
State: open
Area: door

# `Assemble()` after `Read()` without `Declare()` is refused, and says which verb is missing

**Benchmark** — Unreal: `UWorld::InitializeActorsForPlay` refuses on a world that was loaded and
not initialised, by name. RAGE: a map that is streamed but not activated is an assert. **Both
agree** — a stage skipped is refused where it is skipped, never absorbed.

`apps/bench` called `engine.Read(path)` then `engine.Assemble()` and skipped `Declare()`. The door
accepted it. The drive then ran ONE step and failed with

    nothing joined this picture from a file, so there is no body to carry --
    every part stands where the world put it

which is a true sentence about a state four verbs downstream and says nothing about the verb that
was missed. A reader debugging it looks at the asset, the scenario's body and the joining -- all
of which are correct.

**A client's line count measures the door** (CLAUDE.md). Here the client's line count was one line
SHORT and the door said nothing until the shortfall had propagated.

- [ ] `Assemble()` on a session that read a declaration and never declared it refuses, naming
      `Declare`
- [ ] the refusal is reached by a door case, not only by a client that happens to forget

**The measurement that would show I am wrong:** if `Read` is meant to declare, then bench was
right and the door's own three door cases are doing redundant work -- `ScoreWhatAMovingSceneResends`
calls `Read`, then `Declare(engine.Declared())`, then `Assemble()`. Whichever of the two is
intended, one of them is currently wrong and the door does not say which.
