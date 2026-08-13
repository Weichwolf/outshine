Type: bug
Area: harness
Tags: instrument, bug

**The board's invariants are declared and unchecked**

`CLAUDE.md` states seven invariants and one query. **None of them is a test.** They have only ever been
run by hand, from a shell, by whoever happened to be looking — which is the shape the board was built to
replace, one level up: *a pass that has to be remembered is a pass that will not happen.*

Measured by hand at 1 117 files: the whole battery takes **0.46 s**, so cost is not the reason it does
not exist. And it caught something on its first hand run — **three `closed` items cited by nothing under
`test/`**, which is one of the declared invariants firing correctly.

The seven: a dependency cycle · a `closed` item depending on one that is not closed · an edge id that
does not resolve to exactly one file · a `feature` carrying a `Parent:` · a `task` with no `Parent:` or
one naming something that is not a `feature` · a `closed` feature with an open child · **a `closed` id
cited by nothing under `test/`**. Plus the marker's other direction — a `board:` marker naming an id
that does not exist — which the citation test already handles.

**The query is not one of them.** *What is ready to start* must never become a test: a board with nothing
ready is a legitimate state, and a suite that went red on it would teach people to keep a fake item open.

`board:0038` closed because `doc/todo.md` was deleted, and nothing proves the board will not go stale the
same way. **This test is what would.** Until it exists, `0038` stays flagged by the very invariant it is
about, which is the honest state rather than a carve-out.

**Done when** the seven run in the harness with a real verdict, the query is absent from it by name, and
the three currently-flagged closures are either cited or reopened.
