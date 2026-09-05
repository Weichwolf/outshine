Type: bug
State: open
Area: test, harness
Tags: owner, audit

# The seven harness claims are read against today's architecture, and `make lint` is green

**Benchmark** -- Unreal's automation gate and RAGE's build verification are green or the
change does not land; neither keeps a red test in the gate as documentation. Here the gate is
`make lint`, and measured 2026-09-05 its claim block held twelve red claims at HEAD while the
counters (tidy 93, Doxygen 702, unreached 62) were read and reported as the gate. Five were
this session's and are repaired in the same round; seven remain, and the owner's word for them
is that a claim can become FACTUALLY WRONG when an architecture decision moves or code is
refactored, so each is re-read before it is repaired.

## The seven, with what each says today

```
  AGreenTrailerNamesWhatItDidNotJudge   test/opendrive, clearsky, scripts hold no case and the
                                        runner is silent about it
  APreparedFileNeverLandsInTheTree      a prepared file stands in a case directory
  EveryRenderNamesItsIndices            render rows across the prepared corpus = 0; the corpus
                                        render pass is gone since the places are the test
  NoFramePathCallReachesABlock          the physics step and the picture reach an allocation, a
                                        lock or a wait -- the one claim here that is about the
                                        ENGINE and not the harness; board:2130 owns the frame path
  TheBuildDeclarationAuditsItself       a source listed twice or in no suite
  TheCorpusIsRebuiltByOneCommand        planned != declared manifests
  TheNestRefusesASecondRunner           the nested invocation did not pass through the lock
  TheEngineNamesNoSubject               inMotor 42: Seat/Wheel/Door in the door, TerrainField in
                                        HeightSheets -- board:2139's, and recorded there
```

## The solution

- each claim is read against the tree as it stands: if the invariant still holds, the tree is
  repaired; if the invariant moved (the corpus render pass, `make test` suspended), the claim
  is rewritten to the invariant that replaced it or deleted with its reason in the commit
- `NoFramePathCallReachesABlock` is not a harness question and goes to board:2130 with its walk
- `make lint`'s trailer is read whole before every commit, not its counters

## What will be true

- [ ] `make lint` prints `0 guard(s) are RED` and its claim block is all PASS
- [ ] every claim deleted or rewritten names the architecture decision that moved it
- [ ] Negative control: a claim's own negative control still goes red after the rewrite
