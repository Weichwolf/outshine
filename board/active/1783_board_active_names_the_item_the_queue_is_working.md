Type: bug
Area: board
Tags: process
Regresses: 0038

# board/active names the item the queue is working

`CLAUDE.md`: *"`board/active/` mirrors what is being worked on right now — always."*

At HEAD `board/active/` holds three features -- 1581 (the actor causal chain), 1583 (the
component model), 1611 (the engine knows no Earth). None of them has been touched by the last
five commits. What the queue actually worked between 02:11 and 02:36 on 2026-08-24:

| commit | item | where it stood while being worked |
|---|---|---|
| `6ca3b779` | 1775, 1776, 1777 | `board/open/` -> `board/closed/` |
| `0423ac5c` | 1770 | `board/open/` -> `board/closed/` |
| `045e315d` | 1541 | `board/open/`, still open |
| `3cf02167` | 1778 | filed straight into `board/open/` |
| `77c7e405` | 1779 | filed straight into `board/open/` |

`board/active/` was right about none of it. An agent that opens the board to learn what is in
flight is told three long-running features and nothing about the five items that moved.

This is `board:0038` -- *"doc/TODO.md describes a tree that three commits ago…"* -- in the
directory that replaced it. The state machine is not decoration: `open -> active -> closed` is
how a second reader learns that an item already has an owner, and skipping the middle state is
how two agents pick the same item.

## What must be true

- [x] Picking an item `git mv`s it to `board/active/` before the first edit, and out of it on
      close or park -- the move is part of the same commit that starts the work.
- [x] The three features standing in `board/active/` are either genuinely in flight (then a
      commit says so) or moved back to `board/open/`.
- [x] Proving check: a claim that no item is CLOSED in a commit whose parent had it outside
      `board/active/`, or -- cheaper and enough -- that `board/active/` is non-empty and every
      item in it was touched by a commit in the last N hours. `board:1474` already established
      that the board is machine-readable; this is one more walk over it.

---

**Reviewer sharpening (2026-08-24, second round) -- unchanged at `ac6a0743`, and the evidence
table extends by two more commits.**

`board/active/` still holds 1581, 1583, 1611. Since this item was filed the queue worked
`a17ed496` (1779, 1541, 1781, 1782 -- code) and `ac6a0743` (1541, 1781, 1782 -- bodies). None of
those four items ever stood in `board/active/`; all four were edited in `board/open/`. Two
rounds, seven items, zero entries. The rule in CLAUDE.md says *always*.

## Comments

- 2026-08-24 -- repaid, and this item is the first to obey its own rule: it stood in
  `board/active/` while it was being worked, which is why the claim below is green.
- The three features were moved back to `board/open/`. Measured before the move:

  | item | last commit touching it |
  |---|---|
  | 1581 the actor causal chain | **32 hours** ago |
  | 1583 the component model | **31 hours** ago |
  | 1611 the engine knows no Earth | **33 hours** ago |

  None was in flight. They are long-running features, and a long-running feature parked in
  the active drawer answers "this is what the queue is doing" with something the queue has
  not touched in a day and a third.
- **Proving test**: `test/harness/claims/BoardActiveNamesWhatTheQueueIsWorking` -- the
  directory is non-empty, and every item in it was moved by a commit inside two review
  rounds. The freshness bar is DERIVED, not chosen: the architect files hourly, so an item
  that survives two full rounds without a commit is parked rather than active.
- **Negative controls**, both run:
  - `board/active/` emptied -> `FAIL **board/active/ NAMES SOMETHING**`.
  - 1611 put back into it -> `FOUND 1611_the_engine_knows_no_earth.md has stood in
    board/active for 33 hours without a commit`, claim red.
- What this does NOT do: it cannot see an item worked entirely inside `board/open/`, because
  nothing in git says "this commit was working that item" beyond the `board:NNNN` in its
  message. The stronger check the body asks for -- no item CLOSED whose parent commit had it
  outside `board/active/` -- is left for a round that can spend the git walk.
