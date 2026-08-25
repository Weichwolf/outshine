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

- [ ] The grid's entry is ONE contiguous record of declared width -- segment index and square
      packed together -- so the inner loop reads one stream, not three.
- [ ] The square comparison is one operation on one value (a packed key), not four loads and
      two compares.
- [ ] The coordinate cast names its bound: a network whose extent over `cellDeg` leaves the
      declared width refuses rather than wrapping.
- [ ] Proving test: `test/unit/actor/path/ANetworkIsWovenFromWaysThatShareNoIdentity` already
      publishes `PairsTested`, `FullestCell` and `Found` -- the same three numbers must hold on
      all four fixtures across the change, and `sizeof` the entry gets a `static_assert` beside
      it.

## Comments

- 2026-08-25 -- filed by the hourly review. The correctness half of `board:1835` is verified by
  its own negative control: reverting the two guard lines in a worktree reproduces
  `crossings it reports = 9`. This is the layout half.
