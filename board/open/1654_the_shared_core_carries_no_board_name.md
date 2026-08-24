Type: bug
Area: render
Tags: hygiene

# The shared core carries no board name

src/render/stages/MediumCore.h:4-7 opens with a four-line block naming board:1580 — the only
site in src/ that spells a board item (grep -rn 'board:' src/ finds exactly this one). The
house rule is explicit: the code carries no commentary; work items live in board/, code never
names them. `git log --grep 'board:1580'` already carries the linkage; a number in the header
is the drift class the rule exists for (the item closes, the comment stays, the next reader
follows a stale pointer).

The dialect seam itself — the including side defines MEDIUM_CONST, MEDIUM_THREAD and
OUTSHINE_PI for its language before including — is a legitimate non-obvious why and may keep
ONE line saying that much. The board number, the file inventory of who includes it, and the
"scalar physics only" narration go; ParticipatingMedium.h:48-54 is the visible proof of the
contract either way.

---

Closed: the comment names the mechanism, not the item -- "THE ONE SOURCE: compiled as C++ by
the reference ... appended as MSL text" stands, board:1580 is gone from src/ entirely
(grep -rn 'board:' src/ is empty). git log carries the provenance, as the rule demands.

---

**REOPENED by the hourly review, 2026-08-24. The closing proof is false at HEAD.**

This item was closed with: *"board:1580 is gone from src/ entirely (grep -rn 'board:' src/ is
empty)"*. Run it now:

```
$ grep -rn 'board:[0-9]' src/ include/
src/actor/path/SpeedProfile.cpp:13:  "every term that can bind the plan carries a name (board:1787)"
```

Introduced by `727acba5`/`e0f87385` — this hour's own work, written after the rule
(`board:1763`, owner directive 2026-08-23: *"not a TODO, not a board number"*; CLAUDE.md:
*"work items live in `board/`, code never names them"*). Filed harder for that reason.

## The walk cannot see it, and that is the second half of the defect

`test/harness/claims/TheSourceCarriesNoCommentary.cpp:41-63` skips string literals by design
— correctly, because `src/core/Script.cpp:101` holds `"//"` as the data it parses. But the
rule `board:1763` states is not *"no `//`"*, it is *"not a line above a function, not a
trailing note, not a derivation, not a TODO, **not a board number**"*. A board number inside
a `static_assert` message is prose beside the code with the exact drift the rule exists for:
1787 closes, the string stays, the next reader follows a stale pointer out of a diagnostic
the compiler prints.

`TheSourceCarriesNoCommentary` passed on the same tree that carries the violation
(measured this round, isolated nest, `40 PASS`), so the claim certifies a rule narrower than
the one it names in its own `Covers` line.

## What must be true

1. `grep -rn 'board:[0-9]' src/ include/` is empty — the same bar this item was closed on.
2. `TheSourceCarriesNoCommentary` walks literals for `board:<digits>` in `src/` and
   `include/` and refuses with `file:line`, so the bar is a walk and not a habit. Its
   `Covers` line then matches the rule it enforces.
3. `tools/` is decided explicitly rather than by omission: `tools/driver/` carries seven
   `board:NNNN` sites, all inside `CHECK`/`Covers` message strings of driver proofs
   (`tools/driver/APlannerFindsTheRoadFromMunichToHamburg.cpp:110,147`,
   `tools/driver/ASecondRouteIsOnlyTwoCoordinates.cpp:97-98`,
   `tools/driver/TheRoadEdgeIsContinuousWhereSegmentsMeet.cpp:136`,
   `tools/driver/stills/StillsAreTakenAlongTheDriveForTheEye.cpp:377`,
   `tools/driver/window/AWindowShowsTheRoadTheCarIsDriving.cpp:181,233,462`). Either
   `tools/driver/` is proof prose and the rule says so, or the seven go. Silence is how the
   eighth arrives.

## Comments

- 2026-08-24 — the static_assert message is not text the language requires: `static_assert`
  takes a condition alone since C++17. The message is a choice, and a choice inside `src/` is
  subject to the rule.

## Reopened, and the regression came from this session (2026-08-24)

`grep -rn 'board:' src/` was the proof this item closed on. It was FALSE at HEAD:

```cpp
"every term that can bind the plan carries a name (board:1787)");   // SpeedProfile.cpp:14
```

A `static_assert` message is prose the compiler carries, and since C++17 the condition alone
suffices -- so the message is a CHOICE, and a choice in `src/` is under the rule. Written by
board:1787's own repair, one hour after board:1763 made the rule absolute.

The second half is why nothing caught it: `TheSourceCarriesNoCommentary` deliberately steps
over string literals, because `Script.cpp` and `Style.cpp` parse `//` and `/*` as data. It
therefore could not enforce the rule its own `Covers` names, and it was green on the same tree.

The claim now walks `src/` and `include/` for `board:` in ANY text, literal or not.

- **Negative control**: `(board:1787)` put back into the assert message ->
  `FOUND src/actor/path/SpeedProfile.cpp:14 names a board item`. Reverted.
- Still open: `tools/driver/` carries seven `board:NNNN` inside CHECK strings. Those files are
  PROOFS -- they hold `main`, `CHECK` and `Covers` and run under `test/run.sh` -- but they
  live under `tools/`, which CLAUDE.md names alongside `src/` and `include/`. Either the rule
  says proofs may cite their item wherever they live, or those seven go. The walk covers
  `src/` and `include/` only, so it does not pretend to have settled it.
