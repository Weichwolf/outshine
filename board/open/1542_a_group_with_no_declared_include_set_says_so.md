Type: bug
Area: harness
Tags: instrument

**A source group with no declared include set SAYS SO**

`test/run.sh`'s `GroupIncludes` ends in `*) return 1`, and `BuildGroup` propagates that as an exit
without a message. Naming a new suite whose source list contains a group the table does not carry
gives:

```
run.sh: render/outshine/world only
$ echo $?
2
```

**No test name, no group name, no reason.** It took `sh -x` and reading the trace's last line to learn
that the unlisted group was `src/clients/Sim.cpp`.

`Die()` exists and every other refusal in the file uses it -- *a failure is loud* is the rule this one
path misses.

## What must be true

- [ ] **The refusal names the group and the table it is missing from**, the way
      `"$candidate is under test/$candidateLayer, which declares an include set but no source groups"`
      already does one line away
- [ ] **Every `return 1` in the harness that a caller turns into an exit carries a `Die`**

## Comments

**Cost about fifteen minutes of guessing** -- the include set, the link libraries and the toolchain
entry were all checked first, and all three were fine.
