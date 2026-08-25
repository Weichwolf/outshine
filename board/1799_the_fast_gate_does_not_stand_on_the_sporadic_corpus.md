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

## What will be true

- [ ] The fast gate reads no corpus: the survey moves to the suite that owns the corpus, and the
      unit twin reads the one subject it is about.
- [ ] A claim number is unique by construction — a ledger the harness reads, or a number derived
      from the file — and a collision refuses at build time.
