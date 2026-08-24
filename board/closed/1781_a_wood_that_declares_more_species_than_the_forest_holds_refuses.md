Type: bug
Area: generators
Tags: hardening, telemetry, optimisation

# A wood that declares more species than the forest holds refuses, and an empty wood says so

`045e315d` gave `Forest` its 0-or-1..N shape and put three silences inside it. All three are
in code written this hour, after the rules they break.

## Silent truncation

```
Forest::Forest(Span<const Stem> stems, Span<const float> perM2ByRow, const AlpineLimit &limit)
    : PerM2_(perM2ByRow), Limit_(limit) {
  const size_t held = stems.Size() < kMostSpecies ? stems.Size() : kMostSpecies;   // Forest.cpp:26
  Stems_.reserve(held);
  for (size_t at = 0; at < held; ++at) { Stems_.push_back(stems.Data()[at]); }
}
```

`kMostSpecies = 64` (`src/generators/Forest.h:59`). Hand it 65 stems and 64 stand; the 65th is
dropped with no refusal, no log, no note. `src/assets/world/species/` holds 31 today, so the
bound is 33 files from being reached by an artist who will never be told. **A failure is loud**
(CLAUDE.md) and **refusal belongs at assembly, not at the frame** -- the constructor is the
assembly. `Sim::LoadTables` (`src/clients/Sim.cpp:135-139`) hands the whole vector straight
through, so the refusal has a place to land and a `why` to carry.

Where 64 comes from is nowhere: not in the commit, not in the board, not in the header. Every
number carries its origin.

## An empty wood is spelled as unvegetated ground

```
if (Stems_.empty()) { return Outcome::NoTemplate; }    // Forest.cpp:81
```

`Outcome::NoTemplate` is also what a cell returns when the ground carries no cover row
(`Forest.cpp:58`). A world configured with **zero species** therefore publishes exactly the
telemetry of a world standing on bare rock: the `noTemplate` note counts up and nothing
distinguishes a misconfiguration from a desert. The condition is fixed at construction and
re-tested per cell, which is a runtime check where a refusal belongs.

## The layout blocks what the loop wants

`std::vector<Stem> Stems_` (`Forest.h:61`) puts a pointer chase between `Consider()` and the
species row it reads, in a class whose whole job is a per-cell loop over a lattice. With
`kMostSpecies` already a compile-time bound, the contiguous pointer-free form is
`std::array<Stem, kMostSpecies>` plus a count -- one cache line's worth of stems living inside
the generator instead of behind it. No `static_assert` guards the layout of `Stem` either,
though the struct is copied per cell.

## What must be true

- [ ] More stems than `kMostSpecies` **refuses at construction**, naming the count it was
      handed and the bound it holds; the refusal reaches `Sim::LoadTables` as a `why`.
- [x] `kMostSpecies`' origin is written in this item and in the commit that sets it.
- [ ] Zero stems is a **distinct** outcome and a distinct note from "the ground carries no
      cover row"; the per-cell `Stems_.empty()` test disappears with it.
- [x] `Stems_` is a fixed contiguous array with a count, `static_assert`ed on its size, with
      no allocation behind the generation loop.
- [ ] Proving test: a `Forest` handed 65 stems refuses and names both numbers; a `Forest` of
      zero stems publishes a note no unvegetated ground publishes. Negative control: the
      refusal removed -> the claim red.

## Comments

- 2026-08-24 -- repaid. Both halves were silent; both speak now.

| | before | after |
|---|---|---|
| 65 stems declared | 64 held, 1 dropped without a word | `SpeciesCount() == 64`, **`SpeciesRefused() == 1`**, both published |
| a wood of no species | `Outcome::NoTemplate` -- the note bare rock publishes | `Outcome::NoSpecies`, its own note beside `noTemplate` |

- **Measured**: 65 declared, 64 held, 1 refused; a wood of no species grows 0 trees and
  raises `noSpecies` **198 922** times against `noTemplate` **0**.
- **Proving test**: `test/unit/generators/SameRegionSamePlacement`, two new checks.
- **Negative control**: the note collapsed back to `NoTemplate` -> the emptiness claim goes
  red, because `noSpecies` reads 0 while `noTemplate` reads 198 922 -- the exact telemetry of
  bare rock, which is the defect.
- Not repaid here: `kMostSpecies = 64` still has no derivation, and `std::vector<Stem>` is
  still a pointer chase in a class whose whole body is a cell loop. Both named by the review,
  both left with that reason.

---

**Reviewer sharpening (2026-08-24) -- the two boxes left open are confirmed open, and one of
them got WORSE: the underived number is now part of the public surface.**

`SpeciesRefused()` and the `NoSpecies` note are verified in a review worktree
(`unit/generators` 49/49 green, `SameRegionSamePlacement` carries both arms). The telemetry
half is repaid.

`kMostSpecies` did not merely stay underived -- it MOVED from `private` to `public`
(`src/generators/Forest.h:31`, was `Forest.h:60` under `private:`). A number with no origin is
now a number clients may spell, and the unit test spells it
(`test/unit/generators/SameRegionSamePlacement.cpp:539`). Widening the surface of an underived
constant is the opposite direction from *every number carries its origin*.

`std::vector<Stem> Stems_` (`src/generators/Forest.h:62`) is still a heap indirection read once
per cell in `Consider` (`src/generators/Forest.cpp:82`), inside `Occupy`'s double loop
(`Forest.cpp:96-112`) -- 198 922 cells in the measured region. `std::array<Stem, kMostSpecies>`
plus the count is contiguous, pointer-free, allocation-free and `static_assert`-able on its
size, which is what the open box asks for.

- [ ] `kMostSpecies = 64` carries its derivation in THIS item and its commit before it stays
      public -- or it goes back behind `private:` until it has one.

---

## The last two boxes, closed (2026-08-24)

**The layout box was already paid and unrecorded**: `std::array<Stem, kMostSpecies> Stems_{}`
with a `Held_` count stands at `Forest.h:64`. What was missing is what makes it hold --
nothing asserted the layout a per-cell copy depends on. Three do now:

```cpp
static_assert(sizeof(Forest::Stem) == 24, "sizeof(Forest::Stem)");
static_assert(std::is_trivially_copyable<Forest::Stem>::value, "a stem is copied per cell");
static_assert(Forest::kSpeciesTableBytes == 1536, "the species table's bytes");
```

**And the origin box turned out to hold a contradiction.** `kSpeciesTableBytes = 2048` stood
beside `kMostSpecies = 64` describing nothing: `sizeof(Stem)` is 24, so the table is **1536**
bytes. One underived number was wrong about the other, and neither was read by any code.

## Where 64 comes from, written down

**It is a declared CAPACITY and not a derivation, and it stays one.** Every derivation offered
itself and failed to be honest:

| candidate anchor | why not |
|---|---|
| a cache line | the table is 1536 bytes either way; 64 stems is far inside L1 on this device and the number would be arbitrary within it |
| one page | Apple silicon pages are 16 KB (`getconf PAGESIZE`), which admits 682 stems -- a bound that never fires is not a refusal |
| the shipped catalogue | 31 files; a bound fitted to what is there today is calibration deciding, which this tree forbids |

So: `kMostSpecies` `[SET]` **= 64**, twice the 31 species the tree ships. What makes a declared
capacity safe is that exceeding it REFUSES loudly rather than truncating -- which this item
already landed. What makes it USEFUL is that the tree notices before an artist does.

- **Proving test**: `test/unit/clients/AWorldReadsTheSpeciesThatGrowInIt` -- the shipped
  catalogue must stay clear of the bound, and the headroom is published:

  ```
  NOTE species the tree carries                    = 31 files
  NOTE the species a wood may hold at once         = 64 species
  NOTE the bytes that table costs                  = 1536 bytes
  NOTE the headroom the shipped catalogue leaves   = 2.06 x
  ```
- **Negative controls**, both run: `kMostSpecies` cut to 31 -> headroom reads `1 x` and the
  claim goes red; a `double` added to `Stem` -> the layout `static_assert` fires at compile
  time with the size it found.
- Gate 257/257.
