Type: bug
Area: harness
Tags: instrument

**Adding a job to the preparer does not invalidate every oracle**

`test/harness/claims/EveryOracleWasPreparedByThisPreparer.cpp` digests **the whole preparer** and
compares it against the digest recorded beside each prepared oracle. Adding the `scenario-assets` job
-- which fetches nothing, renders nothing and touches no oracle -- moved that digest and turned
**every prepared case in the tree** into *"prepared by a different preparer"*:

```
prepared by 2810824b6f29f668...   the tree digests to bc0869a2f2793ec3...
cases whose oracle was produced by this preparer = 0 cases
```

**The claim is right to exist and its granularity is wrong.** What it must guarantee is that an oracle
was produced by the recipe that still stands -- fetch, patch, convert, render. A job that places a
scenario's licensed asset is none of those, and neither is a docstring.

## What must be true

- [ ] **The digest covers what produces an oracle**, not the file that contains it -- the recipe
      functions, or a declared version the recipe carries
- [ ] **A new job that touches no oracle leaves every prepared case valid**
- [ ] **A changed recipe still invalidates**, loudly, because that is the whole point of the claim

## Comments

**Caused by `board:1544`'s `scenario-assets` job and named rather than worked around.** The alternative
was to leave the job out and place the F31 by hand, which would have made the picture a function of
this machine instead of a declaration.

**The red is real and it is honest**: the tree's prepared corpus genuinely no longer matches the
preparer's digest. Re-preparing would clear it at the cost of hours of Cycles renders, and it would
clear it again on the next docstring edit.
