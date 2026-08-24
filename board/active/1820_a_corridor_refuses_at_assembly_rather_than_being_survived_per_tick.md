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

- [x] `LayCorridor` is the only place a `Corridor` becomes usable, and it refuses -- with the
      reason -- if the fine bands are not one length. The tick indexes them without a branch.
- [x] The three fine bands are one contiguous structure with one extent (a
      `std::mdspan<double, extents<size_t, dynamic_extent, 3>>` over one buffer, or one struct
      of three values per station), so "the same length" is a type property rather than a
      convention three `assign` calls happen to keep.
- [x] `test/unit/sim/ADriveTickHoldsTheCarToTheDeclaredWorld` builds a corridor through
      `LayCorridor`'s own invariant, not by filling members by hand, so a band added to the
      product cannot be missing from the twin.
- [x] Proving test: a unit case that hands the tick a corridor with mismatched bands and expects
      a refusal. Negative control: the `whenEmpty` helper restored -> the case is green against
      a corridor that is missing a band, and names which one.

**Closed.** The three fine bands are one type:

```cpp
src/sim/CorridorLay.h:26   struct Station { double AsideM, EdgeM, LaneHalfM; };
src/sim/CorridorLay.h:32   std::vector<Station> Fine;
src/sim/CorridorLay.h:43   void Bake(double lengthM);
src/sim/CorridorLay.h:45   [[nodiscard]] bool Laid() const;
src/sim/CorridorLay.h:46   [[nodiscard]] const Station &At(double alongM) const;
```

"The same length" is now the extent of one array rather than a convention three `assign` calls
kept. `At` clamps once; the six `band.empty()` branches are gone from the frame path along with
the `whenEmpty` helper that made the missing band silent, and the tick refuses an unlaid
corridor once at entry:

```cpp
src/sim/DriveTick.cpp:39   if (!way.Laid()) { return out; }
```

The seven separate recomputations of `(size_t)(at.AlongM / fineM)` -- one of them with its own
hand-written clamp at the old :155 -- are one call each, and `fineM` no longer appears in the
tick at all.

`Straight()` in the twin builds through `Bake`, so the case that filled `FineAside` and
`FineEdge` by hand and forgot `FineLaneHalfM` cannot be written any more: there is one call and
it produces every field. The `StrayedAtM` attribution board:1767 added is live under the gate
for the first time.

Proving test: `unit/sim/ADriveTickHoldsTheCarToTheDeclaredWorld`, 25 checks -- the unlaid-corridor
block asserts the refusal. Negative control: `if (!way.Laid()) { return out; }` deleted -> FAIL
at :273 against a corridor no lay ever baked.

The rebuild is behaviour-neutral and that is measured, not assumed: on the shipped
Munich-Hamburg drive the least clearance is 0.160301892 m and the worst deviation
-0.889506378 m, bit-identical to the run before it, over 742.636 km.
