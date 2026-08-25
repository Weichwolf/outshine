Type: issue
State: open
Area: test
Tags: process, claims

# The gate's verdict means what it says

Two defects, one statement: a green trailer must not read as coverage it never had.

**The fast gate stands on the sporadic proof's corpus.**
`test/unit/gltf/ADerivedCameraIsTheFramingRuleAndNotAQuotation.cpp:87` does not read one
subject — it walks `recursive_directory_iterator` over the whole prepared render corpus, 169
declared cases. Against a swept temp directory that one twin reported 160 UNPREPARED subjects in
a single fast-gate run: the gate was green only because a directory that no gate builds happened
to hold a corpus, and the machine may sweep it at any moment.

**A claim number names one claim.** Every proof ends in `Covers("<number> <sentence>")` and the
number is the claim's identity — how a scorer, a trailer and a reader tie a case to what it
proves. There is no ledger of which numbers exist, they are chosen by hand, and 42 of the 106 in
use carry two or more UNRELATED sentences (`IV.13`, `IV.14`, `IV.15` each collided the day they
were written).

**MEASURED 2026-08-25 at a32c4919, a full run in its own worktree:**

    862 tests: 845 PASS  0 FAIL  0 TIMEOUT  0 SIGNAL  2 BUILD  0 SKIP  15 UNPREPARED  in 1142019 ms
    THE FAST GATE OVERRAN ITS BOUND -- 752511 ms of RUN over the declared 230000 ms
    apps/viewer/src/main.cpp does not BUILD AND ANSWER --help ... 'Face.h' file not found
    peak 51034 MB, 446 cases pruned, 7414 MB declined

The reds are gone (2 FAIL -> 0) and the run is down from 1205550 ms to 752511 ms -- still **3.3x
the declared bound**, and the corpus is what fills it. The 2 BUILD are unchanged and one of them
is the viewer, which the door-only check refuses because `main.cpp` includes `"Face.h"` from a
sibling directory the door does not carry.

**Nineteen minutes is also why nobody runs it.** Every commit this session reports a SUBSET --
`harness/outshine N PASS, harness/claims 24 PASS, audits green` -- so the default gate's exit
code at HEAD is read by the hourly review and by nothing else. The overrun's own `exit 1`
(test/run.sh:1510) returns before `red` is computed at :1513, so a run that overruns still
reports no verdict on its failures.

## What will be true

- [ ] The fast gate reads no corpus: the survey moves to the suite that owns the corpus, and the
      unit twin reads the one subject it is about.
- [ ] The fast gate holds its declared bound, and the bound is a MEASUREMENT of what the gate
      must run rather than a number it overruns every time.
- [ ] An overrun does not mask a verdict: the counts are computed and printed before any exit,
      so a slow run still says which cases were red.
- [ ] A claim number is unique by construction — a ledger the harness reads, or a number derived
      from the file — and a collision refuses at build time.
