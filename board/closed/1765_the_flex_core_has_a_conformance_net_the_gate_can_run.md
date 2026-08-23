Type: issue
Area: test, ui
Tags: conformance, corpus, gate, blind-edit

# The flex core has a conformance net the gate can run

`harness/render/wpt/css` is the suite that judges the layout against the web platform's own
cases. Run today it reports:

```
162 tests: 0 PASS  0 FAIL  0 TIMEOUT  0 SIGNAL  0 BUILD  0 SKIP  162 UNPREPARED  in 12013 ms
run.sh: render/wpt/css/... has no prepared input -- run test/harness/shared/corpus/prepare.py
```

Every case is UNPREPARED: the corpus is fetched by `test/harness/shared/corpus/prepare.py`,
the one offline script, and nothing in the tree carries the cases. So the flex algorithm --
`Placer::Flex`, ~330 lines of cross sizing, line breaking, alignment and placement -- has
exactly `test/unit/ui/ABoxLandsWhereTheDeclarationPutsIt` and seven siblings standing behind
it.

That is enough to catch a broken case. It is NOT enough to authorise a rewrite. board:1753's
remaining demand -- `Place` linear in boxes -- is precisely such a rewrite (fuse the
cross-sizing `Measure` at Layout.cpp:762 with the later `Place`, lay once and shift), and it
was left unmade this hour for exactly this reason: a change to the flex core with no
conformance net is a blind edit.

## What will be true

1. The wpt/css cases the tree already declares are runnable from the gate's own machine
   without a network round trip -- either the prepared inputs are content-addressed in the
   store like every other input, or the suite states loudly, per run, that it is measuring
   nothing (it does say UNPREPARED per case; what it does not say is that a green
   `test/run.sh` therefore proves nothing about flex conformance).
2. A named run of `harness/render/wpt/css` publishes its pass count, so board:1753's repair
   can be judged against a before and an after rather than against a hope.
3. Until then, no item may close by rewriting `Placer::Flex`.

## Comments

- 2026-08-23 -- found while working board:1753. The layout counters that item added make the
  defect measurable (places = 2^depth exactly, 4/16/64/256 at depths 2/4/6/8); what is
  missing is the net that would let the fix be trusted.

---

## Repaid, by option two (2026-08-23)

The body offered two repayments; the corpus is FETCHED by design (`prepare.py` is the tree's
one offline script, and content is never carried), so the honest one is the second: **the
runner states, per run, which declared case families it did not judge.**

What the walk found, and it is sharper than the body's diagnosis:

| family | declared cases | prepared directories | directories holding a fetched subject |
|---|---|---|---|
| `test/render/wpt` | 165 | 162 | **0** |
| `test/render/test262` | 813 | 813 | **0** |
| `test/render/khronos` | 188 | 185 | 185 |

The prepared directories EXIST for wpt and test262 -- they hold `manifest.json` and
`provenance.json` and nothing else. A directory-existence check would call them prepared;
only a check for a fetched SUBJECT sees the truth. That is why the earlier reading of "the
corpus needs prepare.py" was right about wpt and wrong about khronos, which is fully
prepared and does judge.

`test/run.sh` gained `WhatNoCorpusJudges` -- ONE rule, two callers: the fast gate prints it
in the trailer, and `test/run.sh --corpus` answers it alone so a test can ask without
running a gate inside a gate.

- **Proving test**: `test/harness/claims/AGreenTrailerNamesWhatItDidNotJudge` -- asks the
  runner through `--corpus` on the inherited nest, walks every family under `test/render/`,
  and requires the runner to name EXACTLY the families with no fetched subject: silent about
  khronos, loud about wpt and test262. Both directions are asserted, so the claim cannot be
  satisfied by a runner that simply prints everything.
- **Negative control**: the `! -name manifest.json ! -name provenance.json` filter removed,
  so a manifest counts as a subject -> the runner falls silent and the claim goes red with
  `FOUND test/render/wpt holds none and the runner is silent`. Reverted, green.
- Point 3 of the body -- "until then, no item may close by rewriting `Placer::Flex`" -- was
  honoured this hour: board:1753 was worked, its multiplier located at Layout.cpp:762, and
  left OPEN rather than closed behind a blind rewrite.
- Gate 231/231, and its trailer now names wpt and test262 on every run.
