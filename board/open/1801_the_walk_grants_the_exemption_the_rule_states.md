Type: bug
Area: test
Tags: process
Regresses: 1654

# The walk grants the exemption the rule states

`CLAUDE.md` states the comment rule and its one exemption in a single sentence:

> *The code carries NO comments — `src/`, `include/`, `tools/` hold no `//` ... and `test/` is
> the one place prose may stand because a proof explains what it proves -- **and a proof is a
> source that carries `Covers(`, wherever it lives, so the driver cases under `tools/` are
> proofs and the library sources beside them are not***

`test/harness/claims/TheSourceCarriesNoCommentary` enforces it in two walks, and grants the
exemption in only one of them:

| walk | line | exempts `Covers(` sources |
|---|---|---|
| board numbers in source | `:141` `if (text.find("Covers(") != std::string::npos) { continue; }` | **yes** |
| comments in source | `:76-87` | **no** |

The comment walk's own neighbour says the rule out loud (`:131`):

```
// A PROOF may cite the item it proves, wherever it lives -- and a proof is a source carrying
// Covers(, which is why the driver cases under tools/ are exempt and the library sources
// beside them are not.
```

So the walk knows the rule, writes it down, and applies it to half of what it enforces. A
driver case under `tools/` -- `tools/driver/window/AWindowShowsTheRoadTheCarIsDriving.cpp`
carries `Covers(` at `:509` -- may cite its board item and may not explain its own fixture.

**Found by writing one.** Working `board:1778` I put a six-line note above a budget arm in that
case, explaining why two whole-route claims are skipped when the drive is cut short. The walk
called it a defect at `:291`. It is not one under the rule as written -- and it would be
equally not one if someone else had written it, which is the only reason this is filed rather
than quietly deleted.

Measured: **no source under `tools/` without `Covers(` carries a comment today**, so the walk
has been enforcing the stricter rule and nothing in the tree had to notice the difference.
That is why it survived `board:1654`'s round.

## What will be true

- [x] The comment walk grants the same exemption the board-number walk does, so the rule the
      file states is the rule the gate enforces.
- [x] A library source under `tools/` -- one with no `Covers(` -- that carries a comment is
      still a defect, named.
- [x] Proving test: the claim itself. Negative control: a comment put into
      `tools/viewer/parts/Chrome.cpp`, which carries no `Covers(` -> red, named at its line.

## Comments

- 2026-08-24 -- repaid. The comment walk grants the exemption the board-number walk beside it
  already granted, so the rule the file states is the rule the gate enforces.
- **The needle carries the quote.** `Covers("` and not `Covers(`. The first negative control
  for this repair put the line

  ```
  // a comment in a library source under tools that carries no Covers(
  ```

  into `tools/viewer/parts/Chrome.cpp` -- and the claim stayed green, because the comment
  spelled the word that exempts the file. A narration line must not be able to exempt the file
  that carries it. Both walks take the quoted form now.
- **Negative control**, run with the tightened needle: a plain narration line in
  `tools/viewer/parts/Chrome.cpp`, which carries no `Covers("` ->

  ```
  FOUND tools/viewer/parts/Chrome.cpp:302 narrates
  25 tests: 24 PASS  1 FAIL
  ```
- Owner directive the same session: `CLAUDE.md` may not name a specific client. The rule is
  stated generally there now -- a proof is any source carrying `Covers("`, wherever it lives --
  and the sentence that named a client directory is gone, along with three other mentions.
- Gate 240/240, 81 643 ms of run.

---

## REOPENED by the hourly review, 2026-08-24 — the needle is still spellable by the text it exempts

The closure's own comment states the failure mode and then leaves it standing one character
deeper:

> *A narration line must not be able to exempt the file that carries it. Both walks take the
> quoted form now.*

`Covers("` is as writable inside a comment as `Covers(` was. Measured, isolated worktree at
`959a0d23`, one line appended to a library header in `src/`:

```cpp
// src/core/Span.h:45
// this narration mentions Covers("IV.11") and therefore exempts its own file
```

```
PASS    harness/claims/TheSourceCarriesNoCommentary        69 ms
27 tests: 25 PASS  1 FAIL  0 TIMEOUT  1 SIGNAL
```

**Green.** A comment in `src/core/` -- not a proof, not under `tools/`, not near a case --
disables the walk for the whole file, and every other comment in that file with it. The rule
the owner made binding and absolute is enforced by a substring the forbidden text may contain.

## Why tightening the needle again is the wrong repair

`Covers("`, `Covers(\"`, a regex, a longer literal -- each is the same construction and each is
spellable in the prose it is meant to exclude. The exemption is currently a property of the
FILE'S BYTES; it has to be a property of the file's ROLE, and the runner already knows that
role: a proof is a translation unit `test/run.sh` builds and RUNS as a case, and `TESTS_ALL` is
the list.

## What must be true

- [ ] The exemption is decided by role, not by content: the walk takes the runner's own case
      list (or the same enumeration `test/run.sh` performs -- a source that is the case of a
      declared suite), so a library source cannot exempt itself by quoting the harness.
- [ ] The needle, if one is kept at all, must be something a comment cannot legally contain --
      and there is no such string, which is the argument for the role.
- [ ] Negative control, and it is the one this item now owes: the line above put back into
      `src/core/Span.h` -> `FOUND src/core/Span.h:45 narrates`, red. The run above is the green
      the repair must turn.
- [ ] The same hole stands in the `board:` walk beside it (`:141`), which took the identical
      exemption in the identical form. Both, one repair.
