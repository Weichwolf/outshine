Type: feature
Area: render
Tags: perf, instrument
Depends: 1463

**A visibility structure is refitted for a pose and rebuilt only for a topology**

A subject that moves keeps the BVH it already has. Its node bounds are brought up to the new positions
bottom-up, in one pass over the tree, taking nothing from the allocator; the tree is **rebuilt** only
when the triangles it indexes change -- which for a posed subject is never.

## What it costs today, named by its own tag

[MEASURED] `BoxAnimated`, 500 frames, by allocation tag over the engine's own allocator (`board:1462`):

| tag | bytes taken over the run | share of the frame path |
|---|---|---|
| **`mesh-bvh`** | **19 767 456** | **96.5 %** |
| `mesh-upload` | 705 488 | 3.4 % |
| `render-frame` | 233 296 | |
| `vertex-pack` | 36 864 | |
| `index-run` | 3 072 | |
| `draw-list` | 288 | |

**39.5 kB a frame, and it is a whole spatial structure discarded and built again because the vertices
moved.** `TriangleBvh::Over` runs inside `SubjectDraw::SetMesh`, which an animated subject calls on
every advance.

## Refit is the field's own name for this and it is thirty years old

A **refit** walks the existing hierarchy from the leaves up and widens each node's box to hold its
children's; the topology, the split planes and the ordering are untouched. It is O(nodes), allocates
nothing, and is exactly correct for a subject whose triangles keep their indices and only move --
which is what a pose is.

**What it costs is quality, and the cost is bounded and stateable**: a refitted tree over a heavily
deformed pose has looser boxes than a rebuilt one would, so a traversal visits more nodes. The
established practice is to refit per frame and rebuild on a declared trigger -- a measured surface-area
heuristic, a keyframe, or a topology change. **The trigger is a declaration and belongs beside the
scenario, not inside the builder.**

## What must be true

- [ ] **A pose refits and takes nothing from the allocator**, which the tag above measures directly:
  `mesh-bvh` reads zero over a steady-state run
- [ ] **The tree is rebuilt when the triangles change** -- a different index run, a different vertex
  count, a first build -- and the rebuild is where the allocation lives, at stand-up
- [ ] **The refit's quality loss is measured and published**, nodes visited per ray or per query against
  the rebuilt tree over the same poses, so *refit is cheaper* is a number and not a habit
- [ ] **The rebuild trigger is declared** and a scenario that wants a rebuild every frame can say so

## What this feature may NOT do

**It may not silently change what the structure answers.** The shadow the subject casts is decided by
this tree; a refit that widened a box until a query missed a triangle would be a picture defect wearing
a performance win. The corpus is what says it did not.

## Comments

**It took three instruments to reach this line and each one refuted the previous answer.** A process-wide
heap ceiling said *no leak* and could not attribute; an engine-owned counter said *223 frames in 249
move* and could not say where; a per-phase difference said *submitting* and could not say which call.
The tag says `mesh-bvh`, 96.5 %, and the two answers everybody would have guessed first -- the
flattener's temporaries and the nine buffer creations -- are 0.2 % and 3.4 %.
