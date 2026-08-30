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
