Type: issue
State: open
Area: test
Tags: process, claims, measured

# The gate's verdict means what it says

**Benchmark** — Unreal: automation reports pass/fail per test and a red is a red. RAGE: the same. **Both agree** — a gate whose verdict does not mean what it says is worse than no gate.

## MEASURED at 84115df7, a full `sh test/run.sh` in its own worktree

    1844 tests: 1829 PASS  0 FAIL  0 TIMEOUT  0 SIGNAL  0 BUILD  0 SKIP  15 UNPREPARED
    the viewer (deleted with apps/) did not BUILD AND ANSWER --help ... 'Face.h' not found
    run.sh: 1 program(s) build and answer --help, 1 do not; 2 compile through the DOOR alone
    run.sh: THE FAST GATE OVERRAN ITS BOUND -- 2612327 ms of RUN over the declared 230000 ms
    run.sh: 16 case(s) are RED and the verdict is theirs, whatever the clock said
    EXIT=1                                   16 = 1 unbuilt program + 15 UNPREPARED

**That last line had never been printed before this session.** The overrun's own `exit 1`
returned before `red` was computed, so for as long as the gate has overrun -- every full run this
tree has measured -- the one line naming how many cases are red did not run. Fixed in c9faaaec;
one hour later it spoke, and what it found is below. The fix worked exactly as intended and the
box is done.

## The viewer is refused by a rule the same function contradicts twelve lines lower

`EveryProgramStillLinks` spells the client's include truth TWICE and the two disagree:

    test/run.sh:598   $CXX ... -Iinclude -c "$one"                          <- decides the exit code
    test/run.sh:613   $CXX ... -Iinclude -I"$layer" -I"$layer/parts" -fsyntax-only "$one"

The viewer's entry point included `"Face.h"` from its own `parts/` -- the
viewer's OWN source, not the library. CLAUDE.md's door rule forbids a client reaching into
`src/`; a client's own `parts/` directory is not `src/`, and **:613 has the rule right while :598
has it wrong**. Neither calls `LayerIncludes "$layer"`, which is what `make` uses and under which
the viewer builds and links without complaint.

So `make` and `test/run.sh` disagreed about whether that client compiled, and the disagreement is
a second spelling of the include map inside the very file the Makefile's own header names as the
ONLY spelling (board:1584). The viewer is not broken; the check is.

## The bound is overrun 11x, and un-silencing two corpora is what did it

    a32c4919    862 tests    752 511 ms RUN    3.3x the bound
    84115df7   1844 tests  2 612 327 ms RUN   11.4x the bound

975 of the 982 new cases are `harness/test262/js` and `harness/wpt/css`, which were declared
door-only and therefore did not compile at all (board:1879). Bringing them back is right and the
time is the honest price of it. But 43 minutes of RUN against a declared 230 s is not a bound any
run can meet, so the overrun line has become furniture that prints every time and blocks nothing.
A bound nobody meets is a bound nobody reads.

`test corpora: peak 58 941 MB`. The fast gate still walks the whole prepared render corpus from
one case rather than reading the one subject it is about.

## A claim number names one claim

Every proof ends in `Covers("<number> <sentence>")` and the number is the claim's identity. There
is no ledger of which numbers exist, they are chosen by hand, and 42 of the 106 in use carry two
or more UNRELATED sentences (`IV.13`, `IV.14`, `IV.15` each collided the day they were written).

## What will be true

- [x] An overrun does not mask a verdict: the counts are computed and printed before any exit,
      so a slow run still says which cases were red.
- [ ] The client's include set is spelled ONCE, by `LayerIncludes`, and every check in
      `EveryProgramStillLinks` uses it. Proving case: `build/outshine-client` builds, answers `help` and
      is counted as reaching the library through the door; negative control, `main.cpp` including
      an `src/` header, and it is refused by name.
- [ ] The fast gate holds its declared bound, and the bound is a MEASUREMENT of what the gate
      must run rather than a number it overruns every time. The two long corpora move behind a
      name, or the bound moves to what they cost -- argued, not raised.
- [ ] The fast gate reads no corpus: the survey moves to the suite that owns the corpus, and the
      case that walks the whole prepared tree reads the one subject it is about.
- [ ] A claim number is unique by construction -- a ledger the harness reads, or a number derived
      from the file -- and a collision refuses at build time.
