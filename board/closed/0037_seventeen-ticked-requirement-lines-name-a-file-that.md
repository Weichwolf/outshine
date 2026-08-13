Type: bug
Area: generators
Tags: instrument

**Seventeen ticked requirement lines name a file that is not in the tree — **Band 2****

*Measured at `81d4db1`, mechanically, not by reading.* `board/` cites **50 distinct paths
under `src/`, `test/` or `board/` in backticks**; **23 of the 50 do not exist**, and they appear on **17
of the 289 ticked lines** — the old scope ledger at lines 189, 263, 503, 522, 716-725,
726, 729, 730, 731, 741, 2586`. **A tick means *checked in the tree this round* and names the file that
implements it; a tick whose file is absent is a claim with its evidence removed**, which is the same
defect class the *stale pointers* entry above already carries and which this document says has cost the
project twice.

**Two populations, and they need different repairs.**

- **Six moved and are still there**, from § I.26.9's re-organisation of the unit suite: `test/unit/core/PlanarGeodesyHoldsToItsScope.cpp`, `test/unit/data/AbsenceHandsOver.cpp`, `test/unit/data/TheAnswerNamesItsAddress.cpp`, `test/unit/data/UncoveredIsUndeclared.cpp`, `test/unit/generators/SameRegionSamePlacement.cpp`, `test/unit/generators/draw/GrownBarkIsAClosedMesh.cpp` — each now under `test/unit/`. **Eleven of the seventeen lines are one path-prefix edit**, and every one of those tests **PASSes** in this round's run, so the claim is true and only its address is wrong.
- **Sixteen are gone**, and they are named without their paths on purpose — a deleted file written in citation syntax is the very defect this entry is about: the architecture document · six browser-era clients (the file-artefact sink, the HTTP client, the PNG writer, the server log, the server telemetry, the walker) · the building shader · three deleted entry points (the walk client, the tree bench, the world entry point) · two reachability subjects · the counter-width test · the arrival-order proxy. **These need a per-line judgement and not an edit**: the tree bench alone carries two ticks, and a tick whose test was deleted is a capability that may or may not still be held.

**The harmless explanation, sought and ruled out.** *"The audit already happened"* — `3e90d14` retired
twenty-four ticked lines whose **capability** the port deleted, which is a different question from
whether a surviving line's **citation** resolves; the six moved tests were never a capability loss and
were not in that pass's scope.

**Right:** the eleven prefix edits, then sixteen line judgements. **Fixed when** the extraction above
returns zero missing — and the class ends only when the harness runs it: **every backticked path in
`board/` and the bug tasks in `board/` resolves, or the run is red.** That is a shell loop over one
`grep -oE`, it costs milliseconds, and it converts a defect that has recurred three times into a
compile-time-equivalent property of the documents.
