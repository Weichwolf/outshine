Type: bug
State: open
Area: test, gate
Tags: measured, gate

# The fast gate is GREEN again, and it runs inside the bound it declares

**Benchmark** — Unreal's automation gates on a green run and treats a slow test as a defect the same
way it treats a failing one; RAGE's build farm does the same. Both are unambiguous: a gate that is
red at HEAD is not a gate, it is a report nobody reads, and the day something real breaks it looks
exactly like today.

## Measured 2026-09-01, at HEAD with board:2093's work STASHED

`make test`: `2558 tests: 2457 PASS 100 FAIL 0 TIMEOUT 0 SIGNAL 0 BUILD 0 SKIP 1 UNPREPARED`, then
`101 case(s) are RED`. Of those, `harness/claims` contributes 13, and the identical thirteen appear
with the work applied and with it stashed -- so they PREDATE that branch and none of them is its
doing.

| case | what it says |
|---|---|
| `AGreenTrailerNamesWhatItDidNotJudge` | |
| `AnItemReachesClosedThroughActive` | the board is empty; `State: active` reaches nothing |
| `APreparedFileNeverLandsInTheTree` | |
| `EveryOracleWasPreparedByThisPreparer` | |
| `EveryTypeNameIsDeclaredOnce` | 10 type names twice against a declared 9; 2 constants against 1 |
| `NoEnvironmentVariableDecidesAPicture` | |
| `NoFramePathCallReachesABlock` | |
| `PiStandsOnceAndItIsStdNumbers` | |
| `TheBuildDeclarationAuditsItself` | |
| `TheCorpusIsRebuiltByOneCommand` | the OSM corpus was deleted; the command sees no manifest |
| `TheEngineNamesNoSubject` | |
| `TheMapCitesLinesThatSayWhatItClaims` | the map cites paths that no longer resolve |
| `TheNestRefusesASecondRunner` | |

The empty right-hand column is the work: each case says WHAT it measured, and this item is not
understood until every row names why that measurement moved. Several look like consequences of two
decisions the owner made deliberately -- the board was emptied of 145 items and the OSM corpus was
deleted -- in which case the CLAIM is what stopped meaning anything and the claim goes, not the
tree. A ceiling that may only fall is a different case: `EveryTypeNameIsDeclaredOnce` reads ABOVE
its declared number, which is a real regression somebody has to name.

## The clock is the second half

`604694 ms of RUN over the declared 230000 ms`, with 200100 ms of builds standing beside it. That
figure is NEW and it is board:2093's doing, honestly: `LayerSanitiser` named six deleted `unit/...`
layers, so ASan and UBSan had been building nothing at all, and the seven harness layers that do
exist now build twice. The bound was written when the sanitised half cost zero.

So the choice is stated rather than assumed: either the bound rises with its reason, or the
sanitised set shrinks to the layers whose findings are worth the seconds. **Do not resolve it by
turning the sanitiser back off** -- that is how it came to be pointed at nothing.

## Done when

`make test` prints no RED case and no overrun line, and one deliberately broken oracle turns it red
again.
