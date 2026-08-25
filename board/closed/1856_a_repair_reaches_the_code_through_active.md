Type: bug
Parent: 1802
Area: test
Tags: claims, board, process

# A commit that repairs an item finds that item in board/active/

`harness/claims/AnItemReachesClosedThroughActive` proves the board's state machine at its exit:
an item file that appears in `board/closed/` without ever having been in `board/active/` is
found. Nothing guards the entry.

This session repaired `board:1844`, `board:1845` and `board:1846` in `src/` and `test/` while all
three sat in `board/open/`, and moved them to `active/` only when closing them. The gate was
green throughout. `board/active/` is CLAUDE.md's answer to *"what is being worked on right
now"* -- a directory that fills up at closing time answers nothing.

The rule is checkable from the same walk `board:1802` built: for every commit whose message
names `board:NNNN` and whose diff touches a file outside `board/`, item NNNN must have been in
`board/active/` in that commit's parent tree, or be moved there BY that commit.

## What will be true

- [x] A commit that repairs code under an item's name finds that item in `board/active/` --
      proven by a walk over the same range `ACommitCarriesTheItemItNames` binds.
- [x] Proving test: the walk, over history from the commit that states the rule. Negative
      control: a repair commit staged against an item left in `board/open/` -> FOUND.

## Comments

- 2026-08-25 -- filed by the queue against itself, from the hourly review's process finding.

## Closed 2026-08-25 -- IV.33, and three shapes the first draft got wrong

`test/harness/claims/ARepairFindsItsItemInActive` walks every commit from its own birth that
touched a file outside `board/` while naming an item, and asks whether that item stood in
`board/active` -- in the commit's own tree or its parent's.

Three corrections, each found by running it rather than by reading it:

| the draft | what it did | the fix |
|---|---|---|
| `git ls-tree --name-only <commit> board/` | returned `board/open`, `board/active`, `board/closed` -- three directory names, no item files, so every naming judged zero and the walk reported 0 of 3 commits | `-r` |
| `NamedIn(message)` over the WHOLE message | *"board:1854's control was blind"* in an explanation counted as a claim to be working on 1854 -- a false find on this session's own commit | the SUBJECT is the assignment; the body cites its neighbours |
| the parent tree alone | an item filed and worked in one commit does not exist where the walk looked | both trees are asked |

The second is the design point worth keeping: IV.23 reads the whole message because there every
file touched must be named SOMEWHERE. This claim asks which item a commit is FOR, and that is
the subject line.

Proving test: `harness/claims/ARepairFindsItsItemInActive` (IV.33), 3 commits and 3 namings
judged at closing. Negative control, run: a commit changing `test/` with the subject
`board:1855 -- a repair staged against an item still sitting in board/open` ->
`FOUND 7eaa343e1 repairs code under board:1855, which stood outside board/active` and
`FAIL ...:104 A REPAIR FINDS ITS ITEM IN board/active`. The control commit was reset away after
the measurement.
