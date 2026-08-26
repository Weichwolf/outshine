Type: bug
State: active
Area: test
Tags: board, guard, measured

# The rule that closing passes through active is ENFORCED, not merely written down

CLAUDE.md names `harness/claims/AnItemReachesClosedThroughActive` and the claim does not exist:
`git log --diff-filter=D` shows it was deleted at 0abecf4e. So the rule stood written down with
nothing checking it.

Measured over all of history: **2579 board deletions, 2513 of them straight from `State: open`.**
Nobody followed it, including the session that filed this item, twice in one afternoon.

`State: active` is the only place the board says what has an owner right now.
`grep -l '^State: active' board/*.md` is how the architect, the stakeholder and the next session
after a compaction all answer "what is being worked on". An item that reaches closed without
passing through it was worked where nobody could see it, and two people picking the same item is
what the one extra commit buys off.

## Why the history is not declared as a standing red

It cannot be driven to zero: the commits are the logbook. 2513 declared reds would be a number
nobody can ever clear, and a red nobody can clear is one people learn to read past. The claim
walks from the commit that CREATED it -- derived at run time, never a quoted hash, and anchored
on the creation rather than the last edit so that improving a sentence in it cannot hand out an
amnesty over every closure behind it.

## What will be true

- [x] The claim exists and runs in the gate.
- [x] Its window is derived from git, states its own start, and prints NO CLOSURE IN THE WINDOW
      YET in capitals when it is empty -- so "nothing to judge" cannot read as "everything
      passed".
- [x] Proving case: `harness/claims/AnItemReachesClosedThroughActive`, and the first closure it
      judges is this item's own. Negative control: the window anchored on an older path so that
      it spans the unenforced history, and the claim reports 1429 items that skipped the door.
