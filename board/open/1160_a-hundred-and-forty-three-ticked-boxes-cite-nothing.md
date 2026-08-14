Type: bug
Area: harness
Tags: instrument, scope

**A hundred and forty-three ticked boxes cite nothing in the tree**

A ticked box is a capability claim. This board's own instrument for a capability claim is
`git grep -l 'board:NNNN' -- test/` — *the evidence; empty means unproven* — and its five invariants
already make that reasoning binding at closure: **a `closed` id cited by nothing under `test/` is an
unproven claim.**

**The same reasoning is not applied to a tick, and the gap is not small.** Across the 33 features
`0040`–`0072`, which are the vision's own clauses:

| | |
|---|---|
| ticked boxes | **143** |
| unticked boxes | **400** |
| of those 33 ids, cited anywhere in `src/` or `test/` | **0** |

So a reader of the board sees roughly a quarter of the engine's requirement clauses ticked, and **not one
of them can be traced to a line that implements or proves it.**

**The caveat was sought and it clears.** *Is the marker convention simply younger than those items?* No —
it is used on ids as old as `board:0019`, `0031`, `0037` and `0038`, and heavily on `0073`–`0105`. **29
distinct ids are cited from the tree and every one of them is outside `0040`–`0072`.** The absence is a
property of the work, not of the convention.

**Why it is a bug and not a gap.** The board is the scope and the authority on what the engine must do,
and it currently reports progress that the tree cannot corroborate. That is this repository's own named
failure in its plainest form: **a capability claim decoupled from its evidence** — the exact reason the
header carries no `Test:` field and no test result.

**What would be right instead, and it must not become a sweep.** Three candidates, and the third is the
recommendation:

- **Tick nothing without a citation.** Correct and unenforced; a rule nobody counts is the shape this
  tree keeps replacing.
- **An invariant that reddens on any ticked box whose id is uncited.** Mechanically right and **wrong
  today**: it would go red on 143 lines at once and stay red for months, which is a test that teaches
  people to ignore it.
- **Publish the number, per feature, and let it be read.** A derived count — *ticked boxes, and of those
  how many ids the tree cites* — printed by the harness beside the invariants. **It goes red on nothing
  and it cannot be ignored**, because a feature reporting *9 ticked, 0 cited* says exactly what it is.
  The invariant follows later, when the number is small enough that turning it on is a day's work rather
  than a moratorium.

**And a second, sharper reading is available for free from the same query**: a feature whose *unticked*
count is large and whose citation count is zero is a **pillar that has not started**, which is precisely
what `board:1159` had to measure by hand this round. The number, once published, makes that ordering
readable from the board instead of reconstructed.

**What this must NOT do.** It must not become a licence to untick. A tick that is true and uncited is a
**missing citation in the source**, not a false claim — and the repair is a `board:NNNN` marker at the
site that implements it, which is one line and travels with the code. Removing ticks to make a number
move would be scope given up to improve an instrument.

**Done when** the harness publishes, per feature, its ticked count and how many of its ids the tree cites;
the number is beside the invariants where it is read; and no round can report a ticked clause without the
evidence query disagreeing in public.
