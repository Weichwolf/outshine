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

## THE REFIT IS BUILT AND PROVED; WHAT IS OPEN IS HOW `SetMesh` KNOWS

`TriangleBvh::Refit` writes the corners through the permutation the build already recorded and widens
the boxes leaves-up -- which a REVERSE WALK of a depth-first array is by construction, since a node's
children always sit at higher indices than it does. It allocates nothing.

[MEASURED] `ARefittedTreeAnswersWhatARebuiltOneDoes`, 512 triangles, 281 nodes, depth 14, 4096 rays,
between a rest pose and one twisted about the vertical axis:

| | |
|---|---|
| rays the **unrefitted** tree answers differently | **115 of 4096** -- the negative control fires |
| rays the **refitted** tree answers differently from a REBUILT one | **0 of 4096** |
| nodes | 281 after the refit, 289 in a rebuild -- different splits, identical answers |

**The negative control is the load-bearing half.** Without it a `Refit` whose body was `return true;`
would be green, and the failure it would hide -- a shadow structure holding last pose's geometry --
looks like a scene with slightly wrong shadows rather than like a bug.

## IT IS WIRED, AND THE QUESTION BELOW WAS THE WRONG QUESTION

**No shipped engine asks whether the topology changed, because its data model makes the question
unaskable.** A skinned mesh's index buffer lives in the asset and is uploaded once; only the vertex
streams are dynamic. The question existed here only because `SubjectMesh` bundled indices and vertices
into one *here is everything, again* call.

**So `SubjectPose` is a type with nowhere to put an index run**, and it is the base of `SubjectMesh`
rather than a member of it, so a caller filling a whole mesh writes exactly what it always wrote. The
triangles' corner indices live inside the tree -- twelve bytes a triangle, replacing BOTH the build
permutation this class used to hold AND the index run a caller would have had to keep -- so `Refit`
takes positions and nothing else. **A refit over different triangles is not a case to refuse; it is a
sentence that cannot be written**, which is `CLAUDE.md`'s own preference over a comparison, a hash or a
consumer's promise. It costs zero bytes held and zero comparisons a frame.

`Clients` gains `Move` beside `Aim`, `Surface` and `Place` -- the fourth instance of one separation --
and `Live` stands the subject up on its first submission and moves it on every later one.

## [MEASURED] WHAT IT BOUGHT, same run, same tags

| tag | before | after |
|---|---|---|
| **`mesh-bvh`** | **19 767 456** | **41 984** -- a factor of **471** |
| `mesh-upload` | 705 488 | 842 032 |
| `render-frame` | 233 296 | 233 296 |
| `vertex-pack` | 36 864 | 36 864 |
| `index-run` | 3 072 | 3 072 |
| `draw-list` | 288 | 288 |
| **the frame path entire** | **~20.8 MB** | **1.16 MB** -- a factor of **18** |

**`mesh-upload` is now the largest term** and it is nine GPU buffer creations a frame; persistent
buffers with one copy pass are the next cut, and they are the other half of what a shipped engine does
with a dynamic vertex stream.

## WHAT WAS UNDECIDED, kept because the reasoning is the point

**How does `SubjectDraw::SetMesh` know the triangles are the same triangles?** A refit is correct only
while they are, and three answers are available:

| | cost | how it fails |
|---|---|---|
| **compare the index run against a stored copy** | O(n) reads a frame and 4 bytes a index held for the subject's life -- 18 MB on the corpus's largest | it does not: exact, and always cheaper than the rebuild it avoids |
| **hash the index run** | O(n) reads, 8 bytes held | a collision at 2^-64 leaves a stale structure, which is a picture defect nobody would look for |
| **the consumer declares a topology identity** | free | a stranger sets it wrong and gets last pose's shadows; `CLAUDE.md` prefers unspellable over documented |

**The first is recommended** and its argument is that the comparison is strictly cheaper than the
rebuild it replaces -- O(n) reads with no allocation against O(n log n) with one. The 18 MB is the part
that needs the owner, because it is held for every subject and not only for the ones that move.

## What must be true

- [x] **A pose refits and takes nothing from the allocator.** [MEASURED] over 500 frames of
  `BoxAnimated`, `mesh-bvh` reads **41 984 bytes, 0 of them after the settling point** -- the whole
  total is the one build at stand-up, and the test now brackets the settled window and holds it at
  zero. Before the refit it read **19 767 456**, which is one build per frame
- [x] **The tree is rebuilt when the triangles change** -- `SetMesh` builds, `SetPose` refits, and
  the entry point IS the trigger, so a pose cannot ask for a rebuild and a topology cannot ask for a
  refit. The 41 984 bytes above are that build and they arrive once
- [x] **The refit's quality loss is measured and published, and it is 2.07 %.** [MEASURED] over 512
  triangles deformed between two poses, the SAH cost of the refitted tree is **286.535917** against a
  rebuild's **280.73197** -- a ratio of **1.02067433**. *The surface-area heuristic is the field's own
  measure of a partition's quality and it needs no instrument in the query path*: a refit moves the
  boxes a partition already chose and cannot repartition, so it can only be worse, and the test holds
  it above 1.0 and under 2.0 for exactly that reason
- [x] **The rebuild trigger is declared, and it is the ENTRY POINT rather than a flag.** `SetMesh`
  builds and `SetPose` refits, so a pose cannot ask for a rebuild and a topology cannot ask for a
  refit -- a mistake that would need a wrong argument is one nobody can make. *A scenario knob for a
  per-frame rebuild is not built, because no measurement has asked for one: at 2.07 % quality loss the
  case for paying a full build every frame has not been made, and `board:1480`'s tree is where it would
  go the day it is.*

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

## What proves it

**`test/unit/core/ARefittedTreeAnswersWhatARebuiltOneDoes.cpp`** -- a refitted tree answers what a
rebuilt one does over 4 096 rays with a negative control that fires on 115 of them, and it publishes
the SAH cost of both: 286.535917 refitted against 280.73197 rebuilt, a ratio of 1.02067433, held above
1.0 and under 2.0.

**`test/render/outshine/scenario/AnEngineInSteadyStateReturnsToTheSameLiveByteCount.cpp`** -- brackets
the settled window and holds `mesh-bvh` at **0 bytes taken after it**, so the 41 984 the run reports is
the one build at stand-up and a pose allocates nothing.

