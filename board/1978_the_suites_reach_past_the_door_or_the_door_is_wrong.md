Type: issue
State: open
Parent: 1953
Area: architecture

# Fifteen of seventeen suites reach past the door

`STATE.md` — **15 of 17 declared suites are granted a `-Isrc` path**; only `apps/driver` and
`apps/viewer` reach the library through `include/` alone. CLAUDE.md says everything under `test/`
reaches through `include/` and NOTHING of `src/`, so the rule and the tree have never agreed.

**The benchmark does not hold our rule either.** Unreal keeps low-level tests INSIDE the module
they test (`Private/Tests/`), compiled as part of it, and only the public-API tests stand outside.
That is the same answer with a different address: a case that tests an internal type is part of
that module, not a client of the door.

So one of two things is wrong and the measurement does not say which:

1. TARGET is too strict — internal cases belong beside the code, and only `outshine/door`, the
   corpora and `apps/` are outside. Then the rule names WHERE a case lives, not what it may reach
2. TARGET is right and 15 suites are proving the wrong thing — they hold internals to what the
   internals already do, which is the regression net CLAUDE.md already discounts

- [ ] the rule TARGET states is the one the benchmark holds, and it says so in one sentence
- [ ] the count in `STATE.md` moves, or the rule that produced it does
