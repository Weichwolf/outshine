Type: bug
Parent: 1835
Area: actor/path
Tags: layout, simd, width

# The crossing grid's entry is one record of declared width

`board:1835`'s repair is correct -- a crossing is counted once, proven by a fixture that
reported it nine times. The shape it took is three parallel arrays, and one of them is not a
declared width:

```cpp
src/actor/path/Wayfinding.cpp:345
  std::vector<uint32_t> inCell(holds[cells], 0);
  std::vector<long> squareX(holds[cells], 0), squareY(holds[cells], 0);
```

`long` is platform-width. Everywhere else this file counts in `uint32_t`. On this device the
entry costs 4 + 8 + 8 = **20 bytes across three allocations** where it carries a segment index
and a grid coordinate pair.

The inner loop reads all three, per pair, in the O(n^2)-within-a-bucket sweep:

```cpp
src/actor/path/Wayfinding.cpp:366-369
        const size_t theirs = inCell[two];
        if (segWay[mine] == segWay[theirs]) { continue; }
        if (squareX[one] != squareX[two] || squareY[one] != squareY[two]) { continue; }
```

Three streams gathered at two independent indices to answer one question. The fixture in the
mirror sweeps 24 000 segments; the shipped Munich--Hamburg route is larger. CLAUDE.md's bar:
*contiguous, one-width, pointer-free layouts; a layout that blocks SIMD or forces
gather/scatter is a defect even when correct.*

The square is a pair of small integers -- `cellDeg` is twice the mean segment extent, so a
degree-scale network has grid coordinates in the tens of thousands -- and the only operation on
it after filing is equality. A single 64-bit key packing (x, y), or a 12-byte record
`{uint32 Seg; int32 X; int32 Y;}` in ONE array, makes the test one compare on one stream and
drops the entry to 12 bytes. The `(long)std::floor(...)` cast at `:318` already has an
unbounded-coordinate hazard that a declared width would force into a refusal.

## What will be true

- [x] The grid's entry is ONE contiguous record of declared width -- segment index and square
      packed together -- so the inner loop reads one stream, not three.
- [x] The square comparison is one operation on one value (a packed key), not four loads and
      two compares.
- [x] The coordinate cast names its bound: a network whose extent over `cellDeg` leaves the
      declared width refuses rather than wrapping.
- [x] Proving test: `test/unit/actor/path/ANetworkIsWovenFromWaysThatShareNoIdentity` already
      publishes `PairsTested`, `FullestCell` and `Found` -- the same three numbers must hold on
      all four fixtures across the change, and `sizeof` the entry gets a `static_assert` beside
      it.

## Comments

- 2026-08-25 -- filed by the hourly review. The correctness half of `board:1835` is verified by
  its own negative control: reverting the two guard lines in a worktree reproduces
  `crossings it reports = 9`. This is the layout half.

## Closed 2026-08-25 -- 20 bytes over three arrays became 8 in one

```cpp
struct Filed {
  uint32_t Square = 0;
  uint32_t Seg = 0;
};
static_assert(sizeof(Filed) == 8);
static_assert(std::is_trivially_copyable_v<Filed>);
```

The square is now an INDEX into a grid whose width is derived
(`wide = floor((eastLon - westLon) / cellDeg) + 2`), so the inner loop's test is
`ours.Square != yours.Square` -- one compare on one stream where it was four loads and two
compares across three.

The unbounded cast the item named is bounded by the same move: a network needing more squares
than a 32-bit index holds REFUSES, and the refusal carries its reason through
`std::expected<Swept, std::string_view>`. `DriveAssembly` says
`REFUSED the crossing sweep cannot grid this network` instead of sweeping an empty result.

| number | before | after | what it is |
|---|---|---|---|
| `Found` (sweep fixture) | 3600 | 3600 | the answer; geometric |
| `PairsTested` | 86284 | 86284 | the cost; geometric |
| `FullestCell` | 30 | 16 | bucket occupancy -- a property of the MIXER, and the new one spreads better |
| entry width | 4 + 8 + 8 over three allocations | **8, one** | `static_assert`ed |

**Two findings the controls produced, both landed.**

1. **board:1835's fixture no longer catches board:1835.** Its defect was a dedup comparing the
   BUCKET, and it reported nine because nine of the long segment's squares collided in one
   bucket. Collisions are a property of the mixer, so rehashing the grid disarmed the fixture
   without touching the rule: with BOTH square tests removed it still reported 1. A hash-free
   fixture stands beside it now -- two long diagonals whose boxes share hundreds of squares meet
   in exactly one, and the sweep must ask WHERE the meeting fell.
2. **The square filter before `SegmentsMeet` pruned nothing measurable** in any existing fixture
   -- `PairsTested` was 86284 with and without it. It prunes 538 pairs in the diagonals fixture.
   `Swept::PairsPruned` publishes the count, so the filter's worth is a number.

Proving test: `test/unit/actor/path/ANetworkIsWovenFromWaysThatShareNoIdentity`.
Negative controls, all four run:

| control | result |
|---|---|
| the 32-bit bound removed | `FAIL ...:472 A GRID THAT WILL NOT FIT ITS DECLARED WIDTH REFUSES` |
| `Filed` given a fourth 4-byte field | **compile error** -- `static_assert sizeof(Filed) == 8` |
| the meeting-point test removed | `crossings the diagonals report = 256`, `FAIL ...:444` |
| the square filter removed | 1 crossing still, `PairsPruned` accounts for the 538 pairs it stops |

The fourth is recorded as what it is: the filter is COST, not correctness, and the run says so
rather than the code implying it.
