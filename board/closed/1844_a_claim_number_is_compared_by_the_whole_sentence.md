Type: bug
Parent: 1841
Area: test
Tags: claims, identity, truncation

# A claim number is compared by the whole sentence it carries

`board:1841` landed `harness/claims/AClaimNumberNamesOneClaim` and renumbered 94 sentences so
that "107 numbers became 206, all distinct". The walk that proves it does not compare the
sentence. It compares the first sixty characters of it:

```cpp
test/harness/claims/AClaimNumberNamesOneClaim.cpp:89
      saying[number].insert(whole.substr(space + 1, 60));
```

`IV.15` carries THREE different sentences at HEAD, all in one file:

```
test/harness/claims/TheCorpusIsPrunedByOneRunnerOnly.cpp:50   "... and by no other -- vacuous here, because no corpus was fetched ..."
test/harness/claims/TheCorpusIsPrunedByOneRunnerOnly.cpp:76   "... and by no other -- not judged here, because another nest held the claim ..."
test/harness/claims/TheCorpusIsPrunedByOneRunnerOnly.cpp:145  "... and by no other, so a second checkout reads the same bytes and removes none of them"
```

All three open with `the shared corpus is pruned by the runner holding its claim ` -- 60
characters exactly -- so the walk sees one sentence and says so. Its own log, this review's
run:

```
NOTE distinct claim numbers = 206 numbers
NOTE numbers carrying more than one sentence = 0 numbers
```

Measured over the same corpus with the truncation removed: 206 numbers, and `IV.15` carries
three. It is the ONLY number that does, which is what makes the blindness cheap to fix and
exactly wide enough to hide the one live case -- and that case is `board:1843`'s own repair,
landed the same session as the guard that should have caught it.

Two numbers with no origin sit beside it:

```cpp
test/harness/claims/AClaimNumberNamesOneClaim.cpp:89   whole.substr(space + 1, 60)   -- why 60?
test/harness/claims/AClaimNumberNamesOneClaim.cpp:109  CHECK(covers > 200, ...)      -- why 200?
```

`60` is the width of the blind spot and nothing states it. `200` is a floor under 209 that will
stop meaning anything the day a case is deleted.

## What will be true

- [x] The walk compares the WHOLE joined sentence; no truncation, or a truncation whose width is
      derived and stated.
- [x] `IV.15`'s three sentences are resolved: either the two conditional framings stop being
      `Covers` lines (see board:1845, which is the same code from the other side), or each gets
      its own number.
- [x] The `covers > 200` floor is derived from something -- the count of case sources, or
      dropped, because a claim that only asserts "more than a corner" asserts nothing a walk
      over a fixed tree needs.
- [x] Proving test: the existing walk, with the truncation gone. Negative control: HEAD's three
      `IV.15` sentences -> red, printing `IV.15 carries 3 different sentences over 1 cases`.

## Comments

- 2026-08-25 -- filed by the hourly review. `board:1841` is the right instrument built with a
  comparison that cannot see the thing it forbids; the item's own two open boxes (the
  catalogue) would make this unspellable rather than merely detected.

## Sharpened 2026-08-25 -- two boxes done, one standing, and the standing one is the number

`9117e1c9` did the work the first two boxes name, and it did it properly:

```cpp
test/harness/claims/AClaimNumberNamesOneClaim.cpp:92
      saying[number].insert(whole.substr(space + 1));
```

No truncation. `IV.15`'s three sentences became one: `TheCorpusIsPrunedByOneRunnerOnly.cpp:50`,
`:77` and `:145` now carry the same `Covers` text and the circumstance moved into the `NOTE`
beside it, which is the resolution this item preferred.

The third box is untouched:

```cpp
test/harness/claims/AClaimNumberNamesOneClaim.cpp:112
  CHECK(covers > 200, "the walk found this tree's claims, not a corner of them");
```

`200` still has no origin. It is a floor under a number the walk itself computes and prints one
line earlier (`Note("Covers statements found", (double)covers, "claims")`, `:107`), and the two
values it must separate -- "the walk read the tree" and "the walk read a corner of it" -- are not
what a bare `200` distinguishes: it is satisfied by 201 claims found where the tree carries 209,
and it stops meaning anything the day a case is deleted. The derivable form is the count of
sources the walk visited against the count that carry `Covers(`, which are `walked` (`:79`) and
`covers` (`:88`) and both already exist.

This item stays open on that one box.

## Closed 2026-08-25 -- the floor is a witness, not a number

```cpp
const size_t versioned = Lines(Ask("git ls-files 'test/*.cpp' 'test/**/*.cpp'")).size();
...
CHECK(walked == versioned, "**AND THE WALK READ THE TREE, NOT A CORNER OF IT**: ...");
```

`covers > 200` compared the walk against a number somebody typed. The replacement compares it
against git's index -- a count that comes from OUTSIDE the walk, moves with the tree, and cannot
be satisfied by "more than a corner". Its run: `sources walked = 263`,
`sources git carries under test/ = 263`.

Negative controls, both run:

| control | walked | git | verdict |
|---|---|---|---|
| an unversioned `ASmuggledSource.cpp` dropped beside the cases | 264 | 263 | **FAIL** at `:127` |
| the iterator narrowed to `test/harness` -- the walk reads a corner | 44 | 263 | **FAIL** at `:127` |

The first is the one that shows the change: a source that carries no `Covers(` leaves `covers`
untouched, so the old `covers > 200` stayed green over a tree the walk was no longer reading
truthfully. The second would have failed under either form -- 44 harness sources do not carry
200 claims -- and is recorded because it is the failure the old floor was reaching for and could
only catch by accident of magnitude.
