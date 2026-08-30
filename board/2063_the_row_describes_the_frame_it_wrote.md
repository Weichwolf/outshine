Type: bug
State: open
Area: client
Tags: measured, gate

# The row describes the FRAME IT WROTE, and six cases stop switching themselves off

**Benchmark** — Unreal: `FAutomationTestFramework` screenshot comparison runs after
`FlushRenderingCommands()` and the metadata it stores is the state of the frame it captured.
RAGE: the replay's per-frame record is written from the frame that was presented. **Both agree
a measurement belongs to the frame it describes**, and neither publishes a mid-stream pass as
if it were the picture.

## Measured at Heidelberg, --audit --measures

| measure | reads |
|---|---|
| tiles the ring laid | 128 |
| tiles it is still waiting for | **128** |
| tiles laid bare on the ellipsoid | **128** |
| rebuild: tiles resident when it did | **0** |

All 128 tiles are PENDING at the instant the row is taken, so all 128 are laid on the ellipsoid.
The picture that was written is not that pass: `Heidelberg-c09be935.png` carries the Koenigstuhl,
the Neckar valley and the Altstadt with terrain under all of it. Both statements are true at once
and the row attaches the wrong one to the picture.

`ClientShot.h:92` refuses on `row.BareTiles > 0.0` with "the elevation never arrived, so the
ground and everything on it is drawn at sea level". Against this row that reads as a defect in the
world; against the picture it is false.

## A COLUMN DRIFT WAS MASKING ALL OF THIS, and it is fixed

`Main.cpp`'s `Row()` prints seventeen tab-separated fields and `ClientShot.h` read `row.Why` from
`field[14]`, which is `SettledOver`. Its value is `2`, `2` is not `-`, so EVERY render case called
`Unprepared("2")` and returned before running a single check. That is why the logs read
`UNPREPARED 2` with no sentence: the message WAS the number.

The doc comment over `Row()` says "every field of a `Shot`, tab separated, in a fixed order ...
this is for the case that scores it". A hand-kept parallel list beside a type -- board:2061's
defect exactly, one floor down, between the client and the case rather than the type and the
serializer.

Fixed by reading `Why` from `field.back()` rather than adding a third copy of the column list, and
by refusing a row shorter than seventeen fields instead of misreading it. `CHECKS` went from 0 to
1: the negative control now runs at all.

## THE MECHANISM, traced and tried

Two things stack, and the second is why the obvious fix does not work.

1. **The measure is only published on a REBUILD.** `Picturing.cpp:462` returns early --
   `if (World.EverLaid && !elsewhere && !grew) { return true; }` -- so a settled frame never
   reaches `Published.Places("tiles laid bare on the ellipsoid", laid->Bare, ...)`. The number the
   client reads is whatever the LAST actual rebuild left there, and that rebuild ran while the
   world was still streaming.

2. **The obvious fix was tried and TAKEN BACK.** `GroundPatchwork.cpp` counts `out.Bare` only when
   `!over.Asking`, and rightly: the asking pass calls `TilePool::Wants` rather than `Mesh`, so
   `built` is never filled and the `built.Verts.empty()` test would call every tile bare. Counting
   from the REPLY alone in the asking pass still reported 128 of 128, and made
   `Engine::settled()`'s `World.Bare == 0` -- inert today because the asking pass never counts --
   into a live condition on an artefact. Reverted.

**That hypothesis is now DEAD too.** `TilePool::Poll` does erase from `Done_` when
`!done->second.Holds`, but `TilePool.cpp:529` sets `result.Holds = true` for every `Rank::Mesh`
job. A meshed tile is never consumed, it is copied out, and the asking pass finds it again. No
tile is re-meshed per frame and the two passes do read the same map.

So three explanations have been measured and all three fell: the round (the ledger reports no
clash and preload's clashes are cleared unseen -- board:2067), the asking pass's own counting
(tried, reverted, still 128 of 128), and the pool consuming its results. What is left is the only
branch not yet instrumented: `TileMeshes::Reply::Ready` returned together with an EMPTY
`built.Verts`, which `GroundPatchwork.cpp:179` also counts as bare. `terrain 128/128` says the DEM
arrived; whether `RunMesh` produced a mesh from it is the number nobody has taken.

## What the two runs actually say, now that the cases can speak

Same place, same commit, IDENTICAL picture, opposite rows:

| | cold cache | warm cache |
|---|---|---|
| `Preloaded` | 0 | 1 |
| building triangles | **1 283 058** | **0** |
| bare tiles | **0** | **128** |

The difference tracks `Preloaded`. When preload does not finish, the world is still being built
through the settle advances and the row reports the full build. When preload DOES finish, the
settle advances rebuild nothing and the row keeps whatever the last rebuild inside preload left.
Both pictures are correct and one row says the elevation never arrived.

## What it cost

`test/outshine/places` -- the only suite that validates the rebuild -- reported
**6 UNPREPARED of 9** on a tree whose pictures are correct. Venice, Shibuya, Rothenburg,
TheAlpsFromTheJura and CentralPark all render well and all were refused. A gate that switches
itself off on a stale counter is worse than no gate, because the tick is still green at the top.

## What will be true

- [ ] The row a place writes is taken from the state that produced the PICTURE -- after the
      last rebuild the frame used, not during a pass the frame did not use.
- [ ] `BareTiles` names WHICH pass it counted where it prints, so a reader cannot attach it to
      the wrong one.
- [ ] Measurement that shows this is wrong: Heidelberg and Venice both render terrain and both
      must report 0 bare tiles once the row follows the frame. If either still reports bare
      tiles the world genuinely has a hole and THAT is the finding.
- [ ] Negative control: a place given a patience too short to stream anything reports its tiles
      bare and goes UNPREPARED, so the refusal still fires when it should.
