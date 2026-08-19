Type: task
Parent: 1448
Area: harness
Tags: instrument

**A script case is decided by what it declares about itself**

The harness for `test/test262/js/`: read the case, run it against a host, and decide it by the
expectation the case's own frontmatter states.

## What must be true

- [ ] **A positive case holds when it runs to the end without a refusal.** test262's own convention is
  that a passing test simply returns; a failure throws. This interpreter has no exceptions, so the
  harness supplies a host whose `assert`-family calls refuse — which turns *throw* into *the run
  refused*, and keeps the case's own text unmodified
- [ ] **A negative case holds when the run REFUSES, and refuses with what the case named.** A case that
  passes by refusing for the wrong reason is the same defect `limits-probe` was given a `declines`
  field to prevent, and the answer is the same one: the refusal must contain what the case declared
- [ ] **One process per case**, so a run that takes the interpreter down fails that case and nothing
  else
- [ ] **Outside the subset is a count and not a skip**, printed with the names that put it there —
  a counter that goes up with an empty list is a case quietly dropped

## Depends

`board:1449` for the cases, and `board:1448` for something to run them with. **It is written first
anyway**: a corpus that arrives after the capability measured nothing on the way.
