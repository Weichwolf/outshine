Type: issue
Area: scenario
Tags: scope, instrument, oracle
Supersedes: 1159

**Still to animated, and the ordering the owner named**

**The owner, verbatim:** *the generators we will complete last. what is importand now is the rendering
pipeline from still to animated scenes. scenario loader, animated glTF scenes.*

**This supersedes `board:1159`'s ordering clause and nothing else of it.** That item's census — 29 cited
ids, `0040`–`0072` citing zero, `src/world` at 6 587 lines with one include-set test, 12 498 lines no
device-bearing suite can spell — is unchanged and still the map. **Superseded rather than reopened,
because reopening would present a whole item as undone when only its last section is wrong**, and because
the record that an ordering *was* decided on delegated authority and then replaced by the owner is worth
more than a tidy state. *This is the second ordering to move; both moves are now readable from the board
by `Supersedes:` alone.*

## Measured, so the ordering starts from the tree and not from the vision

| | measured |
|---|---|
| `src/scenario/` | **1 046 lines**, 8 headers, 3 units — `Mod` `Scene` `Animation` `Fields` `Stage` `Standpoint` `Studio` |
| the loader **loads** | **proven** — `test/unit/scenario/EveryCommittedScenarioLoads.cpp` loads every mod under `test/mods` and walks its scenes |
| the animation sampler | **exists at four layers** — `core/Keyframes` · `core/CatmullRom` · `gltf/Track` · `scenario/Animation`, unit-tested at the sampler |
| `Animation::At(Target, double frame)` | **already frame-indexed**, which is the currency `board:1129` decided in |
| **anything that drives a draw from a time** | **NOTHING.** `git grep Animation -- src/clients/ src/render/` returns one *comment* in `EyeTelemetry.h` |
| animation against the oracle | **never** — `board:1128`'s whole statement |

**So the scenario half is not the `src/world` situation and saying so matters.** The loader is built *and
proven to load*; what is missing is one specific link — **no consumer takes a time, samples the tracks and
updates a draw list.** That is a smaller and much better-defined gap than *nothing has ever run it*.

## The order, and each step is the cheapest thing that makes the next one decidable

1. **One animated glTF against the oracle, frame by frame — `board:1169`.** It forces the missing link
   into existence and proves it against something outside the tree on the same round. Rung 6 of
   `board:0078` already ranks the asset: *time, and nothing else — one object, TRS, no light.*
2. **The frame joins the oracle cache key**, which `board:1128` already carries as arithmetic: every frame
   is its own product, or the early exit is not affordable.
3. **The scenario loader drives that clock** — the mod declares a duration and a frame grid, and the same
   consumer step 1 built is what a declared scene uses. `test/mods/demo` declares a `walk` scene and 30
   others; none has ever been rendered.
4. **Velocity, and only here.** `SceneVelocity` is contributed by every geometry stage and the temporal
   resolve reads it; **with nothing moving, no case has ever produced a non-zero one.** An animated case
   is the first thing that can, and the temporal stage's correctness is unmeasurable until it does.
5. **Then the world case, `board:1162`**, which is now behind these rather than first.

**What is filed already and must not be filed twice**: `board:1128` (the tier), `board:1129` (closed — the
frame-by-frame decision, binding), `board:0078` rungs 6 and 13 (the assets and their order), `board:1161`
(the compile group), `board:1162` (the world case).

**`board:1161` serves this unchanged and is now the earlier blocker.** It was written for a group that can
spell `src/world` and `src/generators`; a scenario case driving a clock needs the same boundary decided,
and the item's own clauses — a fifth group, `render` and `frame` keeping their sets, the boundary held by
a compile failure — are the right ones whichever suite arrives first. **It is not refiled under another
name.**

**Done when** an animated glTF is decided against the oracle frame by frame, a declared scene drives that
clock, and the ordering above is either worked or superseded in turn with its reason recorded.
