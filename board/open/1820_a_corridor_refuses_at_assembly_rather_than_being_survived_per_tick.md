Type: bug
Area: sim
Tags: refusal, hot-path, mirror, drive

# A corridor whose bands disagree refuses when it is laid, instead of being survived per tick

`Corridor` carries six parallel bands (`src/sim/CorridorLay.h:24-25`) of which three --
`FineAside`, `FineEdge`, `FineLaneHalfM` -- are baked in one loop at one resolution
(`src/sim/CorridorLay.cpp:318-325`) and must therefore always have the same length. Nothing
states that, and this session's work made the tick tolerate its violation instead:

```cpp
src/sim/DriveTick.cpp:18   [[nodiscard]] double At(const std::vector<double> &band, size_t at, double whenEmpty) {
                             if (band.empty()) { return whenEmpty; }
                             return band[at < band.size() ? at : band.size() - 1];
                           }
```

Six call sites, each branching on `band.empty()` on the frame path, to defend against a
corridor that `LayCorridor` cannot produce. CLAUDE.md's rule is the opposite ordering: refusal
at assembly over runtime checks, and bounded terms on the frame path.

## And the band it was added for is filled by no test in the tree

```
$ grep -rn FineLaneHalfM src/ test/ apps/
src/sim/CorridorLay.h:25          the declaration
src/sim/CorridorLay.cpp:39,322    the one writer
src/sim/DriveTick.cpp:49          the one reader
apps/driver/test/APlannerFindsTheRoadFromMunichToHamburg.cpp:287   a Note
```

`test/unit/sim/ADriveTickHoldsTheCarToTheDeclaredWorld.cpp:80-83` hand-builds its corridor and
fills `LaneHalfM`, `FineAside` and `FineEdge` -- **not** `FineLaneHalfM`. So in the fast gate
the tick's lane band is empty, `At(laneHalfM, here, 0.0)` returns 0, `halfRoomM` is negative,
and the whole `StrayedAtM` attribution added by `board:1767` is dead code under the mirror.
`out.LeftLaneM` is 0 m in every unit run.

The `whenEmpty` default is what makes that silent. Without it the mirror would have crashed the
day `FineLaneHalfM` was introduced, and the twin would have been completed instead of the tick
being taught to survive a half-built corridor.

## What will be true

- [ ] `LayCorridor` is the only place a `Corridor` becomes usable, and it refuses -- with the
      reason -- if the fine bands are not one length. The tick indexes them without a branch.
- [ ] The three fine bands are one contiguous structure with one extent (a
      `std::mdspan<double, extents<size_t, dynamic_extent, 3>>` over one buffer, or one struct
      of three values per station), so "the same length" is a type property rather than a
      convention three `assign` calls happen to keep.
- [ ] `test/unit/sim/ADriveTickHoldsTheCarToTheDeclaredWorld` builds a corridor through
      `LayCorridor`'s own invariant, not by filling members by hand, so a band added to the
      product cannot be missing from the twin.
- [ ] Proving test: a unit case that hands the tick a corridor with mismatched bands and expects
      a refusal. Negative control: the `whenEmpty` helper restored -> the case is green against
      a corridor that is missing a band, and names which one.
