Type: bug
State: open
Area: test
Tags: gate, claims, measured, regression

# Twelve of twenty-six claims are red, and eleven of them are debris of the cut

Measured 2026-08-25 at a3ebe3e0, with `apps/viewer` parked so the gate could run at all
(board:1880): `582 tests: 553 PASS  6 FAIL  0 TIMEOUT  3 SIGNAL  5 BUILD  0 SKIP  15 UNPREPARED`.
The corpus is healthy — `khronos: criteria 179 met of 179, picture bound 178 within, 0 outside`.
**The claims are not.** Twelve of twenty-six, and only ONE of the twelve found a defect in `src/`.

| claim | what it does | what it names |
|---|---|---|
| `TheSourceCarriesNoCommentary` | **SIGABRT** | `recursive_directory_iterator: No such file or directory ["tools"]` — the NO-COMMENTS rule is unguarded at HEAD |
| `TheDeviceLeavesTheLibraryOnlyForItsOwnTwins` | **SIGABRT** | the same `["tools"]` — the device boundary (board:1826) is unguarded |
| `OneToolchainIsSpelledOnce` | **SIGABRT** | `std::out_of_range: basic_string` after `both spellings of the toolchain are readable` failed — an unguarded `substr` where a refusal belongs |
| `APreparedFileNeverLandsInTheTree` | FAIL | names `test/render/outshine/grown`, deleted with the cut |
| `TheLayeringIsDeclaredOnce` | FAIL | expects the group `render/outshine/client`, deleted with the cut |
| `TheBuildDeclarationAuditsItself` | FAIL x5 | every negative-control SEED names a deleted source (`src/clients/Sim.cpp` in the world suite, `Wayfinding.cpp`) — the four detectors are unproven, so the audit's green means nothing |
| `APruneRemovesOnlyWhatItProved` · `EveryOracleWasPreparedByThisPreparer` · `EveryRenderNamesItsIndices` · `harness/render/test262/js` · `harness/render/wpt/css` | BUILD | do not compile |
| `TheMapCitesLinesThatSayWhatItClaims` | FAIL | 2 drifted citations, 2 dead paths — the review owns the repair |
| `EveryGuardSpellsItsFolder` | FAIL | **the only one that found a defect in `src/`** — board:1883 |

**`EveryNodeTheMapDrawsIsNamedByAProof` is WITHDRAWN, not repaired.** Its sentence is *"the unit
mirror IS the layering proof, so a class the tree draws in its own architecture and no test ever
mentions is a layer nobody has checked"*. That is a claim about OUR OWN SHAPE, which the tree no
longer accepts: a case declares what the engine stands up and compares it against a reference
whose truth does not depend on our design. It was satisfiable only because `test/unit/` mirrored
`src/` file-for-file, and it went red the hour that mirror was deleted. Deleting it is the
correct answer; it is listed here so the deletion is a decision and not an omission.

## What will be true

- [ ] No claim aborts. A path that is not there is a REFUSAL with a name, never an exception out
      of `std::filesystem` — a guard that crashes is a guard that stopped guarding, and it goes
      GREEN in every summary that counts only failures (board:1857).
- [ ] `TheSourceCarriesNoCommentary` and `TheDeviceLeavesTheLibraryOnlyForItsOwnTwins` walk
      `src/`, `include/` and `apps/` — the three trees CLAUDE.md now names — and are green.
- [ ] Every negative control in `TheBuildDeclarationAuditsItself` seeds against a source that
      EXISTS; a seed naming a deleted file fails the claim as a stale control, by name.
- [ ] `EveryNodeTheMapDrawsIsNamedByAProof` is deleted, and the report says so.
- [ ] The five BUILD failures compile or are deleted with what they proved.
