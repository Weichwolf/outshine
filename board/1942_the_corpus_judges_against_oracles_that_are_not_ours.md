Type: feature
State: open
Progress: corpus
Area: test
Tags: benchmark, target

# Every case is a scenario against an oracle whose truth does not depend on our design

**Benchmark** — Unreal: automation tests assert against known-good data or physical truth. RAGE: the same. **Both agree** — a case asserting the shape of our own architecture specifies nothing while the architecture moves.

Neither benchmark publishes a comparable gate, so this is the area where outshine is AHEAD and
the list is about keeping it honest rather than catching up. 1844 cases run against Khronos, WPT,
test262 and GeographicLib.

- [ ] the gate's verdict names every red it can produce; `unprepared` and `unlinked` have a
      channel, `undeclaredSkips` and `compileBlind` do not (board:1923)
- [x] the build declaration audits itself, and its own controls seed against what the
      declaration holds AT RUN TIME rather than a path quoted when they were written
      proof: harness/claims/TheBuildDeclarationAuditsItself
- [x] closing an item passes through `State: active`, checked from a self-anchored window
      proof: harness/claims/AnItemReachesClosedThroughActive
- [x] no source in `src/`, `include/` or `apps/` carries a comment
      proof: harness/claims/TheSourceCarriesNoCommentary
- [ ] every suite reaches the library through `include/` alone -- 15 of 17 are granted `-Isrc`
      today (board:1879)
- [ ] a commit whose subject says CLOSED actually deleted that item's file (board:1938)
- [ ] one bad source fails ONE case and never the whole gate (board:1869)
- [ ] `test/CORPORA.md` names, for every capability of TARGET, which established corpus asserts
      it and at what grade (board:1877)
