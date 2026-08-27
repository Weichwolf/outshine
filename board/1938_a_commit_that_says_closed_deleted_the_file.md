Type: bug
State: open
Area: board, test
Tags: measured, guard, logbook


**Benchmark** — Neither engine has a board, so neither answers this. **The choice is mine**: a closure that deletes the file puts what was said in the commit that removed it, which is the only place a reader looks for what happened.
# A commit whose subject says an item is CLOSED deleted that item's file

CLAUDE.md: *Closing is DELETING the file.* `harness/claims/AnItemReachesClosedThroughActive`
guards one direction of that rule -- every DELETION passed through `State: active` -- and nothing
guards the other. So a commit may announce a closure and leave the item standing, and
`git log --grep 'board:NNNN'` then says done while `board/` says open.

Two items are in that state at HEAD, both inside the new claim's own window:

    873f8f65  "board:1915 closed -- a wait for a tile can end"
              touched 5 files, none of them board/1915_a_wait_for_a_tile_can_end.md
    ab065d14  "board:1927 closed, 1928 filed"
              touched 3 files, none of them board/1927_...md

Measured: `git log --since='4 days ago' --format='%h|%s' | grep -i ' closed'` against
`git show --diff-filter=D --name-only`. Both files still carry `State: open`.

The claim's own prose is wrong about one of them: it states that board:1927 was "filed and
deleted in one breath, which is what happened ... by the hand that wrote this"
(`AnItemReachesClosedThroughActive.cpp:16-18`). 1927 was filed at 40d11012 and never deleted.

## What will be true

- [ ] For every commit in the window whose subject says `board:NNNN closed`, `board/NNNN_*.md`
      is absent at that commit -- the same walk, facing the other way.
- [ ] The two directions are one claim, because they are one rule.
- [ ] Negative control: a commit announcing a closure whose file survives, and the claim names
      the number, the commit and the state the file still carries.
