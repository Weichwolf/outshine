Type: bug
Area: board
Tags: process

# A commit message describes the commit it carries

`CLAUDE.md` makes the log the record:

> *a number's origin lives in its board item and its commit* · *`git log` is what was — no journal*

Three commits this session carry a message about one thing and a diff containing another,
because `git add -A` swept a working tree that held unrelated finished work:

| commit | says | also carries |
|---|---|---|
| `138c798d` (amended to `fa28d4f1`) | board:1799 filed | `test/run.sh`'s rebuild, three glTF twins |
| `f289291b` | board:1573 sharpened | the whole `tools/driver` -> `apps/driver` move and its `run.sh` changes |

The amend fixed the first. The second is in the log as it stands. A reader running
`git log --grep 'board:1797'` finds the item's own commits and not the tree move that its
mechanism required, and a reader of `f289291b` is told about a requirement document while
reading a directory rename.

This is not cosmetic. `board:1474` made the board machine-readable and the commits are its
other half: `git log --grep 'board:NNNN'` is the documented way to find every commit on an
item, and it is only as true as the messages.

## What will be true

- [ ] Work is staged by what it is, not swept: `git add <paths>` per commit, and a commit that
      would carry two unrelated changes becomes two commits.
- [ ] Proving check: a claim that walks recent commits and flags one whose message names a
      board item while touching paths no other commit for that item touches -- or, cheaper and
      honest, this item stands as the record that it happened and what it cost.
- [ ] The two commits above are named here rather than rewritten. History is what was.

---

## It happened again, this hour, to another agent's work in flight (2026-08-24, reviewer round)

`d94d39eb board:1781 active -- its two remaining boxes` carries, besides the `git mv` its
message describes, the reviewer's REOPENING of two unrelated items:

```
$ git log --oneline -3 --name-status -- board/open/1801_the_walk_grants_the_exemption_the_rule_states.md
d94d39eb board:1781 active -- its two remaining boxes
A	board/open/1801_the_walk_grants_the_exemption_the_rule_states.md
```

Same for `board/open/1806`. Both were `git mv`-ed out of `board/closed/` by the hourly review
while `board:1781` was being worked in the same nest; `git mv` stages immediately, so the next
sweeping commit took them. A reader of `git log --grep 'board:1806'` is not told that the item
was reopened, and a reader of `d94d39eb` is told about a species table.

This sharpens the item in a direction its first filing did not have: it is **not only about one
author's staging discipline.** Two agents share one worktree and one index. `git add -A` /
`git commit -a` in that setting is not sloppiness, it is a race -- whatever the other party has
staged goes into your commit, under your message, and neither of you can see it afterwards
without a path-scoped log.

- [ ] The rule is stated as a rule and not as a habit: **every commit stages by explicit path.**
      `git add -A` and `git commit -a` are the two spellings that cannot be correct in a shared
      nest.
- [ ] The hourly reviewer's own instruction ("one commit per run over all board changes") is
      compatible with that -- it names its paths.
