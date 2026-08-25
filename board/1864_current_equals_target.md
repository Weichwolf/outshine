Type: feature
State: open
Area: architecture
Tags: measurement, review

# CURRENT equals TARGET

The distance between the two maps in CLAUDE.md is the work list, and this item is where its
measurement lives — because the measurement CHANGES, and changing content belongs in `board/`,
not in a brief and not in the map itself.

**The hourly architect appends one row per round.** Both figures are derived by counting the
colours in CLAUDE.md's CURRENT diagrams: today's from the file as it stands, the previous
round's from `git show <last review commit>:CLAUDE.md`. Nothing here is stored that could be
counted.

`green-and-reached / total` is the figure: a green node whose only path to a client runs through
a red one draws no pixel, so it counts in the denominator and not the numerator.

## The measure

| round | diagram | green | amber | red | stranded | total | green-and-reached |
|---|---|---|---|---|---|---|---|
| 2026-08-25 07:17 | class structure | 44 | 15 | 4 | 5 | 68 | 44/68 = 65 % |
| 2026-08-25 07:17 | render plan | 9 | 1 | 2 | 0 | 12 | 9/12 = 75 % |

## What will be true

- [ ] Every node of every CURRENT diagram is green and reached, and the two maps are the same
      drawing.

## Comments

- 2026-08-25 — opened so the measurement has a home. The first row is the architect's own from
  its 07:17 round; `Engine` went green → amber that hour on the queue's own commit, which is why
  the share fell while a node was added.
