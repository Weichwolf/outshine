Type: issue
Area: docs
Tags: map, traffic-light, adjudication

**The CURRENT diagram names the reason a node is red, and the reason is true**

CLAUDE.md's class-structure CURRENT paragraph justifies two red nodes with one sentence:

> red — provably wrong — `TilePool` and `World` spell camera and LOD inside the ground layer

Half of that is false at HEAD. Checked by walking the code:

| node | claim | at HEAD |
|---|---|---|
| `World` (src/ground/World.h:49,55,189-205) | spells camera and LOD | TRUE — `struct Eye`, `Refine(const Eye&)`, `Viable/WantSplit/Splits/CanCover/Descend/DrawChildren(… const double eye[3], const double fwd[3])`, `EyeInMercatorBand()` |
| `TilePool` (src/ground/TilePool.h, TilePool.cpp) | spells camera and LOD | FALSE — `grep -c 'eye\|Eye\|camera\|Camera\|frustum\|Frustum\|Lod\|lod'` over both files: **0**. The class is a byte-budgeted LRU work pool (`ByteBudget_`, TilePool.cpp:162) with a `Ledger` of counts and timings, keyed on `ErrM` — projected error, the one currency CLAUDE.md admits |

A decisionless, budgeted, instrumented pool keyed on projected error is the RAGE reference
the tree cites, not a layering violation. Whatever `TilePool` still owes (it holds threads,
mutexes, a `std::map` and a `std::set` on a hot layer — an amber's worth of question), the
**stated** reason for its red is not it.

The map is the first thing a reader trusts. A colour whose justification is provably stale
teaches the reader to discount the other colours too.

## What will be true

1. The owner re-adjudicates `TilePool`'s colour and the sentence beside it names the reason
   that actually holds at HEAD — or the node moves to amber/green and the sentence names
   `World` alone.
2. The same walk is done for the other reds in that paragraph before the next round quotes
   them: `SubjectDraw` ("six responsibilities"), `Sim` and `Live` ("hand-wired god
   facades"). Each verdict cites file:line the way the two rows above do.
3. Only the owner edits CLAUDE.md — this item exists so the correction is not silently
   folded in by a reviewer.
