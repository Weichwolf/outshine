Type: bug
State: open
Area: build, all
Tags: measured, owner, gate

# clang-tidy reports NOTHING, and the baseline is 0 because it is the target

**Benchmark** — Unreal builds with `bWarningsAsErrors` and gates a static-analyser pass; a warning
does not travel. RAGE cannot be read on this, so it does not get a column. **This tree already
states the rule itself** -- CLAUDE.md's craft list says `-Wall -Werror -Wpedantic` and *"a warning
is an error"* -- and the analyser was the one place that rule was not held: a SHRINKING BASELINE let
4402 findings stand as long as the number never rose. The owner has removed that compromise. The
baseline reads 0 and the gate is RED until it is true.

## Measured 2026-09-01, after the config was made strict

`.clang-tidy` had thirteen checks switched off. Five were off for taste and stay off with their
reason. Eight were off without one, or with "stage two" as the reason, and are back on. What that
exposed:

    12 979 findings, and the largest single one names the worst thing in the tree:
    src/engine/Picturing.cpp:542  'Grounds' has cognitive complexity of 1264 (threshold 25)

| check | findings | what the fix is |
|---|---|---|
| `modernize-avoid-c-style-cast` | 3667 | mechanical, `--fix` does it |
| `readability-magic-numbers` | 2299 | a named constant WITH its derivation, per number |
| `modernize-avoid-c-arrays` | 1310 | a judgement per record: GPU layout or not |
| `modernize-use-designated-initializers` | 1053 | mechanical |
| `misc-non-private-member-variables-in-classes` | 800 | design: a public data member is an invariant nobody can hold |
| `misc-include-cleaner` | 796 | mostly mechanical |
| `readability-isolate-declaration` | 407 | mechanical |
| `bugprone-easily-swappable-parameters` | 331 | strong types, or the parameters change |
| the rest | ~1300 | mixed |

Roughly six thousand are mechanical and roughly five thousand need a decision. **The count is the
schedule and it may only fall.**

## How it is tested while it falls

`make lint` is the measure. The RENDER check while sweeping is **Venice and OldTown only** -- the two
fastest places -- because a mechanical sweep over thousands of casts has to be proved not to move a
pixel, and proving that twice quickly beats proving it nine times slowly. The full nine run before
the item closes, not during it.

## What will be true

- [ ] `make lint` reports 0 findings against a baseline of 0.
- [ ] `Grounds` no longer appears at all, because it no longer exists at that size -- board:2091's
      three passes are what replaces it, not a smaller version of the same function.
- [ ] Every check still switched off carries its count and its reason on the line above it.
- [ ] Proving case: Venice and OldTown render byte-identical across each mechanical sweep, and all
      nine places once at the end.
- [ ] Negative control: put one `(float)` cast back and require the gate to go RED at 1.
