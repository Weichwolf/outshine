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

- [ ] Every tick computes the wheel-to-edge clearance and folds it into a histogram beside the
      deviation's, so the drive reports p50/p95/p99 AND the minimum of the clearance.
- [ ] The case asserts the clearance's low tail against a declared floor rather than asserting
      the binary "did not leave" -- a drive that arrives with 1 mm to spare is not the same
      verdict as one that arrives with 300 mm, and today they print identically.
- [ ] Proving test: the clearance floor asserted in
      `apps/driver/test/APlannerFindsTheRoadFromMunichToHamburg`. Negative control:
      `kLagsToCover` back to 1.0 -> the minimum clearance goes negative at km 113.990 and the
      case names the station BEFORE the binary check fires.
