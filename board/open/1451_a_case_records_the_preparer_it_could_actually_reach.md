Type: bug
Area: corpus
Tags: instrument

**A case records the digest of the preparer it could actually reach**

The digest a prepared case records is taken over **every** `.py` under `test/harness/`, so adding a
vendor's fetch step invalidates every other vendor's corpus. [MEASURED] this happened FOUR times in
one run of work: 143 cases, then 6, then 330, and then every one of them again -- the fourth time for a
**comment added to a Python file**, which cannot change a product by any argument anyone would make — and the third was caused by
`test/harness/test262/js/prepare/fetch.py`, a file a Khronos case cannot reach, because the vendor's
step is chosen by the case's own path.

**It is `CLAUDE.md`'s *input set too wide*, in the instrument this time.** The claim *this product was
made by the preparer now in the tree* is right; the population it is drawn from is wider than the claim
it decides, so a true statement about one corpus is refuted by an edit to another.

## What it costs, and the cost is why this is a bug and not a preference

Re-preparing the picture corpus is **hours of Cycles**. Paying that because a second corpus gained a
fetch step is paying for a coupling that does not exist.

## What must be true

- [ ] The digest is over **the shared preparer plus the case's own vendor's steps**, which is the same
  lookup `vendor.harness_of` already performs to CHOOSE the step — so the two cannot drift, and the
  guarantee stays exactly where it means something
- [ ] The C++ check computes it the same way, per case, from the case's own path. **It may not read a
  list out of the provenance it is checking**: a digest verified against a list the same file supplies
  is a document agreeing with itself
- [ ] The rule that made the population wide is kept where it is right: *a named list is a second copy
  of a fact* — so the narrowing is **derived from the vendor lookup** and never written down as a table

## A second narrowing the fourth occurrence argues for

**A case whose preparation produced no ORACLE product should record no preparer digest at all.** Its
files are upstream bytes verified against their own `sha256`, which is a stronger guarantee than *made
by this code* -- so 975 of the tree's 1146 cases are paying for a claim that says less than the one they
already carry. That alone removes most of the churn, and it is derivable from the manifest's own
`schema`.

**And the vendor-specific modules belong in their vendor's directory.** `prep/wpt.py` and
`prep/test262.py` sit in the SHARED preparer, so the reachability rule above cannot exclude them from a
Khronos case even though nothing reaches them. Moving them beside the fetch steps they belong to makes
the same rule sharper without a list -- which is what `board:1196` refused and this preserves.

## Comments

The wide population was deliberate and its reason is still good — `board:1196` widened it after a
split by vendor moved `fetch.py` and `grown.py` out of one directory and a glob silently stopped
covering them. **The repair is not to narrow it back by a list**, which is what that round refused; it
is to derive the same reachability the preparer already derives when it picks a step.
