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

## Measured 2026-09-05, after the round that read them

```
  green now  AGreenTrailerNamesWhatItDidNotJudge  the runner counted case DIRECTORIES two deep and
                                                  never saw a one-level family; it counts manifests
             APreparedFileNeverLandsInTheTree     a reference per FRAME (reference.f0000.png) is the
                                                  oracle's picture too; the claim knew only reference.png
             EveryRenderNamesItsIndices           UNPREPARED, not red: no render row exists until
                                                  `make test` renders the corpus again
             TheBuildDeclarationAuditsItself      src/client is `make`'s one program, not a suite's source
             TheCorpusIsRebuiltByOneCommand       opendrive/a9 and clearsky/egbert were a third manifest
                                                  schema with two fetch scripts of their own; they are
                                                  declared-case manifests now, planned and FETCHED by the
                                                  one preparer, digests checked (20.6 MB, 160 KB)
             TheNestRefusesASecondRunner          passes; the refusal names both nests now, so the next
                                                  time it does not the cause is in the log
             NoFramePathCallReachesABlock         the step published three measures through a ledger that
                                                  allocates -- the picture publishes them now; the depth
                                                  pyramid was read back with a WAIT every audited frame --
                                                  Readback::Enqueue and Poll, a fence queried a frame
                                                  later; the walk prints the call chain to each block
  red        EveryOracleWasPreparedByThisPreparer 1 239 of 1 239: this round changed prepare.py (the two
                                                  case trees) and restored the khronos fetch step that
                                                  board:2049 deleted with the render harness, so every
                                                  provenance names an older preparer. The way back is the
                                                  one command over every case, measured at 5.2 s for Box
                                                  with Blender 5.2.0 -- about two hours -- and it runs
                                                  after this commit, because the digest is the tree's
  red        TheEngineNamesNoSubject              board:2139's: Seat/Wheel/Door in the door, TerrainField
                                                  in HeightSheets
```

Found on the way and repaired here: `prepare.py fetch` on any khronos/glTF case raised
AttributeError since board:2049 removed test/harness/khronos/glTF with the render harness -- the
fetch step went with it and the dry-run that the corpus claim runs never called it. The step
stands at test/harness/khronos/prepare/fetch.py, where harness_of finds it for glTF and generator.

## What will be true

- [x] each claim read against the tree as it stands, repaired or rewritten with its reason
- [ ] `make lint` prints `0 guard(s) are RED`: EveryOracle after the re-prepare, TheEngineNamesNoSubject
      with board:2139
- [ ] Negative control: a claim's own negative control still goes red after the rewrite -- the
      frame-path walk's chain names Readback::Land when a wait is put back
