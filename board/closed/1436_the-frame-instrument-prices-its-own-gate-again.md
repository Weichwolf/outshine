Type: bug
Area: harness
Tags: perf, instrument

**The frame instrument prices its own gate again**

Three of the four arms of `TheFrameCostIsPublishedAgainstItsOwnFloor` were refused before they rendered,
so the instrument's whole reason for existing -- *one more shadow ray per fragment must cost more than
the floor* -- had not been measured at all.

```
ARM fill refused: vertex 0 sits 4.065543 m along the view axis,
                  inside the near plane of 13.758289 m this placement declares
```

**The refusal was right and it was about the PLACEMENT.** `OrbitAt` moves the eye to `scale` times the
framing distance -- 0.25 for the filling arms, which is the whole point of them -- and copied the framed
placement's clip planes unchanged. So the camera stood at a quarter of the distance with a near plane
derived for the full one, and the subject sat inside it.

**It is `board:1433`'s rule one suite over: a clip range is a depth window and never a crop.** There the
far plane had to follow the grid; here the near plane has to follow the eye. The window is re-derived
from the distance the orbit actually stands at and the radius the subject actually has, which is the same
expression the framing rule uses.

| | before | after |
|---|---|---|
| arms rendered | 1 of 4 | **4 of 4** |
| the gate | never reached | `second-ray = 1.801 ms, floor-sum = 0.252 ms, **resolved**` |

## A margin invented to cover a case, which then covered it wrongly

The first reading clamped the near plane to `distance * 0.5` for standpoints inside the subject's own
bounding sphere. [MEASURED] it refused `normal-tangent` in the sibling instrument at **0.489680 m against
a plane of 0.496809 m** -- a number chosen to be safe that was not. `distance - radius` is already the
bound: no vertex lies further from the centre than the radius, so none can be nearer than that along any
axis. **A spherical subject ATTAINS the bound at one vertex** and the studio refuses a vertex sitting
exactly on the plane, so the plane goes at half the clearance -- and where the eye is inside the sphere
there is no positive bound to be had, so it hands the question to the renderer's own constant, which is
the number `ClearsNearPlane` already falls back to.

## The floor did its job on the way past, and that is worth recording

The first green run was not green: `PRICED second-ray=1.813 ms floor-sum=1.949 ms **resolved=no**`. One
repeat of five had wandered -- p50 3.903 ms against 2.083, 2.244, 2.048 and 2.019 -- after a session of
back-to-back Cycles renders, and that single outlier put the instrument's own noise above the effect it
was pricing. **It refused to claim a result it could not resolve.** A minute later, unchanged: 1.801 ms
against a floor of 0.252 ms, resolved.

*A performance number that is not a distribution over repeats would have reported 1.8 ms both times, and
one of those reports would have been a measurement of a warm phone.*

## The warm-device outlier has now been seen three times, and it is always the same arm

[MEASURED] across this session, one repeat of five on the `fill` arm has three times come back at roughly
double the other four -- p50 3.903, 3.907 and once more against a neighbourhood of 1.98 to 2.24 -- after
long stretches of continuous GPU work. **`fill` is the fill-bound arm**, and a fill-bound frame is what a
warm phone-class GPU slows first.

**The instrument refuses correctly every time** -- `resolved=no` -- and a few minutes of quiet is enough
for it to come back at `resolved=yes` with a floor five to eight times smaller. *Recorded so a later
round does not read a `resolved=no` as a regression: it is a measurement of the device's thermal state,
which is exactly what a distribution over repeats is for.*

## A fourth observation, and it is the arm drifting rather than one repeat

[MEASURED] at the END of a full suite -- fourteen minutes of continuous GPU work -- the `fill` arm's five
repeats came back at p50 **3.073, 4.178, 3.856, 3.719, 2.382 ms** against a standalone 1.98 to 2.15. The
whole arm is warm, not one repeat of it, and the gate collapsed to `second-ray=0.524 ms` against a floor
of 1.952: `resolved=no`.

**Standalone, minutes later, the same build resolves at 1.850 ms against a floor of 1.761.** So the frame
suite's gate is not resolvable when it runs last, and `test/run.sh`'s ordering is what decides that.

## What the shape of a repair would be, stated and not taken

**A wide floor and a small effect are two different verdicts and this gate gives them one.** The floor is
the spread across repeats, so:

- floor **narrow** and effect small -> the effect is genuinely absent, and that is a FAILURE
- floor **wide** -> the instrument yields no number, which `CLAUDE.md` calls *not measurable* rather than
  *not yet measured*, and a refusal is the honest verdict

`resolved=no` already carries that distinction in the log and the CHECK flattens it into a failure. **A
gate that cannot fail while it is warm would be worse**, which is why the discriminator above is written
down rather than applied: it is the owner's call whether the frame suite runs first, cools down, or
refuses as unmeasurable.
