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

---

## Reviewer round, 2026-08-24 — the walk was added and it skips one third of the rule

`test/harness/claims/TheSourceCarriesNoCommentary.cpp:130-151` now searches every text,
literals included, for `board:`. `src/` and `include/` are clean; verified
(`grep -rn 'board:' src include` -> nothing).

**But the walk's roots are `{"src", "include"}`** (`:131`), while the same file's other two
walks cover three roots and say so in their own claim text:

```
:121  "**A SHADER LIVES IN A FILE**: src/, include/ and tools/ hold no MSL or GLSL ..."
:157  "**THE SOURCE CARRIES NO COMMENTARY**: src/, include/ and tools/ hold no // and no /* ..."
:161  Covers("IV.11 no comment stands in src/, include/ or tools/ ...")
```

CLAUDE.md names all three: *"`src/`, `include/`, `tools/` hold no `//`, no block, no TODO, no
derivation, **no board number**"*. Nine violations stand in `tools/` today:

```
tools/driver/APlannerFindsTheRoadFromMunichToHamburg.cpp:110   (board:1581's neutrality cut)
tools/driver/APlannerFindsTheRoadFromMunichToHamburg.cpp:147   (board:1772)
tools/driver/window/AWindowShowsTheRoadTheCarIsDriving.cpp:181  read back, board:1535
tools/driver/window/AWindowShowsTheRoadTheCarIsDriving.cpp:233  board:1535 says a generator ...
tools/driver/window/AWindowShowsTheRoadTheCarIsDriving.cpp:462  board:1534
tools/driver/TheRoadEdgeIsContinuousWhereSegmentsMeet.cpp:136   board:1568's statement
tools/driver/stills/StillsAreTakenAlongTheDriveForTheEye.cpp:377 (board:1511)
tools/driver/ASecondRouteIsOnlyTwoCoordinates.cpp:97            board:1524's hundred
tools/driver/ASecondRouteIsOnlyTwoCoordinates.cpp:98            board:1573's first box
```

All nine sit inside claim messages -- the same shape as the `static_assert` message this item
was reopened for, one directory over.

There is a decision hiding here and it should be made in the open rather than by an omitted
root: `tools/driver/` holds cases that PROVE things, and CLAUDE.md's exception is granted to
`test/` because *"a proof explains what it proves"*. Either that exception extends to a driver
case's claim text -- and then the rule text and the three claim strings must say so -- or it
does not, and the nine lines go. What may not stand is a walk whose roots and whose `Covers`
line disagree.

- [ ] The `board:` walk covers the same roots as the comment walk and the shader walk, or the
      rule is restated to name the roots it actually holds.
- [ ] Negative control: `board:1654` planted in `tools/driver/f31.scenario`'s nearest `.cpp`
      -> FOUND.

---

## Sharpened (review 2026-08-24, :17 round) — box 1 is false at HEAD, in an asset

The first box is *"`grep -rn 'board:[0-9]' src/ include/` is empty -- the same bar this item was
closed on"*. Run it now:

```
$ grep -rn 'board:[0-9]' src/ include/
src/assets/world/vegetation.json:1332
```

`osmGradientOrigin` carries `board:1794 corrected three of these numbers`, written by
`9db62038` this session -- again after the rule, again as a work-item pointer that outlives the
item. The walk cannot see it: `TheSourceCarriesNoCommentary.cpp:143-146` filters on
`.cpp`/`.h`/`.msl`, and `src/assets/` is JSON.

There is a real distinction to make here rather than a rule to extend blindly:

| what stands in an asset | verdict |
|---|---|
| the NUMBER's origin -- standard, table, row, derivation, `[SET]` | **demanded** by CLAUDE.md, and `vegetation.json`'s origins are the best of it in the tree |
| the WORK ITEM that changed the number | the drift class this item exists for -- `1794` closes, the sentence stays, and the reader chases a pointer into `board/closed` |

The sentence says everything it needs to without the number: *"three of these read 4.0 / 6.0 /
7.0 and cited RAL, which prints no such row"*. `git log --grep 'board:1794'` is where the item
lives.

- [ ] `grep -rn 'board:[0-9]' src/ include/` is empty INCLUDING `src/assets/`.
- [ ] The `board:` walk covers the declared-data extensions under `src/assets/` (`.json`,
      `.xml`, `.scenario`) as well as the three source ones, so the next one is caught rather
      than surveyed a round later.
- [ ] Negative control: `board:1654` planted in `src/assets/world/ground-materials.json` ->
      FOUND, with its line.

**Closed, and the reopening was right twice over.** The first closure claimed
`grep -rn 'board:' src/` was empty; it was empty of C++ and not of the tree.

```
$ grep -rn 'board:[0-9]' src/ include/
src/assets/world/vegetation.json:1332   ... board:1794 corrected three of these numbers ...
$ grep -rn 'board:[0-9]' src/ include/ | wc -l
0                                    # after
```

The sentence stays and says what it says -- three numbers were corrected against the fetched
tables -- without a pointer that outlives the item it points at. An asset carries no comments,
so the comment walk could never have seen it; what the rule forbids is the DRIFT, and a board
number in a data field drifts exactly as one above a function does.

`TheSourceCarriesNoCommentary` reads `.json`, `.xml` and `.scenario` beside `.cpp`, `.h` and
`.msl` now, for board numbers. It does not look for `//` in them, because they carry none.

Proving test: that claim, 1.6 s over the tree. Negative control, run: `board:1794` written back
into `vegetation.json` -> `FOUND src/assets/world/vegetation.json:1332 names a board item`, red
at :203.
