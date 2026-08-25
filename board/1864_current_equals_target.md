Type: feature
State: open
Area: architecture
Tags: measurement, review

# CURRENT equals TARGET

The distance between the two maps in `CLAUDE.md` is the work list, and this item is the
definition of done. The measurement itself lives in `CLAUDE.md` under *The distance to TARGET*,
one row per diagram per round, rewritten by the hourly architect from the file as it stands and
from `git show <last review commit>:CLAUDE.md` — nothing is stored that could be counted.

`green-and-reached / total` is the figure: a green node whose only path to a client runs through
a red one draws no pixel, so it counts in the denominator and not the numerator.

## What will be true

- [ ] Every node of every CURRENT diagram is green and reached, and the two maps are the same
      drawing.
