Type: issue
Area: test
Tags: map, claims, process
Depends: 1824

# A CURRENT row's sentence is measured, and not only its quoted line

`board:1824` landed `harness/claims/EveryColourCitesALineThatSaysIt`: it parses every
`` `<quoted code>` (File.h:NN) `` pair out of CLAUDE.md, opens the file and asserts the line
holds the quote. The gate is green on it this round -- and TWO rows of the CURRENT class
diagram state things about the code that are false, because what a row ASSERTS is not what the
claim checks.

## `CorridorLay`'s row denies a line one above the line it cites

```
CLAUDE.md:292   The product it lays no longer holds bands that must agree by convention:
                `std::vector<Station> Fine;` (:43) is one extent ...
```

The quote at `:43` resolves, so the claim is green. One line above it:

```cpp
src/sim/CorridorLay.h:42   std::vector<double> RoadM, HalfWidthM, LaneHalfM, AsideM;
```

Four bands, still filled, still shipped, and `Fine` is derived from them by an index division
at `src/sim/CorridorLay.cpp:295-297`. The row's sentence is the exact opposite of the header.
Filed separately as `board:1828`.

## `DriveTick`'s row carries a number the code contradicts

```
CLAUDE.md:293   The copy is gone and the struct is 2472 -> **424 bytes measured**
```

```cpp
src/sim/DriveTick.cpp:18   static_assert(sizeof(Ridden) == 440, "sizeof(Ridden)");
```

`board:1818` added `LeastClearanceM` and `LeastClearanceAtM` (`src/sim/DriveTick.h:38-39`) in
the same session that wrote the 424, so the map went stale within the hour that produced it.
The measurement is IN THE CODE, as a `static_assert`, and the map still carries a hand-copied
number beside it -- which is the failure mode `board:1824` exists to prevent, one level up.

## The shape of the gap

| what a row carries | checked by | today |
|---|---|---|
| a `file:line` citation | `EveryColourCitesALineThatSaysIt` | green, and it works |
| a NUMBER stated in the prose | nothing | 424 against a `static_assert` saying 440 |
| an ASSERTION about the code ("no longer holds bands") | nothing | false |

A map whose citations resolve and whose sentences are wrong is more dangerous than one with a
stale line number, because the stale line number announces itself.

## What will be true

- [ ] A number stated in CLAUDE.md that a `static_assert` also states is read FROM the
      `static_assert`: the claim greps `src/` for `static_assert(sizeof(T) == N` and asserts the
      map's number for `T` is `N`, naming both. A map that states a size the tree does not
      assert is red.
- [ ] Where a row asserts the ABSENCE of something ("no longer holds", "nothing spells"), the
      row carries the grep that proves it, in the form the claim can run -- a negative citation
      is a pattern and a count, not a sentence.
- [ ] The two rows above are corrected: `CorridorLay`'s says what the header holds, `DriveTick`'s
      cites `src/sim/DriveTick.cpp:18` rather than restating its number.
- [ ] Proving test: `EveryColourCitesALineThatSaysIt` extended, negative control = the 424
      restored -> red, naming 440 and the assert's file:line.

## Comments

- 2026-08-24 -- filed by the hourly review, which is the diagrams' only writer and therefore
  the author of both defects. The mechanism that would have caught them is the one this item
  asks for.

**Both rows corrected.** `CLAUDE.md`'s `CorridorLay` row said the product *"no longer holds
bands that must agree by convention"* while `CorridorLay.h:42` still declared four of them; it
now says the product holds no band parallel to its stations, which board:1828 made true rather
than merely asserted. The `DriveTick` row said `424 bytes measured` where
`src/sim/DriveTick.cpp:18` asserts 440; it now names the `static_assert` as the measurement
instead of carrying a hand-copied number beside it.

The item's own reading stands and is the sharper half: `EveryColourCitesALineThatSaysIt` proves
the CITATION resolves, never that the SENTENCE around it is true. Both rows were green through
the defect.
