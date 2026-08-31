Type: bug
State: open
Area: test
Tags: gate, process, measured

# The fast gate runs the rules the tree holds ABOUT ITSELF, and names them when it cannot

**Benchmark** — Unreal: `RunUAT BuildCookRun` runs the automation tests AND the project's own
`.Automation.cs` rules in one pass; a coding-standard rule that only runs nightly is a rule the
build does not have. RAGE: the same file that builds also runs `parCheck` over the metadata.
**They agree** and the matter is closed: a rule the fast path does not run is a rule that is red
for a week before anyone sees it.

## Measured, 2026-08-31

`test/gate.sh` names seven steps: `make`, the tier audit, `roundtrip`, the clear-sky score, two
Khronos cases and `outshine/places`. **It does not name `harness/claims`**, which is the suite
holding this tree's rules about its own sources, its own board and its own corpus.

    sh test/run.sh harness/claims
    36 tests: 22 PASS  12 FAIL  0 TIMEOUT  0 SIGNAL  0 BUILD  0 SKIP  2 UNPREPARED

**Twelve of the tree's own rules stand red behind a gate that is green.** Named, because a count is
not a finding:

    AGreenTrailerNamesWhatItDidNotJudge      AnItemReachesClosedThroughActive
    APreparedFileNeverLandsInTheTree         EveryOracleWasPreparedByThisPreparer
    EveryTypeNameIsDeclaredOnce              NoEnvironmentVariableDecidesAPicture
    PiStandsOnceAndItIsStdNumbers            TheBuildDeclarationAuditsItself
    TheCorpusIsRebuiltByOneCommand           TheEngineNamesNoSubject
    TheMapCitesLinesThatSayWhatItClaims      TheNestRefusesASecondRunner

And `gate.sh`'s own NOT-covered trailer names the validator, wpt, test262, the geodesic corpus, the
render corpus and the client -- **and not this suite**. So the trailer that exists to stop a green
run being read as coverage is itself short by the one family that judges the tree.

**The tree already says so and nobody is listening:** `AGreenTrailerNamesWhatItDidNotJudge` is one
of the twelve, and its own message is that every family whose corpus holds no fetched subject must
be named by the runner. `test/scripts` is such a family and is not named.

## How this was found

Not by reading the gate. `make test` was run after an unrelated change and reported
`1284 tests: 1269 PASS 14 FAIL`, while `make lint` and `sh test/gate.sh`'s own suites were green.
One of the fourteen was that change's own defect -- the client had named a path under `test/` --
and it was invisible to every fast path.

## What will be true

- [ ] `test/gate.sh` names `harness/claims` as a step, or names it in the trailer with the REASON
      it is not run and the count that stands red
- [ ] the twelve are triaged: each is either repaired, or carries a declared shrink-only count the
      way `lint` does, or is deleted with its reason. A rule nobody intends to satisfy is not a rule
- [ ] Negative control: a source that breaks one of the twelve turns the FAST gate red, not the
      full run an hour later

## What this does NOT cover

Whether the twelve are each correct. Two of them (`TheNestRefusesASecondRunner`,
`TheCorpusIsRebuiltByOneCommand`) may be measuring the harness's own concurrency rather than a
defect, and a rule that cannot pass is the same fault as a proof that cannot fail. The triage box
is where that is decided, one at a time, and it is deliberately not decided here.
