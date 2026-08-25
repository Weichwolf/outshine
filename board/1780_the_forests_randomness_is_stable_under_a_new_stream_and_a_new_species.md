Type: issue
State: open
Area: generators
Tags: determinism, content, optimisation

# The forest's randomness is stable under a new stream and a new species

A region's wood is a pure function of `(zoom, x, y)` and nothing else -- that is what makes it
a place a player can return to and a picture an oracle can bound. At HEAD it is a pure function
of `(zoom, x, y, kStreamsPerCell, Stems_.size())`, and the last two move whenever the code or
the asset directory grows. **Adding a stream or adding a species must move no tree that was
not asked to move.**

## What the code does now

`src/generators/Forest.cpp:52` indexes the per-cell entropy by a **stride multiply**:

```
const uint64_t place = region.Seed(index * kStreamsPerCell);          // :53
const uint64_t draw  = region.Seed(index * kStreamsPerCell + 1);      // :63
const Stem &stem = Stems_[(size_t)(region.Seed(index * kStreamsPerCell + 3) % Stems_.size())]; // :82
const float size = SizeFactor(region.Seed(index * kStreamsPerCell + 2), stem.HeightSigma);     // :83
```

`045e315d` raised `kStreamsPerCell` from 3 to 4 (`src/generators/Forest.cpp:13`). Under a
stride, cell *i*'s streams were `{3i, 3i+1, 3i+2}` and are now `{4i, …, 4i+3}`: **every cell
but cell 0 draws different numbers than it drew before that commit.** Every jitter, every
density draw, every trunk size in every region on Earth changed, and the commit that changed
them says nothing about it -- it reports the gain (1 species -> 31) and not the cost.

The second axis is worse because it is data, not code. The species index is
`Seed(...) % Stems_.size()`. `src/assets/world/species/` holds 31 `.json` files today.
**Dropping a 32nd file into that directory reindexes every tree in the world** -- a content
addition, made by an artist, silently rewrites every wood the engine has ever grown.

## What is NOT wrong -- measured, so the item stays honest

The draw itself is uniform. `Region::Seed(stream) = Mix(Seed_ ^ Mix(stream))` with the
splitmix64 finaliser (`src/generators/Region.cpp:12-17`), over 200 000 cells of one region:

| species | expected/bin | min | max | chi² | df |
|---|---|---|---|---|---|
| 3 | 66 667 | 66 388 | 66 944 | **2.32** | 2 |
| 31 | 6 452 | 6 256 | 6 597 | **25.25** | 30 |
| 64 | 3 125 | 2 977 | 3 291 | **56.56** | 63 |

Every chi² sits below its df: the modulo is unbiased at these N (2⁶⁴ mod 31 leaves a relative
bias of ~2⁻⁵⁹). **The defect is stability, not distribution.** The reviewer's probe stands in
the scratch tree, not the repo; reproduce with `Mix(seed ^ Mix(i*4+3)) % N`.

## What must be true

- [ ] **A stream is keyed by its purpose, not by a stride.** `Seed((purpose << 40) ^ index)`
      or an equivalent disjoint keying, so a fifth stream disturbs nothing that the first four
      drew. The stride multiply at `Forest.cpp:53,63,82,83` goes.
- [ ] **A species is chosen by a key that does not move when the catalogue grows.** Selection
      by a stable per-species hash (the species' own name or id mixed with the cell), or an
      explicit weight table, rather than `% Stems_.size()`. Adding beech to a directory that
      already holds oak leaves every oak standing.
- [ ] **The forest publishes its mix.** `Forest::Note` (`Forest.h:30-33`) counts six refusals
      and one high-water mark and says nothing about WHICH species stood. A scenario suite
      cannot today assert that a 31-species world is not a stand of one. Publish per-species
      placements, or at minimum the count of distinct species placed.
- [ ] **A proving test pins the numbers across the change, not within one process.**
      `test/unit/generators/SameRegionSamePlacement.cpp:498-513` grows the same ground twice in
      one process and compares -- which a pure function passes by construction and which cannot
      see either axis of this defect. The proof owed is: a recorded digest of one region's wood,
      unchanged when a stream is added and unchanged when a species is added.
- [ ] The per-tree `% Stems_.size()` is a 64-bit division on the generation path; a stable
      keying that lands on a mask or a multiply-shift is the form.

Reference: RAGE's decisionless pools and Decima's runtime placement both key entropy by
(cell, purpose) precisely so that a new consumer of randomness is additive. A stride is the
shape that forbids it.
