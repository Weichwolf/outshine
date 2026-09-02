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

**THE CONTROL FOUND A DETERMINISM DEFECT, WHICH IS WHAT A CONTROL IS FOR.** Twice, a sweep moved
Venice's digest while every changed line was semantically a no-op -- `held >>= 8` becoming
`held >>= 8u` on a `uint32_t`. Reproduced by reverting one header and re-applying it.

The build set no `-ffp-contract`, so clang may FUSE a multiply and an add, and WHETHER it does
depends on inlining. A source edit that changes nothing about the program still changes which
operations get fused, which changes the last bit of a float, which changes a pixel. The picture was
therefore a function of codegen decisions rather than of the declaration -- against the invariant
that says determinism is compulsory outside the shaders.

`-ffp-contract=off` in `test/run.sh` repairs it. Proved: with the header change and without it, the
digests are now IDENTICAL, and stable across repeated renders.

    control digests, before   Venice d99e6aaf   OldTown ecb4e513
    control digests, now      Venice c713d304   OldTown b0f9f7d7

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

## 2026-09-02: a fixit is a SUGGESTION about code the tool did not read

Half the tail is mechanical and `run-clang-tidy -fix` applies it in seconds, one check at a time.
That is the fast lane and it is where the round's one regression came from.

`modernize-loop-convert` rewrote `src/render/plan/Compiled.cpp`:

    for (size_t at = 0; at < Wanted.size(); ++at) { (void)Resolve(Wanted[at]); }
    for (auto &at : Wanted) { (void)Resolve(at); }                    // what it became

`Resolve` reaches `Want`, and `Want` pushes onto `Wanted`. **It is a worklist**: the index loop
re-read the bound every round because the list grows as it is walked. The range-for takes `begin`
and `end` once, so every resource pulled in by a dependency was dropped -- and a vector that
reallocates under an iterator is undefined behaviour besides. Every place refused to assemble, and
the audit said so correctly: *"nothing this plan requests reads what it draws into"*.

The check cannot see that `Resolve` reaches `Want`. **Nothing it emits is a patch until a person has
read the loop.** Every loop-convert of the round was re-read afterwards; the other five convert a
fixed array or a span nobody grows.

The repair is not a revert, it is a shape no fixit can mistake:

    size_t drained = 0;
    while (drained < Wanted.size()) { (void)Resolve(Wanted[drained++]); }

**AND THE GATE WOULD HAVE CAUGHT IT ON THE DAY.** `test/outshine` holds a render case per place and
all of them would have gone red. The commit went in without one, because the checks before it had
been textual and the habit had gone slack. THE GATE RUNS BEFORE THE COMMIT, not after it -- for
every check that touches a body, which after the textual ones is all of them.

## The second self-inflicted red, and it is the same shape as the first

`OsmField::Build` opened with two locals the centre tile was written into:

    uint32_t cx = 0, cy = 0;
    if (!TileIndex::Of(centre, Zoom_).TryXy(&cx, &cy)) { return 0; }
    CentreX_ = static_cast<int>(cx);
    CentreY_ = static_cast<int>(cy);

Turning `TryXy` into a `std::optional<TileId>` meant rewriting the two lines that READ `cx` and `cy`
into the members -- which I did -- and leaving the two lines forty rows further down that read them
again for the tile ring:

    const long tx = static_cast<long>(cx) + dx;

They were still declared, still initialised, still used, so nothing warned. Every OSM tile was then
fetched around tile (0, 0) -- open ocean off Africa -- and OldTown rendered with its buildings gone
and its four-second streaming stall gone with them, because there was nothing left to wait for. The
digest read 9a94420c and the frame time looked BETTER: p50 3.25 -> 1.77 ms, p99 5.5 s -> 232 ms.

**A number improving is not evidence.** The only thing that caught it was 545 119 of 921 600 pixels
differing by up to 138 of 255, and a wall at (519, 337) that had turned into grass. CLAUDE.md's
third question, exactly: if it draws, LOOK AT IT before believing any number.

Both of this round's reds are one habit: **I replaced where a value is WRITTEN and did not read the
rest of the scope for where it is READ.** The worklist was the same thing seen from the other side
-- the loop's shape carried a fact about `Resolve` that the diff did not show. The guard is not a
better checker, it is finishing the scope: after any change to how a value arrives, grep the
function for every use of it before building.
