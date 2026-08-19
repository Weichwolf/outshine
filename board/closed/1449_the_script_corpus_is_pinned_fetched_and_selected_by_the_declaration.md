Type: task
Parent: 1448
Area: corpus
Tags: instrument

**The script corpus is pinned, fetched and selected by the declaration**

**test262 is the vendor**, the way Khronos is for glTF and web-platform-tests is for CSS: it is the
conformance suite the language itself is measured by, so a claim about what this interpreter runs is
made against somebody else's tests or it is made against nothing.

`test/test262/js/` holds the cases, one directory each, under the same manifest format everything that
renders already uses — so the browser reaches them by the same enumeration and no second format is
introduced.

## What must be true

- [ ] **Pinned to a commit and fetched through the same preparer**, with a per-file digest. A corpus
  that moves under a measurement is not a corpus
- [ ] **A new subject kind `script` and a criterion kind the suite decides by.** test262 states its own
  expectation in a `/*--- ... ---*/` block — `negative` says a case must be REFUSED and says with what,
  `flags: [raw]` says it needs no harness, `includes` names the harness files it does need. Reading
  that block is how the case declares itself, and reading it wrongly is how a suite passes by
  misunderstanding what it was asked
- [ ] **Selection is derived and never curated.** A case is in when everything it reaches is inside the
  subset `board:1448` writes down. The count will be small at first and that is the measurement, not a
  disappointment
- [ ] **Two counts side by side** — cases held, and how much of the suite the subset reaches. Neither
  stands for the other, and the second is the one that would improve by attempting less

## The expected shape of the first answer, written down so it can be wrong

test262 is overwhelmingly about objects, prototypes, closures and the standard library, every one of
which is named outside. **The subset should reach the `language/expressions` and `language/statements`
corners and almost nothing else**, and a first number much above a few hundred of tens of thousands
would mean the subset checker is admitting cases it cannot really run.
