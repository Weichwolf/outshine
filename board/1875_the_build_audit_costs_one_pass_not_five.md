Type: task
State: open
Area: test
Tags: gate, cost, claims

# The build declaration is audited once per gate, not five times inside one case

Measured 2026-08-25 over two runs of `test/run.sh harness/claims` at 128db81e: the 26 claims
spend **47.3 s** of arm time and `harness/claims/TheBuildDeclarationAuditsItself` is
**30.9 s and 31.1 s** of it — 66 %, more than the other twenty-five together.

The case shells out to the runner five times: `--audit`, `--audit` on a seeded copy,
`--audit-link`, `--audit-link` on a copy with a source struck, `--audit-link` on a copy with a
ghost. Each seeded copy pins `ROOT="$PWD"` and re-derives object sets under its own identity, so
the negative controls pay full price every run on a machine that has already proven them.

The audits themselves are the RUNNER's, and the runner already runs them. What the case adds is
the four negative controls -- and a negative control is only informative when the detector could
have changed. The shape to reach:

- the runner publishes its audit verdicts once, in the trailer, where every other measurement
  stands;
- the seeded controls are driven against a declaration READ FROM A STRING, not against a copy of
  run.sh that rebuilds the tree -- the detectors are text over the declaration and need no
  objects to prove they fire;
- the case then costs what the other twenty-five cost.

## What will be true

- [ ] The claim's arm time is under 5 s, measured over three runs.
- [ ] All four detectors keep a negative control that fires on a seeded defect.
- [ ] The audit verdicts appear once per gate in the trailer.
