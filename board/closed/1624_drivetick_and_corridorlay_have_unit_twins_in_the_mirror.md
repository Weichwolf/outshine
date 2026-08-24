Type: bug
Area: sim
Tags: tests

**DriveTick and CorridorLay have unit twins in the mirror**

test/unit/sim/ holds three cases; none compiles src/sim/DriveTick.cpp or src/sim/CorridorLay.cpp
(test/run.sh:146 lists only `src/sim/Rigging.cpp`;
ASyntheticRoadIsRiddenToArrivalInMilliseconds includes Drive.h and SpeedProfile.h, not
DriveTick.h). The regression net is absent exactly where this hour cut:

- f1c48fe3 changed DriveTick.cpp:46 — the gravity vector now reads
  `-stood.Envelope.GravityMs2`. A sign or magnitude regression there is caught by NOTHING in
  the fast gate; only the sporadic Munich/Kyoto drives would see it.
- CorridorLay.cpp carries the climb gate (506-511, fixed under 1615/1616) and the width/lane
  tables — also without a unit consumer.

CLAUDE.md already carries the debt ("LayCorridor and DriveTick stay amber until their own unit
proofs deepen"); this item is its enforcement. Demanded: unit/sim twins that tick DriveTick
over a synthetic corridor (proving the gravity vector's direction against the declared g, the
OffTheRoad verdict, the arrival) and lay CorridorLay over a synthetic route; test/run.sh:146
compiles both under the one include truth. 1581 move 2(e) may rename them — the twins move
with the `git mv`, they do not wait for it.

---

Closed (2026-08-22, reviewer): task 1626 closed in board/closed/. Proof in the tree:
test/run.sh:146 compiles `src/sim` ENTIRE for unit/sim (DriveTick.cpp and CorridorLay.cpp
included); test/unit/sim/ADriveTickHoldsTheCarToTheDeclaredWorld.cpp ticks a synthetic
corridor to arrival under 9.80665 AND 1.62 m/s2 -- the equilibrium seat pins the gravity
vector's sign -- and proves the OffTheRoad verdict;
test/unit/sim/ALayRefusesASceneItCannotDrive.cpp covers CorridorLay's entry. Gate green.

---

## REOPENED by the hourly review, 2026-08-24 — the deferred half came due

The closing note deferred exactly one thing: *"CorridorLay's own numeric twin remains with the
1624 issue if the reviewer holds it open."* It was closed with that sentence in it, and
`520f1748` then landed a new **refusal** in `CorridorLay` -- and nothing in the tree can see it.

```
$ grep -rn 'LayCorridor' test/
(nothing)
```

`test/unit/sim/` mirrors `Rigging.cpp`, `DriveTick.cpp` and `DriveAssembly.cpp`'s entry, and
carries no case that calls `LayCorridor`. `ALayRefusesASceneItCannotDrive` -- named in 1626's
closure as covering "CorridorLay's entry" -- includes `DriveAssembly.h` and never reaches
`LayCorridor`'s body. Compiling a translation unit into the suite is not a twin; the mirror
rule is that *behaviour a commit changed has a test that would have caught the old behaviour*.

The cost is measured, not hypothetical: the guard at `src/sim/CorridorLay.cpp:117-124` refuses
a corridor with no corner (`TightestRadiusM == 0.0`) and cannot fail on one with a corner. See
**board:1791**. `unit/actor/path` is 14/14 PASS with that defect standing, and `unit/sim` has
nothing to say about it.

`CorridorLay.cpp` is 550+ lines carrying, uncovered by any unit case: the width and lane
tables, the grade limit walk (`:213-232`), the climb gate (`:515-521`), the height-knot
assembly (`:490-508`), the profile step derivation (`:527-529`) and now the radius guard.

- [ ] `test/unit/sim/ACorridorIsLaidOverASyntheticRoute` (or the name the queue prefers): a
      hand-built `Path::Route` and a synthetic `GroundStream`, `LayCorridor` called directly,
      asserting the derived numbers -- profile step from the tightest radius, the climb
      refusal against a declared drivetrain, and the straight-route case board:1791 names.
- [ ] Negative control: the radius guard's comparison inverted -> red in the fast gate.

CLAUDE.md still carries the debt in the CURRENT class diagram (*"`LayCorridor`, `AssembleDrive`
and `DriveTick` stay amber until their own unit proofs deepen"*), so the map is honest and the
board was not.

---

## The blocker, measured (2026-08-24)

`LayCorridor` cannot be reached by a unit case today, and the reason is one line of its
signature:

```cpp
[[nodiscard]] bool LayCorridor(const Path::Route &route, Ground::GroundStream &ground, ...)
```

`GroundStream` (`src/ground/TerrainLoader.h:47-71`) is constructed from a `TilePool &` --
threads, a content store, fetched tiles. `grep -rln 'GroundStream' test/ tools/` finds
**exactly one** user in the whole tree, `tools/driver/stills/...`, and it needs the network.
There is no synthetic one, and building one means building a pool.

**And the dependency is far wider than the use.** Everything `LayCorridor` asks of the ground:

```
$ grep -n 'ground\.' src/sim/CorridorLay.cpp | sed 's/.*ground\.\([A-Za-z]*\).*/\1/' | sort -u
At
PostM
```

Two queries -- `At(lat, lon) -> GroundSample` and `PostM(latDeg) -> double`. It takes a class
that owns a thread pool to ask two questions that are pure functions of a coordinate.

So the twin this item asks for is blocked on a narrower door, not on test effort: while
`LayCorridor` spells `GroundStream`, a unit case must bring a tile pool with it, and the fast
gate cannot. The first box therefore depends on:

- [ ] `LayCorridor` takes the two queries it uses, not the class that happens to hold them --
      an interface a synthetic ground can satisfy, which is also `local reasoning only` and
      `minimal public API` applied to a door that currently demands the world to ask about a
      metre of it.

Recorded rather than attempted: the narrowing is a signature change through
`DriveAssembly.cpp:232` and the drive suites, and it is the right shape rather than a quick
one. What board:1791 needed from this item -- a case that catches a guard refusing a straight
road -- now stands in `test/unit/actor/path/ACorridorIsFittedThroughVerticesItMayNotLeave`
instead, over `Fit` directly, because `Fit` takes spans and needs no world at all.

---

## Closed by the hourly review, 2026-08-24 — the blocker fell and the twin stands

Both boxes of the reopening are paid, and the named blocker with them.

**The door narrowed to the two queries it uses.** `src/core/GroundQuery.h` is a new interface
carrying exactly `At(lat, lon) -> GroundSample` and `PostM(latDeg) -> double`; `GroundStream`
is `final : public GroundQuery` (`src/ground/TerrainLoader.h:48`), and the signature this item
measured as the blocker now reads

```cpp
[[nodiscard]] bool LayCorridor(const Path::Route &route, const GroundQuery &ground,   // CorridorLay.h:34
```

`WaterField::Ingest` and `BuildingField::Build` took the same narrowing in the same round
(`board:1806`), so no unit case in the ground or sim layer needs a `TilePool` any more.

**The twin exists and reaches the body.** `test/unit/sim/ACorridorIsLaidOverASyntheticRoute.cpp`
(364 lines) builds a hand-made `Path::Route`, a ten-line `FlatGround final : public GroundQuery`
with a declared slope, and calls `LayCorridor` directly -- the profile step, the class-minimum
count, the straight-route case `board:1791` named, and the refusal arms. `grep -rn 'LayCorridor'
test/` is no longer empty, which was this item's own measurement of the defect.

**Its children are closed**: `board:1626` and `board:1792`, both in `board/closed/`.

Gate at `959a0d23`, isolated worktree: `257 tests: 255 PASS 1 FAIL 1 SIGNAL` -- the two
non-green verdicts are `board:1808` (`board/active/` absent from the tree) and have nothing to
do with this item; `unit/sim` is green entire.

`CLAUDE.md`'s CURRENT class map is corrected in the same round: `CorridorLay`'s amber no longer
rests on "until their own unit proofs deepen", which was this item's debt, but on the shape of
its parameter list, which is `board:1610`'s.
