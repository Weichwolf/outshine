Type: bug
State: active
Area: board
Tags: measured

# A commit that says `closed` has deleted the file

**Benchmark** — Unreal: a Jira/UDN issue marked resolved disappears from the open query; the state and the list cannot disagree. RAGE: the same in its own tracker. **Both agree** — the record of a closure and the list of open work are one thing, never two that must be kept in step by hand. **The choice of mechanism is mine** because this board has no database: the file IS the state, so the commit that declares a closure must be the commit that removes it.

CLAUDE.md is unambiguous: *closing is DELETING the file; what it said is in the commit that
removed it.* A commit that announces `board:NNNN closed` and leaves the file behind produces the
one thing a flat-file board cannot survive -- a git history saying the work is done and a
directory saying it is open. Whichever a reader trusts, the other one lies to them.

Measured over every commit whose subject says `board:NNNN closed`:

    1963  d9ab2a38  "the one integration test runs from a clean checkout, and it shows a
                     picture" -- it ADDED apps/driver/src/scene.gltf, which is the whole of
                     what the item was missing, and never touched board/1963
    1881  acede045  "the canvas comes first, and the present stage is in the plan" -- and the
                     item still carries FOUR unticked predicates, so this one is the opposite
                     mistake: the commit was ahead of the work

Two different failures behind one symptom, which is why the guard has to be mechanical rather
than a habit: a closure announced and not performed, and a closure announced too early.

## And building the guard found the deeper one

The first run reported FIVE, not two. `1986` and `1987` were false -- and false for a reason that
matters more than the thing being measured: **a number is reused once its file is deleted.** The
next id was taken as

    ls board/*.md | grep -o '[0-9]{4}' | sort -n | tail -1

which reads the DIRECTORY, so a closed 1986 frees 1986 for the next filing. Then
`git log --grep 'board:1986'` returns two unrelated bodies of work, and CLAUDE.md's own sentence
-- *the number is identity* -- is false. Six numbers were already reused before this session
noticed: 1870, 1893, 1932, 1933, 1966, 1970. This item and the naming survey made eight, and
this one had to be renumbered to 1988 to stop being its own false positive.

The next id has to come from the HISTORY, which knows every number ever issued. The guard here
cannot be right until it does: it will keep reporting a live item as an unperformed closure of a
dead one.

- [ ] the next id is derived from the history, so a number is issued once and never again
- [ ] no commit subject says `board:NNNN closed` while `board/NNNN_*.md` still stands
- [ ] a claim walks the history and refuses, so the next one is caught in the gate
