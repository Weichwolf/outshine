Type: feature
State: active
Parent: 1998
Area: scenario, render
Tags: declarative, camera

# a declared view stands at a PLACE, or follows a body, and refuses only when it does neither

**Benchmark** — Unreal: a `ACameraActor` is placed in the level and a view target may be any
actor, attached or not; a static camera is a first-class thing. RAGE: the camera manager owns a
tree of cameras and a vehicle merely FEEDS one -- a fixed camera is a node in that tree, not an
absence. **Both agree**, and neither treats "does not follow" as an error.

`src/scenario/Views.cpp:23` refuses a view whose `Follows` is empty, and the reason it gives is
right about the defect it prevents: *"a camera the client drives frame by frame is the defect this
mechanism replaces"*. But it prevents one thing too many. A view that STANDS somewhere is still
declared -- the client drives nothing -- and it is the only way to watch something move.

**What that costs, measured.** board:1998's remaining predicate needs a fixed eye over a subject
whose PLACEMENT moves, and the chain is blocked at exactly this link:

| link | measured |
|---|---|
| a fixed eye over a subject | reachable: `ScoreWhatAMovedPlacementWrites` reads 0 against 118 |
| a node animation | moves VERTICES -- the engine bakes node transforms like the harness does |
| a body | the only thing that moves a placement, through `Live::Places` |
| watching that body | `Engine::State::Carries` computes the EYE FROM the body, so it follows |

So the relative motion of a falling body is zero by construction, and 0 moving pixels is what a
declared body reads today. The previous placement row that board:1989 and board:1998 built has no
reader, and this is the one link missing.

## What will be true

- [ ] `View` carries a station the way `Body` does -- `bool Placed` and a `Standing` -- and
      `Views.cpp` refuses only a view that neither follows nor stands. The refusal keeps its
      current words for that case, because they are right.
- [ ] The scenario reader takes the station from the same attributes a body uses, so a reader of
      one knows the other.
- [ ] `Carries` uses the active view's station when it has one, and the body only when it does
      not. A view that stands does not move when what it watches moves.
- [ ] board:1998 closes on top: a body falls in front of a standing view, the moving-pixel count
      is its silhouette, and forcing the placement row's previous half to equal its current one
      drops that count to zero while the picture is unchanged.

**The measurement that shows I am wrong**: if a standing view changes any khronos picture, the
station is reaching a path it must not -- 444/444 is the floor, and no corpus case declares a view
at all.
