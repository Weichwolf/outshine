Type: bug
Area: test, tools
Tags: gate, blind-spot, named-only, interface-drift

# A suite the gate does not run still compiles

`test/run.sh tools` at HEAD, before this item's repair:

```
9 tests: 2 PASS  1 FAIL  2 TIMEOUT  0 SIGNAL  4 BUILD  0 SKIP  0 UNPREPARED
```

**Four of the nine did not compile**, and the fast gate said nothing, because `tools` is
named-only and a suite the gate does not RUN it also does not BUILD.

What the four were:

| failure | cause |
|---|---|
| `Wgs84.h` not found (x4) | `LayerIncludes tools/viewer` (test/run.sh:152) never named `-Isrc/data`, though the suite's own source list compiles `src/clients/GltfStudio.cpp`, whose header includes it |
| `outshine/Scenario.h` not found | the same line never named `-Iinclude`, though the list compiles `src/clients/Live.cpp` |
| `Global(const std::string &) override` hides a virtual | **interface drift**: `board:1621` moved `Script::Host::Global` to `std::string_view`, and this consumer was never rebuilt |
| two `-Wshadow` errors | a `sheet` inside a `sheet` |

The third row is the one that matters. A signature change in `src/` broke a consumer in
`tools/`, the change shipped green, and the breakage sat until an unrelated item ran the
suite by name. `-Wall -Werror` is only a rule for translation units somebody compiles.

This is the compile-time twin of `board:1765`: there the gate was silent about conformance
it did not measure, here it is silent about code that does not build. A corpus needs
fetching; **a compile needs nothing**, so there is no excuse for the second.

## What will be true

1. Every source the tree declares in a suite COMPILES on every gate run, whether or not the
   gate runs that suite. Building is cheap; a translation unit nobody compiles is a rule
   nobody enforces.
2. The check names the suite and the diagnostic, so the failure reads like any other.
3. `LayerIncludes` cannot declare a source it does not give the includes for -- the same
   drift that hid `-Isrc/data` for a year.

## Comments

- 2026-08-23 -- found while closing board:1554, which needed `test/run.sh tools` to prove it
  had not made things worse. The four build failures were already there; the run before the
  change and the run after it were identical, which is how they were noticed at all.
- Repaired in place: `-Isrc/data -Iinclude` added to `tools/viewer`'s includes, the
  `Global` override moved to `std::string_view`, the two shadows renamed. `tools/viewer` now
  builds 4 of 4. Two still FAIL for want of the khronos GENERATOR corpus, which is a
  different absence (and one those two report as FAIL where they should report UNPREPARED --
  filed separately if it survives the next round).

---

## Repaid (2026-08-23)

`test/run.sh` gained `EverySourceStillCompiles`, run at the end of every fast gate: for each
source the gate STOOD ASIDE from, it compiles that source AND the extra sources its layer
declares, with that layer's own includes and toolchain, `-fsyntax-only` under the house
warning set. A failure prints the source, the layer, and the first four diagnostic lines,
and it counts toward `red` -- the gate exits 1.

It found three more of the same defect on its first run, in suites nobody had named recently:

```
test/harness/shared/render/Parity.cpp does not COMPILE under harness/render/khronos/generator
test/harness/shared/render/Parity.cpp does not COMPILE under harness/render/khronos/glTF
test/harness/shared/render/Parity.cpp does not COMPILE under harness/render/outshine/grown
  src/clients/GltfStudio.h:4:10: fatal error: 'Wgs84.h' file not found
```

`LayerIncludes` for those three named `-Isrc/clients` and compiled `Parity.cpp`, which
includes `GltfStudio.h`, which includes `Wgs84.h` from `src/data`. Repaired the same way:
`-Isrc/data -Isrc/scene -Isrc/scenario -Isrc/ui -Iinclude` added.
`harness/render/khronos/generator` went from `1 BUILD` to `102 tests, 0 BUILD` (all
UNPREPARED for want of its corpus, which is a different absence and board:1765's subject).

| | before | after |
|---|---|---|
| `test/run.sh tools` | 2 PASS 1 FAIL 2 TIMEOUT **4 BUILD** | 2 PASS 1 FAIL 2 TIMEOUT **0 BUILD** |
| `harness/render/khronos/generator` | **1 BUILD** | 102 UNPREPARED, 0 BUILD |
| fast gate | silent about all of it | `39 source(s) the gate did not run still compile, 0 do not` |

Cost: the whole check adds about 5 s to a 170 s gate, against 120 s of headroom.

- **Proving test**: the gate itself -- `run.sh: 39 source(s) the gate did not run still
  compile, 0 do not`, printed on every fast run, and a non-zero count is counted into `red`.
- **Negative control**: `-Isrc/data` taken back off the khronos/grown include line -> the
  gate printed the three diagnostics and **exited 1**. Restored, exit 0.
- Point 3 of the body -- `LayerIncludes` cannot declare a source it does not give the
  includes for -- is now enforced by construction: the check compiles exactly the sources the
  layer declares, with exactly the includes the layer declares, so a mismatch between the two
  is the failure.
- Gate 232/232.

---

## REOPENED (review 2026-08-24) — the check cannot see `tools/`, which is the tree that filed this item

The repair is real for `test/`, and blind to exactly the tree whose breakage is this item's
title, evidence table and third row ("a signature change in `src/` broke a consumer in
`tools/`, the change shipped green").

`EverySourceStillCompiles` (test/run.sh:487) iterates `$TESTS_ALL`. `TESTS_ALL` is assigned
at test/run.sh:756 from `$TESTS`, and `$TESTS` is built at test/run.sh:697 by
`find $TREES -name '*.cpp'`. `TREES` is:

```sh
TREES=test                                                  # test/run.sh:691
for named in $SUITES; do
  case "$named" in tools | tools/*) TREES="test tools" ;;   # test/run.sh:693
done
```

On a fast gate `$SUITES` is EMPTY (that is what makes it the fast gate, test/run.sh:745), so
the loop body never runs and `TREES` stays `test`. **No file under `tools/` is ever a
candidate, so none is ever compiled.**

The arithmetic confirms it:

| | count |
|---|---|
| `.cpp` under `test/` in a NAMED_ONLY layer | 36 |
| the check's own report | `39 source(s) … still compile` |
| `.cpp` under `tools/` | 12 |
| of those 12, compiled by the check | **0** |

36 named-only test sources plus three declared extra sources make the 39 the closure quotes
as its proof. Twelve tools sources — including
`tools/viewer/EveryRenderCaseTheBrowserShowsDrawsSomething.cpp` and
`tools/viewer/TheBrowserDrawsItselfWithTheEngineItShows.cpp`, the two the same commit
(99c4d735) repaired BY HAND for the `Script::Host::Global` signature drift — remain outside
every gate run.

So the negative control proves the check catches a broken `test/` layer. It does not, and
cannot, prove the case in the body: **repeat the `Global`-style signature change today and
`tools/` breaks green again.** The closure named a proving test for the half of the defect
that was easy.

### What will be true (unchanged in intent, sharpened in scope)

1. `TREES` for the compile-only walk is `test tools`, unconditionally — the RUN set stays
   `test` on a fast gate, the COMPILE set does not. Cheapest form: keep `TESTS` as is and
   build `TESTS_ALL` from `find test tools`, so every tools source lands in the
   "gate stood aside from" branch by construction.
2. The report distinguishes the trees, because a reader must be able to see that `tools/`
   was judged: `N test source(s) and M tools source(s) the gate did not run still compile`.
3. The negative control is the item's OWN case, not a substitute: move one `src/` signature
   the way `board:1621` moved `Script::Host::Global`, without touching `tools/`, and the
   FAST gate exits 1 naming the tools consumer. Revert.

---

## CLOSED (review 2026-08-24)

The reopening said `EverySourceStillCompiles` walks `$TESTS_ALL` while `TREES` stays `test` on
a fast gate, so 39 sources were checked and 0 of the 12 under `tools/` -- the tree whose
`Global` signature drift filed the item. b4e9ce04 fixes exactly that:

```sh
TREES=test
[ -z "$SUITES" ] && TREES="test tools"
```
— test/run.sh:691-692

Measured, reviewer worktree at HEAD b4e9ce04, `test/run.sh` with no suite named:

```
run.sh: 53 source(s) the gate did not run still compile, 0 do not
```

39 → 53 = the 12 `.cpp` under `tools/` plus the 2 extra sources their layers declare
(`tools/viewer/parts/Chrome.cpp`, `tools/host/CurlTransport.cpp`, test/run.sh:270-273). Every
tools layer resolves through `LayerIncludes`/`LayerToolchain`/`LayerGroups`
(test/run.sh:152-154, 164-165, 235-236), so the walk does not die on an undeclared layer, and
the `-Wall -Werror -Wpedantic` set that filed this item now stands over `tools/` on every
default gate. Point 1 holds, point 2 held from the first repair, point 3 is what found the
three `Parity.cpp` breakages.

Two residues, named rather than hidden, neither of them this item:

- The check is `-fsyntax-only`. A call-site signature drift -- the exact defect that filed this
  item -- is caught; a definition that vanishes and breaks only the LINK is not.
- `TREES` stays `test` when a suite is NAMED, so `test/run.sh unit/core` still does not compile
  `tools/`. That is the fast-path bargain and it is the right one: the DEFAULT gate is the gate
  that gates.

Negative control: revert test/run.sh:692 and the trailer reads 39 rather than 53, with
`tools/viewer` back outside the warning set.
