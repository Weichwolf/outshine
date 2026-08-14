Type: bug
Area: corpus
Tags: oracle, khronos, instrument, scope

**The artefact that defines phase one's scope is the one artefact nobody fetched**

Every subject in this corpus is fetched, digested against a declared SHA-256 and kept in the content
store. **`Models/model-index.json` is not.** Phase 1's denominator — `board:1171`'s **148 models, 72
`core`, 53 uncovered** — came from a **live fetch**, and it is not reproducible offline: no object under
the content store contains `XmpMetadataRoundedCube`, searched **by content** rather than by name, because
store objects are named `derived_key(kind, recipe)` and not by their bytes.

**The pin is real and tracked** — `glTF-Sample-Assets @ 2bac6f8c57bf471df0d2a1e8a8ec023c7801dddf`, cited
by every Khronos manifest — so the *subjects* are pinned and the *inventory over them* is not.

**Why this is worse than an ordinary missing digest.** The same reader returned **148**, then **228**,
then **244** for that one file. 148 was taken only because an independent query named the same first and
last elements, so a truncated list was ruled out. **Had the first total been taken, phase 1's denominator
would have been wrong by 80** — and nothing in the tree could have contradicted it. *A number that decides
a phase's size, produced by an instrument that disagreed with itself twice, with no offline path to
re-derive it.*

## The repair, and the weaker option is named so it is not chosen by default

- [ ] **Fetch and digest the index like any other product** — correct, and **insufficient alone**: it
  makes the number reproducible and leaves the denominator a function of upstream. A model added upstream
  would silently change what *pass the Khronos corpus* means, and the pin would still verify
- [ ] **THE IN-SCOPE SET IS DECLARED IN THIS TREE AND CHECKED AGAINST THE PIN — take this one.** The set
  of models phase 1 must pass is **ours**, written down, one line per model with its band and, where it is
  excluded, the reason. The index is fetched and digested, and the check is that **every declared model
  exists at the pin** and that **every model at the pin is either declared in scope or declared out with a
  reason**. An upstream addition then **fails a check** instead of moving a denominator
- [ ] **The two counts stay apart**, as everywhere else here: *models declared in scope* and *models with
  a case within the picture bound*. Phase 1's sentence needs both and neither is the other
- [ ] **The exclusion reasons are the valuable half**, not the list: *this engine has no term for
  `KHR_materials_transmission`* is a statement that can be revisited when it gains one, and *Khronos marks
  this model `issues`* is a statement about the asset. A filter with no reasons is a number nobody can
  argue with

**The caveat, sought and cleared.** *Is a declared set just a second copy of upstream that will drift?* It
is a copy, and the drift is **the point**: it drifts visibly, against a digest, at a named commit, and a
check goes red. The alternative is a denominator that changes when someone else edits a file — which is
the same defect as a threshold that moves without a diff, one level up.

**Done when** the index is a fetched product with a digest, the in-scope set is declared in this tree with
a reason per exclusion, a model present at the pin and absent from the declaration is a failure, and
`board:1171`'s 148 / 72 / 53 are re-derivable with the network unplugged.
