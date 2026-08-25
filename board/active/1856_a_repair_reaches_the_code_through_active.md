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

- [ ] A commit that repairs code under an item's name finds that item in `board/active/` --
      proven by a walk over the same range `ACommitCarriesTheItemItNames` binds.
- [ ] Proving test: the walk, over history from the commit that states the rule. Negative
      control: a repair commit staged against an item left in `board/open/` -> FOUND.

## Comments

- 2026-08-25 -- filed by the queue against itself, from the hourly review's process finding.
