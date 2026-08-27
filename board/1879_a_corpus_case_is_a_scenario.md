Type: issue
State: open
Area: test, scenario
Tags: corpora, door

# A corpus case is a scenario the engine loads and runs

**The front door is: outshine loads a scenario and runs it.** Nothing else. Not a script verb,
not a layout verb, not a parser class — a client declares a scenario and the engine behaves.

Everything under `test/` may use `include/` and NOTHING of `src/`.

**AND THE BENCHMARK DOES NOT HOLD THAT RULE AS WRITTEN.** Unreal keeps low-level tests INSIDE the
module they test (`Private/Tests/`), compiled as part of it; only public-API tests stand outside.
That is the same answer at a different address -- a case that tests an internal type is part of
that module rather than a client of the door. So the rule may be naming the wrong thing: not what
a case may REACH, but where it LIVES. Either way the current shape is wrong, because these cases
neither live inside the module nor go through the door.

**Measured 2026-08-25 at a32c4919: 13 of the 17 declared suites are granted a `-Isrc/...` path
by `LayerIncludes` (test/run.sh:210-224). Only `harness/wpt/css`, `harness/test262/js`,
`apps/driver/src` and `apps/viewer/src` reach the library through `include/` alone.** The rule is
not eroding at the edges; it is the default way a case gets written, and STATE.md does not print
the number (board:1907).

**And it costs a closure its proof.** board:1891 was closed on
`outshine/scenario/ScoreWhenAVolumeFires`, which stands `TriggerField` directly out of
`src/scenario/Triggers.h`. The oracle is good -- enter is an edge and not a state, with a control
that tells the two edges apart -- but the DEFECT that was fixed was three lines in
`src/engine/Engine.cpp` publishing the firing through `Numbers()`, and the case never touches the
engine. Delete those three lines tomorrow and the case stays green: a guard that does not guard
the thing it closed. `outshine/door/ScoreWhatTheShadowCasts`, written the same session
through `include/` alone, is the shape it should have had.

| scorer | what it reaches into | what it actually wants |
|---|---|---|
| `harness/test262/js` | `Script.h`, `program.Run(host, error)` | run this ECMAScript and tell me whether it threw |
| `harness/wpt/css` | `Markup.h`, `Style.h`, `Layout.h` | lay out this document with this stylesheet and give me the boxes |
| `harness/khronos/glTF` | half the render tree via `RenderCase.h` | stand this glTF up and render it at this camera |
| `outshine/door` (2 of 4) | `Live.h`, for `Live::AssetReads()` and `Live::PlanInits()` | the counts the door already carries a channel for: `Engine::Numbers()` returns `Measure` values as of this hour |
| `outshine/physics` (2) | `Body.h`, `Rig.h`, `Rigging.h` | stand a rig and read the wrench it builds |
| `outshine/fuzz` (2) | `Document.h`, `Subject.h` | read this glTF and tell me whether it was refused, and why |

And `test/run.sh` is where the rule is being paid off: three include sets were added at
`test/run.sh:200-202` that hand `-Isrc/content/gltf`, `-Isrc/sim`, `-Isrc/engine` and eleven more
straight to the new groups. Widening the runner is not reaching through the door; it is
deciding not to.

The `door` group is the sharpest of the six, because the door ALREADY has the answer: `Measure`
in `include/Event.h` and `Engine::Numbers()` are the return channel, filed and built the same
hour the two cases chose `Live.h` over it. Publishing `assets read` and `plans initialised`
as measures costs two `Number()` calls and the case stops including `src/`.

`test/harness/shared/` is likewise only for TEST-specific work — manifest reading, prune
bookkeeping, EXR comparison. It may not carry engine internals to get there.

## What will be true

- [ ] A corpus case is expressed as a SCENARIO: the manifest becomes a declaration the engine
      reads, and what the case asserts is what the engine did with it — a picture, a refusal, a
      measured box.
- [ ] `test/` compiles with `-Iinclude` and `-Itest/harness/shared` alone; a claim walks every
      source under `test/` and finds no include that resolves into `src/`.
- [ ] Where a corpus needs something the scenario grammar cannot declare, the GRAMMAR gains it —
      not the door a second verb.

## What the measurement already says

A Khronos manifest declares exactly what a scenario declares: a glTF subject, a frame, and a
CAMERA that is derived from the framing rule the engine itself carries (`src/content/gltf/Framing.h`) --
the manifest quotes it so the runner can refuse a mismatch rather than trust one. `RenderPlan`
holds `Frame` and `Fill`; that is the same rule. **So a Khronos case needs no new grammar: it is
an asset plus a frame plus a fill.**

What the door still lacks for it is the PICTURE in the precision the oracle is kept in: the
oracle is EXR (float) and `Engine::Capture` writes PNG. A client that wants to compare against a
reference needs the frame as it was computed, not as it was quantised -- which is a real client
need and not a test convenience.

The other two are further away: test262 wants a scenario that declares a SCRIPT and reports
whether it threw; WPT wants one that declares a document and a stylesheet and reports the boxes.
Both are grammar, not door.
