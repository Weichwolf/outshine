Type: bug
Area: sim
Tags: measured, telemetry, drive

# The drive publishes how close a wheel came to leaving, not only whether it left

`apps/driver/test/APlannerFindsTheRoadFromMunichToHamburg.cpp:297` asserts the binary:

```cpp
CHECK(rode.LeftTheRoadAtM <= 0.0, ...)
```

and `board:1767` closes on it. It holds -- and it holds with **no margin anybody measured**.

## The numbers the closure itself publishes

| | |
|---|---|
| the reserve the corridor keeps between aim and edge | 0.7195 m |
| p99 of the deviation over 2 791 050 stations | 0.3125 m |
| **the worst single sample** | **0.8895 m** |

The worst deviation is **1.24x the reserve**. The wheel stays on the road at that station only
because the aim there is not at the clamp `room = fineEdge - 0.5*carWidth - budgetM`
(`src/sim/CorridorLay.cpp:340`); where the aim IS at the clamp, 0.8895 m of deviation puts a
wheel 0.17 m past the carriageway, which is exactly what km 113.990 was before `board:1814`.

So the drive arrives because the two extrema -- worst deviation and aim-at-the-clamp -- do not
coincide on THIS route. Nothing in the tree says they will not coincide on the next one, and
`board:1524` asks for a hundred.

## What is missing is a length, and the tick already holds every term of it

`Ridden` carries `LeftRoomM`, `LeftHalfWidthM`, `LeftAcrossM` and `LeftAsideM`
(`src/sim/DriveTick.h:57-75`) -- all four written only in the branch where a wheel HAS left. The
same four evaluated every tick give the clearance

```
clearanceM = fineEdge(here) - 0.5*carWidth - |offset|
```

which is the quantity that decides whether a route survives, and it is unpublished.

## What will be true

- [x] Every tick computes the wheel-to-edge clearance and folds it into a histogram beside the
      deviation's, so the drive reports p50/p95/p99 AND the minimum of the clearance.
- [x] The case asserts the clearance's low tail against a declared floor rather than asserting
      the binary "did not leave" -- a drive that arrives with 1 mm to spare is not the same
      verdict as one that arrives with 300 mm, and today they print identically.
- [x] Proving test: the clearance floor asserted in
      `apps/driver/test/APlannerFindsTheRoadFromMunichToHamburg`. Negative control:
      `kLagsToCover` back to 1.0 -> the minimum clearance goes negative at km 113.990 and the
      case names the station BEFORE the binary check fires.

## Repaid, and the number was worth having (2026-08-24)

Every tick computes `fineEdge - |offset| - 0.5 * carWidth` and folds it into a histogram beside
the deviation's. The drive publishes it:

```
NOTE the least clearance a wheel ever had to the carriageway edge = 0.160302 m
NOTE where that was                                              = 643.525 km
NOTE the clearance at p01                                        = 0.5475 m
NOTE at p05                                                      = 0.7175 m
NOTE at p50                                                      = 1.3175 m
```

**16 centimetres, at km 643.525** -- and the drive that reported *"no wheel ever left the
carriageway"* had never said so. The closest point of a 742 km drive is not km 113.990 at all;
that station was merely the one where the margin ran out under the old rate.

## The bar is a relation between two measured distributions

```
NOTE the room left at p01 against the deviation spent at p99 = 1.752 x
```

The room a wheel has at its worst hundredth must exceed the deviation the pilot spends at its
worst hundredth. Neither side is a constant somebody liked: both are measured on the drive being
judged, and the statement is that the road leaves more than the car uses. If the two tails meet,
a wheel leaves on a road nobody would call narrow.

- **Proving test**: `apps/driver/test/APlannerFindsTheRoadFromMunichToHamburg`, 5/5 at
  `--timeout 900`.
- **Negative control**, run: `kLagsToCover` back to 1.0 ->

  ```
  NOTE the least clearance a wheel ever had to the carriageway edge = -0.168919 m
  FAIL apps/driver/test/APlannerFindsTheRoadFromMunichToHamburg
  ```

  **-0.169 m** -- and that is exactly the 0.1689 m by which `board:1812` measured the deviation
  overrunning the reserve. The two arithmetics close on each other, which is what says the
  clearance is the same defect seen from the other side.
- **Recorded, not hidden**: the p01-against-p99 relation did NOT go red in that control (3.22x),
  because a drive that stops at km 114 measures both tails over a seventh of the route. That
  claim is landed and not yet proven falsifiable, the same residue `board:1800` carries, and
  the control that would move it is a route where the road is genuinely narrow rather than a
  car that leaves it early.
- Gate 260/260.
