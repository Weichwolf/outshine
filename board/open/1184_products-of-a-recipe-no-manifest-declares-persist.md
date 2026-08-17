Type: bug
Area: corpus
Tags: oracle, instrument

**Products of a recipe no manifest declares persist, and only the prune's guard noticed**

`test/khronos/glTF/PointLightIntensityTest/` holds `oracle.seed-shift.exr` and `oracle.seed-shift.raw` —
**17.5 MB of products of a recipe its manifest no longer declares.** No key was ever recorded for them, so
`board:1181`'s prune could not prove them recoverable, **left them standing, and said so**. That is the
guard behaving exactly as designed, and it surfaced a defect that predates it.

**The defect is that a case directory can carry products no declaration names and nothing says so.** The
preparer writes what the manifest declares; it does not remove what the manifest **stopped** declaring.
A recipe deleted from a manifest leaves its products behind, and until the prune existed **nothing in the
tree would ever have looked at them.**

**Why it matters beyond 17.5 MB.** A stale product is a file a reader can open. `Parity.cpp` reads
companions **by name** beside `oracle.exr`, so a case that regains a `seed-shift` recipe under different
declared conditions would find bytes from the old one already sitting there — and `_matches` compares
against the key it *expects*, not against a key nothing recorded. **The failure mode is a case scored
against a product of a declaration that no longer exists.**

- [ ] **A product with no key in the account is either removed or reported by name**, and the choice is
  the owner's, not the preparer's: removing is a deletion of something derived, reporting is a line that
  accumulates. **`board:1181`'s guard already reports it** — what is missing is anything acting on the
  report
- [ ] **The case's own account is the enumeration**, and `board:1154` put it in the store on both paths,
  so *which products should exist* is now answerable without re-running anything. **This defect became
  fixable in the same round it became visible**
- [ ] **Check the corpus for the rest of them**, exhaustively, rather than repairing the one case the
  prune happened to name. Two files were left standing on the measured run; **whether that is the whole
  population is unmeasured**, and a count is one pass over the accounts

**Done when** a product no declaration names is named by the run that finds it and removed or kept by a
recorded decision, and the corpus is shown to carry none that nobody knows about.
