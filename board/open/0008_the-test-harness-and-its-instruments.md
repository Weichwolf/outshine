Type: bug
Area: harness
Tags: perf, instrument

**The test harness and its instruments**

*Six entries deleted 2026-08-12 as fixed or as naming deleted sites: `verify-data` (the target, the deleted HTTP client and every `curl_` symbol are gone — `grep -rl curl src/` is empty); `verify-walk-asan` cannot see a stack lifetime error (`test/run.sh:573` sets `detect_stack_use_after_return=1`); an interrupted instrument leaves something bound (`test/run.sh` carries the group kill and the eleven `verify-*` recipes are gone — the Makefile has three targets); two tests hold each other's claim (`OutsideIsNeverAsked.cpp` is now `test/outshine/unit/data/UncoveredIsUndeclared.cpp` and the false `CountingTransport` comment is gone); a directory declared as the Makefile's is trusted (the Makefile owns no test source now, and a compile subject is driven by `-DOUTSHINE_COMPILE` at `test/run.sh:517` rather than being unrun).*

- **The registry's and the store's counters have no reader.** `Data::SourceSet::Ledger`
  (`data/SourceSet.h:76`) publishes nine — `Asked`, `Delivered`, `HandedOver`, `Vacant`, `Undeclared`,
  `Refused`, `Retried`, `FromStore`, `DeliveredBytes` — and `Data::ContentStore::Ledger`
  (`data/ContentStore.h:52`) six. **Verified at `9f4ba9e`**: `Counters()` is called from
  `test/outshine/unit/data/AbsenceHandsOver.cpp:136`, `TheStoreNamesBytesByTheirKey.cpp:78,113` and **nowhere
  else in the tree** — no telemetry row, no close-out line. The store's hit rate is the one number
  that decides whether the store is doing anything, and nothing prints it. `Per.6`, and § I.23's
  zero-consumer rule applies to a counter as much as to a constant. **Band 3** — waits for the round
  that restores a telemetry consumer, because there is no row to ride today. Right: both ledgers ride
  the ordinary row and the close-out line.

- **The trailer is authenticated by shape alone, so a file that never includes `Check.h` can print a
  green verdict.** `test/run.sh:343` accepts one line of eight fields with `CHECKS`, `FAILURES`,
  `SKIPPED` in the right places; `:435` cross-checks it against the process exit status, which a
  forger satisfies by returning 0. *The demonstration is gone with the deleted forged-trailer probe;
  the shape is not, and it is re-demonstrable in one file.* Right, two lines and no change to
  `Check.h`: every increment of `Failures` prints exactly one line beginning `FAIL ` and every `Skips`
  one beginning `SKIP `, so `grep -c '^FAIL '` **must** equal `FAILURES`. That is a second witness on
  an independent path — per-failure `printf` against a counter — and it also catches a counter zeroed
  by any spelling `Tally` does not forbid. `CHECKS` has no printed witness and stays single-sourced.
  **Band 3** — waits for a test the harness cannot already judge.

- **A hard error stops the run, so one malformed test hides the verdict of every test after it.**
  `test/run.sh:435` `Die`s mid-loop when the trailer and the exit status disagree, and fifteen `Die`
  sites remain. The rule the deleted Makefile stated in its own words — *"every gate runs even after
  one has fallen, because the second failure is information the first one would have hidden"* — is
  now stated nowhere, and the harness is the instrument it matters most in. Right: a missing, doubled,
  malformed or disagreeing trailer is a per-test verdict of its own that is red and counted, the loop
  continues, and the run exits non-zero. Only the pre-flight directory scan may refuse before anything
  is built, which is correct there because nothing has run yet. **Band 3**, with the entry above.

- **The harness's build cache is keyed by path relative to the root, so two checkouts of this tree
  share objects, logs and binaries.** `test/run.sh:41-42` — `BUILD=${TMPDIR:-/tmp}` then
  `BUILD=${BUILD%/}/outshine-tests`, with **no component identifying the root**, verified at
  `9f4ba9e`. `UpToDate` compares mtimes of prerequisites resolved against the *current* root, so a
  second checkout whose sources are older than the first's objects links the **first checkout's**
  binaries, and every number read from them belongs to the other tree. A git worktree and a
  `git bisect` clone are ordinary, and the effect is silent. Right, one line: fold the root's real
  path into the build directory, e.g.
  `BUILD=${TMPDIR}/outshine-tests/$(printf %s "$ROOT" | cksum | cut -d' ' -f1)`. **Band 2.**

- **A real test placed in a non-harness directory is run by nothing.** `test/run.sh:207-214`
  `NotTheHarnesses` names `.`, `host` and `unit/compile*`; a `.cpp` there that includes the reporter
  and checks claims is named by no line of the output. Bounded — `:390` is a hard error for a
  directory in neither list, and two of the three are structurally not tests — so this is the narrow
  residue of a larger entry deleted above, not that entry. **Band 2**, and it is about six lines:
  refuse a source under those directories that includes `Check.h`.

- **The unit-height check accepts 168× the worst deviation it measures, and it bypasses the reporter's
  own rule about tolerances.** `test/outshine/unit/generators/draw/GrownBarkIsAClosedMesh.cpp:225` judges with
  a raw `std::fabs(v.DeclaredExtent - 1.0) > 1e-5` rather than `CHECK_NEAR`, so the number that
  decides an acceptance carries no origin and no frame of reference — the thing `Check.h:52-54` was
  written to forbid. Measured over all 31 declarations: the worst deviation is **5.96046448e-08 in
  `dog_rose`**, which is `2^-24` exactly, the float spacing immediately below 1.0 — one ulp, and the
  other 30 land on 1.0 bit-for-bit. `1e-5` is 168 ulps, so a normalisation that drifted to 0.99999
  passes. Right: `CHECK_NEAR(extent, 1.0, 2.4e-7 /* 4 ulp at 1.0 */, …)`, with the ulp derivation
  beside it. Note also what the lying branch proves: `DeclaredExtent` re-decides `GrowthForm::Lying`
  the same way `TreeGrower::NormalizeToUnitHeight` does, so for a lying form the check is
  *consistency* between two copies of one predicate and not the decidable class — only the standing
  case is decidable. **Band 2.**
