Type: bug
Parent: 1767
Area: sim, actor/body
Tags: physics, unasserted-number, drive

# The drive publishes where it slid, and how far

`tools/driver/APlannerFindsTheRoadFromMunichToHamburg` prints, on a run that PASSES all 41 of
its checks:

```
NOTE worst share of a contact's grip used = 2.27446642 of it
```

A share above 1.0 is a contact that asked for more force than friction could supply. Over
753.597 km the worst ask was 127 % past the circle -- and nothing in the proof says so.

## Corrected diagnosis (2026-08-24)

An earlier version of this item claimed two things that are FALSE, and both are withdrawn
rather than quietly edited:

| claimed | actually |
|---|---|
| "the force is applied anyway and the body is moved by a number no surface could produce" | `ShedAt` (src/actor/body/Shear.cpp:18-23) clamps to the friction circle: `share = HoldN / asked`, both components scaled, `Sliding = true`. The physics is right. `Ratio` is the ASK, published beside the clamped answer |
| "`where a contact first went past its limit = 0 km` and `WorstRatio = 2.274` cannot both be true" | they measure different things and both are true. `PastLimit` is the SUSPENSION's force limit (`out.PastLimit = contact.LimitN > 0.0 && out.LoadN > contact.LimitN`, src/actor/body/Contact.cpp:28), not the friction circle |

Filing a defect against correct code is worse than filing nothing, because it spends the
next reader's attention on a thing that works.

## What is actually wrong

`Rig::Ridden::Sliding` (src/actor/body/Rig.h:55) is COMPUTED on every tick and read by
**nobody**: `grep -rn 'Sliding' src/sim/` is empty. The tyres let go, the engine knows it,
and the fact dies inside the tick.

`WorstRatio` survives the tick (`DriveTick.cpp:148`) and reaches the drive's report, where it
is printed and never judged -- while every neighbouring number on that page IS asserted: the
distance, the arrival, the wheel leaving the carriageway, the deviation from the lane. A
measurement standing in a proof with no claim attached reads as a measurement the verdict
covers.

And unlike every other fault the drive reports, this one carries no PLACE. `OffTheRoad` has
`LeftTheRoadAtM`, `PastLimit` has `BrokeAtM`; sliding has neither, so "2.274 somewhere in
753 km" cannot be looked at.

## What will be true

- [ ] `Sliding` leaves the tick: the drive reports WHERE it first slid and HOW FAR it slid,
      the way it already reports where a wheel left the road
- [ ] `WorstRatio` carries a claim, not just a `Note` -- whatever the bar turns out to be, it
      is stated and derived
- [ ] A unit case in `test/unit/actor/body/` drives a contact past its circle and pins both
      the clamp and the report, with no network

## Comments

- 2026-08-24 -- found by closing board:1767, sharpened by reading the code it accused. The
  drive's trailer carried 2.274 through every previous run; it was never red because nothing
  ever asked it to be, and it was never LOOKED AT because nothing says where it happened.

---

## Repaid (2026-08-24)

`Ridden` carries what the tick computed and threw away:

| field | what it says |
|---|---|
| `Slid` | a tyre let go somewhere on this ride |
| `SlidFirstAtM` | the station it first let go at |
| `SlidM` | how far it travelled while sliding |
| `WorstRatioAtM` | the station the worst ask was made at |

`DriveTick.cpp:148-160` fills them; `Rig::Ridden::Sliding` finally has a consumer.

**Measured**, synthetic straight corridor, a car declaring `grip = 0.05` against the
plausible car's own declaration, no network:

| | grip 0.05 | the declared car |
|---|---|---|
| worst share asked | **39.013** | below 1 |
| where | 280.258 m | — |
| first let go | 50.239 m | **never** |
| slid | **230.362 m** | **0 m** |

The second column is the control the claim needs: the same corridor on the declared grip
slides nowhere, so the report is not simply always true.

- **Proving test**: `test/unit/sim/ADriveTickHoldsTheCarToTheDeclaredWorld` -- a fifth arm,
  no network, in the fast gate.
- **Negative control**: `if (read.Sliding)` disabled in the tick -> `FAIL **THE DRIVE
  PUBLISHES THAT IT SLID**`. Reverted.
- The Munich--Hamburg drive now prints the station and the distance beside the share, and
  asserts they are consistent with the ride.
- **Box 2 is honestly half-open**: `WorstRatio` now carries a claim (that it is reported with
  a place), but NOT a bar on its value. A bar of "never above 1.0" is wrong -- a tyre at its
  limit in a bend is ordinary driving -- and any other number would be fitted to the run that
  happened. What the repair buys is that the next round can SEE where 2.274 was asked for and
  over how much road, and decide the bar from a measurement instead of a guess.
- Gate 234/234.
