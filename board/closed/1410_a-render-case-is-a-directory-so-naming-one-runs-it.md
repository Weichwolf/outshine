Type: task
Parent: 0079
Area: harness

**A render case is a directory, so naming one runs it**

`test/run.sh` took a suite and nothing narrower, so the smallest thing that could be run was a whole
declarative suite -- **45 cases to see one number move, and the corpus is 148**. That is a tax on
exactly the iteration the corpus exists to make cheap, and it was paid every round.

[MEASURED] one Khronos case: **8 minutes to 2.4 seconds**. One grown case: 4 minutes to 2.5 seconds.

## What must be true

- [x] **The layer is DERIVED by asking each layer's own `LayerCases`** whether it holds the path, never
  by a second table beside it -- a mapping written twice can disagree with itself, and this one would
  disagree silently by running the wrong suite over no cases at all
- [x] **A path that carries a manifest and no layer enumerates is a refusal**, naming what to do
- [x] **The trailer still decides.** A filtered run reports the count it actually ran, so `1 tests:
  1 PASS` cannot be quoted as a suite -- the same protection the trailer already gives every run
- [x] **A suite name still means the suite**, so nothing that worked stopped working

## Comments

**The filter is a case DIRECTORY and not a test source**, so `outshine/shader/SomeTest` is still a
refusal rather than a filter -- the declarative suites are the ones with cases, and the mirror tree
runs by suite as it always did.
