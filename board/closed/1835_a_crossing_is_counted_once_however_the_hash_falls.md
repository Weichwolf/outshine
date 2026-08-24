Type: bug
Area: actor/path
Tags: correctness, regression, spatial-index, measured
Regresses: 1831

# A crossing is counted once, however the hash falls

`board:1831` replaced the dense cell grid in `Network::Crossings` with a spatial HASH and
closed on *"A crossing is still kept by exactly one bucket, so no pair is double-counted."*
That sentence is false, and the tree now miscounts the number `AssembleDrive` publishes as
*"places two ways cross in plan without sharing a node"* -- the number every grade separation,
bridge and portal reconstruction will be built on.

## The mechanism

```cpp
src/actor/path/Wayfinding.cpp:306   const auto squareOf = [&](double atLon, double atLat) { ... }   // exact, injective
src/actor/path/Wayfinding.cpp:311   const auto bucketOf = [&](std::pair<long, long> square) {
src/actor/path/Wayfinding.cpp:312     const uint64_t mixed = (uint64_t)(square.first * 73856093L ^ square.second * 19349663L);
src/actor/path/Wayfinding.cpp:313     return (size_t)(mixed % (uint64_t)cells);
src/actor/path/Wayfinding.cpp:334     overSquares(seg, [&](std::pair<long, long> square) { inCell[filled[bucketOf(square)]++] = (uint32_t)seg; });
src/actor/path/Wayfinding.cpp:369     if (bucketOf(squareOf(atX, atY)) != cell) { continue; }
```

A segment whose bounding box spans several SQUARES is inserted once **per square**. Two of its
squares can hash to the SAME bucket, so the segment stands in that bucket **twice**. The pair
loop then enumerates the pair `(mine, theirs)` once per copy, and the ownership test at `:369`
asks which BUCKET the intersection falls in -- never which SQUARE. Every copy answers the same
bucket, so every copy is accepted and the same crossing is appended again.

With the dense grid this was impossible: `cellOf` was injective, so distinct squares were
distinct cells and a segment stood in each of them exactly once.

## The measurement

One north-south way and one east-west polyline, sharing no node, crossing exactly once:

```cpp
Network n(1.0, 6371008.8);
const double a[4] = {50.85, 10.00005, 51.15, 10.00005};       // one long segment
n.Lay(std::span<const double>(a, 4), 4.0, 0.06, 2, 400.0);
std::vector<double> b;                                          // 60 short segments
for (int s = 0; s <= 60; ++s) { b.push_back(51.0); b.push_back(9.99 + 0.02 * (double)s / 60.0); }
n.Lay(b, 4.0, 0.06, 2, 400.0);
```

```
segments=61 Found=9 PairsTested=288 FullestCell=41
  0: 51.0000000, 10.0000500  ways 0/1
  1: 51.0000000, 10.0000500  ways 0/1
  ... nine identical rows ...
```

**Nine copies of one crossing, at identical coordinates.** Swept over 4 500 fixtures (the long
segment's half-extent 0.01 to 3.00 deg, 20 to 300 short segments), each holding exactly one
crossing:

| build | fixtures answering something other than 1 | worst |
|---|---|---|
| `3db9e6d6` -- the dense grid `board:1831` replaced | **0 of 4 500** | -- |
| `e7de9c1e` -- the hash | **396 of 4 500 (8.8 %)** | **11 copies of one crossing** |

The regime that triggers it is not exotic: it is one long segment among many short ones, which
is what a motorway link beside a digitised urban way IS. `cellDeg` is `2 * mean reach`
(`Wayfinding.cpp:302`), so a segment 100x the mean spans ~50 squares in each axis, and
`cells = 2 * segments + 1` (`:303`) is far smaller than the square count that segment alone
occupies. Collisions are then near-certain rather than rare.

The XOR hash makes it worse than a birthday estimate: for the vertical segment `x` is CONSTANT
and only `y` varies, so `(x*73856093) ^ (y*19349663)` mod `cells` walks a correlated sequence
rather than a uniform one -- 9 collisions out of ~29 squares into 123 buckets.

## Two more things the same lines carry

- `swept.FullestCell` (`Wayfinding.cpp:337-340`) is the max over BUCKETS and is reported as
  *"segments in the cell that held the most"* (`src/sim/DriveAssembly.cpp:205`). A bucket holds
  unrelated squares, so the number no longer measures spatial occupancy -- it measures hash
  load. The name and the number disagree.
- `square.first * 73856093L` at `:312` is signed `long` multiplication with no bound on
  `square.first`; a small `cellDeg` over a wide span is signed overflow, which is undefined
  behaviour, not a wrap.

## What will be true

- [ ] A crossing is appended by the entry whose SQUARE contains it, not by the entry whose
      bucket the square hashes to: the square (or its 64-bit key) travels beside the segment
      index in `inCell`, and `:369` compares squares. A hash bucket may then hold a mix of
      squares and the answer stays exact.
- [ ] `FullestCell` measures a square's occupancy, or it is renamed to what it measures.
- [ ] The hash mixes before it truncates and cannot overflow a signed type.
- [ ] Proving test: the fixture above in
      `test/unit/actor/path/ANetworkIsWovenFromWaysThatShareNoIdentity` -- one long segment
      among sixty short ones, `CHECK(Found == 1)`. Negative control: HEAD -> 9. The sweep over
      the 4 500-fixture family belongs beside it as a Note, because a single point proves the
      defect and the family proves the regime.

## Comments

- 2026-08-25 -- filed by the hourly review, which is also `board:1831`'s author. The item's
  four requirements are all met; the closure's ARGUMENT that no pair is double-counted was
  reasoning about a grid it had just stopped using. The existing fixtures do not catch it
  because both of them are uniform: the flat one has 160 segments of one length, the clustered
  one 800 of one length plus a single outlier that crosses nothing.

**Closed.** Each entry in the bucket table carries the square it was filed under:

```cpp
src/actor/path/Wayfinding.cpp:344   std::vector<long> squareX(holds[cells], 0), squareY(holds[cells], 0);
src/actor/path/Wayfinding.cpp:362   if (squareX[one] != squareX[two] || squareY[one] != squareY[two]) { continue; }
src/actor/path/Wayfinding.cpp:372   const auto met = squareOf(atX, atY);
src/actor/path/Wayfinding.cpp:373   if (met.first != squareX[one] || met.second != squareY[one]) { continue; }
```

A pair whose two entries sit in different squares is skipped BEFORE the intersection test, and a
crossing is kept only by the square that contains it. That restores what the dense grid gave for
free, and it is cheaper: pairs that only share a bucket now fall out early.

| | before the regress | with it | after this repair |
|---|---|---|---|
| the reviewer's fixture | 1 | **9** | **1** |
| pairs tested there | -- | 288 | 32 |
| the 60x60 grid | 3600 in 142 171 pairs | 3600 in 98 954 | 3600 in **86 284** |
| pairs per crossing | 39.5 | 27.5 | **23.97** |

Proving test: `unit/actor/path/ANetworkIsWovenFromWaysThatShareNoIdentity` -- the reviewer's
fixture, one long north-south way against sixty short east-west segments. Negative control: the
bucket comparison restored -> **9 crossings** and the case red, which is the reviewer's number
arriving.

**And the shipped route did not show it.** Munich--Hamburg reports 17 474 crossings before and
after, because tile-quantised segments are short and rarely span more than one square; the
pair count fell from 1 417 749 to 1 118 277 and the answer did not move. That is the second
defect this session whose only measured route happens to hide it -- board:1830's divisor was
the first, where `Fastest()` equals `TopMs()` on a motorway. A route that passes is not a
proof that the term is right, and both were caught by a fixture built to make the term bite.
