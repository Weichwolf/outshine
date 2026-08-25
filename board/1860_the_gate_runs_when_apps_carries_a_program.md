Type: bug
State: active
Area: test, apps
Tags: gate, blocking, driver

# The gate runs when apps/ carries a program and not only tests

At HEAD (`38641b13`) `test/run.sh` runs NOTHING. Any invocation that names `apps` -- and
every invocation that names nothing at all, because `TREES="test tools apps"` is the default
(test/run.sh:745) -- dies before the library is built:

```
$ ./test/run.sh
run.sh: apps/driver/src/main.cpp is under test/apps/driver/src, and the harness knows no such
directory -- declare it in LayerIncludes and LayerGroups to run its tests, or in
NotTheHarnesses if the Makefile judges it
$ echo $?
2
```

The walk at test/run.sh:754 takes EVERY `*.cpp` under `test tools apps` and demands its
directory be a declared test layer:

```sh
for candidate in $(find $TREES -name '*.cpp' | sort); do
  candidateLayer=$(dirname "${candidate#test/}")
```

`apps/driver/src/main.cpp` landed in `38641b13` as the driver's ENTRY POINT -- the thing
board:1803 has been asking for. It is a program, not a proof, and there is no layer under
which it could be declared without lying about what it is. The commit message says *"It
compiles and links first try against include/outshine/ and nothing else"*, which is true and
was measured by hand; what nobody measured is that the gate itself stopped.

Two defects, one line apart:

| | |
|---|---|
| the walk cannot tell a PROGRAM from a PROOF | test/run.sh:754-764 -- `find $TREES -name '*.cpp'` and every hit must be a case |
| the refusal misnames the path | test/run.sh:763 prints `under test/apps/driver/src`; the file is at `apps/driver/src/main.cpp`. `${candidate#test/}` strips a prefix that is not there and the message pastes `test/` back on |

## What will be true

- [ ] `test/run.sh` with no arguments runs the fast gate green at a HEAD where `apps/driver`
      carries an entry point, and a NEGATIVE CONTROL shows it red against `38641b13`.
- [ ] The runner has ONE declared answer to "what is a program under `apps/`" -- the same
      authority board:1801 gave it for what a proof is -- and it BUILDS every such program
      under the layer's include truth, so an entry point that stops compiling turns the gate
      red instead of going unnoticed (board:1602, board:1766 are the twins for `src/` and for
      named-only suites).
- [ ] The refusal at test/run.sh:763 names the path that exists.

## Comments

- 2026-08-25 -- found by the hourly review's own first step: `test/run.sh apps/driver/test/stills`
  is the instruction the reviewer follows every round, and at HEAD it exits 2 without rendering
  a pixel. The main nest's working tree at the time of the review has `main.cpp`,
  `include/outshine/Outshine.h` and `src/clients/Engine.cpp` modified for board:1859 and
  `test/run.sh` untouched, so the gate is still down in the work in flight.

- 2026-08-25, what the down gate has already cost -- the review ran `harness/claims` in its own
  worktree with `main.cpp` held aside, and `ARepairFindsItsItemInActive` is RED at HEAD:

  ```
  NOTE the rule binds from 567fabfd49fb4284c9fa4461f55150b82e17bedf
  FOUND 2e7799012 repairs code under board:1859, which stood outside board/active
  FOUND 38641b130 repairs code under board:1859, which stood outside board/active
  FOUND 434ed8861 repairs code under board:1858, which stood outside board/active
  ```

  IV.33 is the guard board:1856 landed at `567fabfd` -- less than one hour before these three
  commits. All three violated it, and nobody saw, because between `38641b13` and now
  `test/run.sh` has not been able to reach a single case. A guard whose gate cannot run is a
  guard that does not exist, which is why this item is the first one of the hour.
