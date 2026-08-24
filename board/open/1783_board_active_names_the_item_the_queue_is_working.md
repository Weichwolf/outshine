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

- [ ] Picking an item `git mv`s it to `board/active/` before the first edit, and out of it on
      close or park -- the move is part of the same commit that starts the work.
- [ ] The three features standing in `board/active/` are either genuinely in flight (then a
      commit says so) or moved back to `board/open/`.
- [ ] Proving check: a claim that no item is CLOSED in a commit whose parent had it outside
      `board/active/`, or -- cheaper and enough -- that `board/active/` is non-empty and every
      item in it was touched by a commit in the last N hours. `board:1474` already established
      that the board is machine-readable; this is one more walk over it.
