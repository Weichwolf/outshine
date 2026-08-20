Type: issue
Area: harness
Supersedes: 1124, 0038
Tags: instrument, scope

**No test reads the board, and the seven checks live where the work is**

The owner's ruling, and the reasoning is one sentence: **a test answers what the ENGINE does.** The board
is a notebook about the work. A dangling `Depends:`, a `feature` that grew a `Parent:`, a `closed` item
whose child is open -- every one of those is a bookkeeping slip, and a suite that went red over one would
be reporting a markdown file as a defect in the tree. That is the confusion between the thing and the
writing about the thing, and this repository spends most of its length avoiding it everywhere else.

**`CLAUDE.md` already contained the argument against the test it asked for.** *The board is kept true
incrementally, at the point of use, never by a sweep. A pass that has to be remembered is a pass that
will not happen.* A test that walks `board/*/*.md` on every run **is** that sweep, moved from a person to
a machine but not made into a different kind of thing. `board:1124` read the first half of that paragraph
and built the sweep the second half refuses.

## What was done

| | |
|---|---|
| deleted | `test/outshine/harness/EveryBoardInvariantHolds.cpp`, 275 lines |
| narrowed | `test/outshine/harness/EveryPathCitedInADocumentResolves.cpp` reads `CLAUDE.md` and `src/assets/tables/` -- the BINDING documents -- and no longer walks `board/` or the `.claude/agents` tree, which does not exist |
| rewritten | `CLAUDE.md`'s *seven invariants* section: six are read by eye when an item is groomed into `board/active/`, and the seventh is `git grep -l 'board:0042' -- test/` at the moment the item is closed |

**The seventh keeps its force and loses its sweep.** *A closed item cited by nothing under `test/` is an
unproven claim* is still the rule; what changed is that it is answered in one line when an item closes,
which is when the person asking already knows what the item was about. A sweep answers it at a moment
when nobody is thinking about any of them.

## The multi-agent residue went with it

The tree was written for a fleet of agents and is worked by one. Four sentences in `CLAUDE.md` still
addressed that fleet and were corrected rather than kept: *a number an agent reports*, *not in an agent
description*, *while any agent is running, stage named files*, and `ES.9` *stood in two agent
definitions*. The setup table also claimed this file carries **the roles**, and it carries no such
section -- a stale claim about itself in the one document that is binding on everyone.

**The one rule underneath the concurrency sentence survived, because its reason survived**: `git mv`
STAGES its rename, so `git add -A` sweeps whatever else is in flight into a commit about something else.
A background run's artefacts are still in flight even when nobody else is working, so *name every file
you commit* is kept and the fleet is dropped from its reason.

## Comments

`board:0038` was a bug about `board/active/` describing a tree that had moved on, and its only proof was
the deleted test. That claim is not an engine capability and never was: keeping the board true is a
working discipline, and the instrument for a discipline is doing it, not a red light somewhere else.
