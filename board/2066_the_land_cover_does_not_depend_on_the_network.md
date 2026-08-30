Type: bug
State: open
Area: world, render
Tags: measured, picture, determinism

# The land cover does not depend on how fast the network is

**Benchmark** — Unreal: a Landscape's layer data belongs to the level and a component's material
is settled once its data is loaded; automation screenshots are gated on streaming being complete
before the frame is captured, which is why a comparison can be bit-for-bit. RAGE: a replay plays
a drive back frame for frame, which is impossible if content that arrived at a different speed
paints a different picture. **Both agree**: what is drawn follows from what is declared, never
from how quickly bytes turned up.

## Measured, Heidelberg, one commit, one machine, one declaration

| cache | digest | the picture |
|---|---|---|
| COLD | `236b28e5` | a green wooded valley, the Koenigstuhl in leaf |
| WARM | `a2d6cd59` | the old desert brown, land cover absent |

Three consecutive warm runs give `a2d6cd59` exactly. The difference is the cache, and the cold
run is the CORRECT one: it waits 23 s on the network and the classification finishes inside that
wait. **The picture is better when the line is slower.**

## What is established

- The class field completes and publishes. Probed in `ClassField::Update`: it is entered 126
  times, `Opened_`, `Veg_` and `Veg_->Ready()` all hold, and by the end
  `submitted=0 fineHave=1 coarseHave=1 finePending=0 coarsePending=0` -- every clause of
  `Complete()`. `ClassBuilder` hands back version **3**.
- Rebuilds DO follow the classification. A guard on `ClassStructure::Version()` was added to
  `Grounds()`'s rebuild test and fires: `World.LaidClasses` tracks the published version to 3.
  It changed no digest, because tiles landing already forced those same rebuilds today -- it
  closes the case where the classification moves and no tile does.
- `the ring's vertices a land class names` reads **0** and `class field: it published a
  structure` reads **0** in the same run whose probe showed version 3 published. One of those two
  is describing a different moment from the other.
- `measures published twice in one round` reads **0**, so the ledger sees no name written twice
  with a different value. The round is not the explanation.

## What is NOT established, and is the next measurement

Why `ClassField::ClassAt` names nothing at the ring's vertices in the warm run while a structure
of version 3 stands published. The two candidates, neither tested:

1. `ClassAt` projects through `ClassField::Frame_` and evaluates in the grid of the structure,
   which was built with `job.Frame`. If those frames are not the same origin every vertex falls
   outside the grid and returns -1.
2. The colours are computed in the LAST rebuild, and the last rebuild may predate version 3
   even though a rebuild fired for it.

Distinguishing them needs one number: at the rebuild that produced the picture, the class version
in hand and the count of vertices it named.

## What will be true

- [ ] A place renders the same picture from a cold cache and a warm one. Negative control: clear
      the cache, render, clear again, render -- both must equal the warm digest.
- [ ] `make shots` states streaming and classification completeness as a PRECONDITION where it
      prints, because a digest taken before the world settled is a snapshot of a race.
- [ ] Every measure published inside `Grounds()` says which rebuild it describes, or is published
      from a state the picture actually used.
