Type: defect
State: open
Area: test
Tags: measured, vocabulary, audit

# The subject audit sees the COMPOUNDS C++ actually names things with

**Benchmark** — the claim itself already holds the right structure and cites the right pair: Unreal
keeps wheeled movement in a PLUGIN outside the engine module, RAGE keeps `CVehicle` in the game
layer above `fwEntity`. `TheEngineNamesNoSubject` was written to hold that line and its declared
counts may only fall. **The structure is right and the WALK is blind**, which is worse than no
claim, because a green claim is read as evidence.

## What it sees, and what is there

The walk is `grep -rowh '\bWord\b'`. C++ does not name things in bare words -- it names them in
COMPOUNDS -- and a word boundary after `Road` fails on `RoadStation`. Measured over `src/` and
`include/` with the claim's own exclusion of `generators/`:

    word        \bWord\b   Word*   the row says
    Terrain            0     214   {"Terrain", 0, "the engine knows no terrain"}
    Road               0      40   {"Road", 0, "a road is what OSM calls one"}
    Building           7      31   {"Building", 6, "all six are the PARTICIPLE"}
    Tree              20      23
    Street        NOT ASKED    59
    Bridge        NOT ASKED    29
    Tunnel        NOT ASKED     8

**`Terrain` declares 0 and the tree holds 214** -- `TerrainGrid` 69, `TerrainField` 40,
`TerrainMesh` 34, `TerrainBytes` 31, `TerrainTiles` 24, `TerrainSource` 7, `TerrainLoader` 6,
`PoolTerrain` 3. The row's own comment asserts the rule holds while the tree breaks it in every
compound. This is `CLAUDE.md`'s named trap, **a measure that cannot see**, and it is the second time
it has been paid for here.

## And where the words actually are, which is the fair half

                 Terrain   Road   Street
    src/engine         0     32       17
    src/world        213      1       37
    src/render         0      0        0
    src/scenario       0      0        0
    src/client         0      0        0
    include            0      0        0

**The door and the renderer hold the rule completely.** It is broken where the work happened. That
is worth stating plainly: the rule is not aspirational and not unreachable -- four tiers keep it at
zero today.

## What will be true

- [ ] The walk matches a compound, not a bare word, and the declared counts are re-derived against
      the walk that can see. Every count RISES on the day the walk is fixed, and that rise is the
      repair rather than a regression -- it is recorded in the commit that fixes the walk
- [ ] `Street`, `Bridge`, `Tunnel`, `Junction`, `Kerb` and `Lane` join the list. They are the
      subjects this round of work introduced and none of them was ever asked about
- [ ] `Terrain`'s row is DECIDED rather than left at a false zero: either a height field is a LAW
      and the row states that with its 214, or it is a subject and the 214 have to move. A row
      that declares zero against 214 decides nothing
- [ ] Negative control: adding `RoadStation` to a file under `src/engine/` makes the claim go RED.
      Today it does not, which is how this was found

## What this does NOT cover

Whether a word is a subject at all. A walk cannot tell `Building` the participle from `Building` the
house -- the existing row says so and carries its reason, which is the right answer and stays. The
fix is that the walk SEES the word, not that it judges it.
