Type: issue
State: open
Area: test
Tags: gate, guard
Depends: 1898

# Every red the gate can produce has a declaration channel, so no failure is nameless

`EXPECT_FAIL` declares a standing red per CASE with its count, and the gate turns red the day a
declared case passes. `EveryProgramStillLinks` produces a different red -- a program that does
not build or does not answer `--help` -- and had no such channel, so the failure appeared in the
verdict with no name, no reason and no expiry.

`EXPECT_UNLINKED` now closes that hole for programs. The question this item holds open is
whether any OTHER red the gate can produce is still undeclarable:

- `EverySourceStillCompiles` -> `compileBlind`
- `undeclaredSkips`, `unprepared`, `compileBlind` in the verdict at `test/run.sh` line 1841
- the audits (`--audit-layers`, `--audit-access`, `--audit-numbers`, `--audit-link`), each of
  which refuses on a count moving

Each needs the same treatment or a stated reason why it does not: a red that cannot be declared
is a red somebody silences by deleting the check.

## What will be true

- [ ] Every refusal path the runner has either names its declaration variable or states in one
      line why a standing instance of it is impossible.
- [ ] Proving case: a claim walks the runner's refusal paths and refuses when one has no
      declaration channel. Negative control: the path removed from the walk, and it passes.
