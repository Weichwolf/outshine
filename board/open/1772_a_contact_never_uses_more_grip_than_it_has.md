Type: bug
Parent: 1767
Area: sim, actor/body
Tags: physics, unasserted-number, drive

# A contact never uses more grip than it has

`tools/driver/APlannerFindsTheRoadFromMunichToHamburg` prints, on a run that PASSES all 41 of
its checks:

```
NOTE worst share of a contact's grip used = 2.27446642 of it
```

A share above 1.0 is a contact demanding more force than friction can supply. 2.274 is not a
tyre near its limit; it is a tyre being asked for **127 % more than exists**, which means the
force is applied anyway and the body is moved by a number no surface could produce.

`tools/driver/APlannerFindsTheRoadFromMunichToHamburg.cpp:138` prints it and nothing judges
it:

```cpp
Note("worst share of a contact's grip used", rode.WorstRatio, "of it");
```

Every neighbouring number on that page IS asserted -- the distance, the arrival, the wheel
leaving the carriageway, the deviation from the lane. This one is a measurement standing in a
proof with no claim attached, which is the shape `board:1765` names on the suite scale: a
number published beside a green verdict reads as a number the verdict covers.

## History, which sharpens it rather than excusing it

Before `board:1767` the same figure was 1.480, and the drive ended at 113.990 km. With the
crest bounded the car survives the whole 753.597 km -- and the worst ratio ROSE to 2.274.
That is not a regression introduced by 1767: it is 639 km of road that no run had ever
reached, carrying a worse case than the first 114 km held. The number got worse because the
measurement finally covers the route.

## What will be true

- [ ] `WorstRatio` is asserted, not merely printed: a contact may not be asked for more force
      than its declared grip supplies
- [ ] Where it is exceeded is published the way the other faults are (`where a contact first
      went past its limit` is already computed and printed as 0 km, which is now a FALSE
      zero -- it means "never" and prints like a station)
- [ ] The physics either clamps the force at the friction circle and lets the contact slide,
      or refuses -- silently applying an impossible force is neither
- [ ] A unit case in `test/unit/actor/body/` drives a contact past its circle and pins the
      behaviour, with no network

## Comments

- 2026-08-24 -- found by closing board:1767. The drive's own trailer carried the number
  through every previous run; it was never red because nothing ever asked it to be.
- `where a contact first went past its limit = 0 km` prints beside it. Both readings cannot
  be true: either no contact passed its limit, or the worst one passed it by 127 %. One of
  the two is measuring something other than what it says.
