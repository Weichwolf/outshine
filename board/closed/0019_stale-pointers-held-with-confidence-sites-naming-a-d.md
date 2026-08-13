Type: bug
Area: render
Tags: scope

**Stale pointers held with confidence — sites naming a deleted document — **Band 2****

the deleted architecture document and the deleted vision document were folded into `CLAUDE.md` and deleted. Comments in `src/`
still cite the deleted architecture document as the authority for a rule they state. **The count is not restated
here because it has been wrong at three different values across three rounds** — nine, then seven, then
fewer again after the SDL_GPU port took `GeometryStage.h` and `TaaStage.cpp` with it. `grep -rl
architecture.md src/` is the count, it is one command, and a number copied into this file ages the
moment a file is deleted. A reader who follows the pointer finds nothing; a reader who does not follow it takes
the rule on the comment's word, which is exactly the failure mode a citation exists to prevent. This is
the same defect class as a miscited rule number — a confident reference to something that is not there —
and it costs a round the first time somebody tries to check one of these rules against its source.

- `src/core/Material.h:19` — *"nothing in it can switch a pipeline state (doc/architecture.md)"*. The
  rule is live and correct; it is in `CLAUDE.md` under *the core dictates the pipeline*.
- a line of the old scope ledger — *"declared in the deleted architecture document, not found in `PresentStage`"*: the line's
  own evidence is a document that no longer exists, so the line cannot be checked as written.

Right: each site names `CLAUDE.md` and the sentence there, or states the rule without a citation if the
rule is local. A grep for the deleted architecture document returning zero in `src/` is the check.
