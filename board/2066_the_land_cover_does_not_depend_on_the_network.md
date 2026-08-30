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

## Two more facts, and candidate 1 falls

- **The frames are the same.** `ClassField::LendTo` sets `job.Frame = Frame_`, so the structure is
  built in exactly the frame `ClassAt` projects through. Candidate 1 is out.
- **The measures describe the FIRST rebuild, before any `Update` ran.** `class field: tiles it
  waits for` reads -1, which is only returned when `Fine_.Field` is null -- the state before
  `ClassField::Update` has been entered once. `the version the colours used` reads -1 beside it.
  So every class measure in `Grounds()` is answering from the first moment the world was touched,
  while the probe showed a version-3 structure published and a rebuild firing for it.

That leaves the question sharper than it was: a rebuild DOES run with version 3 in hand, and the
ground still wears no class. The remaining candidate is EXTENT -- `SubmitDue` builds a grid of
`HalfCells * CellM` about the camera, while the ring runs out to the declared sight, 240 km. A
vertex outside the grid returns -1 by construction. What that does not yet explain is why `named`
is exactly 0 rather than small: the vertices near the camera are inside any such grid.

## PROBED AT THE LAST REBUILD, and it names REAL classes

Logging straight past the ledger, inside the tinting loop, on the rebuild that produced the
picture:

    named=253515  unmapped=0  version=3  verts=653400  materials=1  missingLayers=26  clsPending=0

**38.8 % of the ring's vertices wear a land class and NOT ONE of them is the unmapped row.** The
classification is applied, it is real, and the picture is still brown. Every ledger measure that
said otherwise was answering from the first rebuild.

The same four points, warm against cold, tell what actually differs:

| point | warm `a2d6cd59` | cold `236b28e5` |
|---|---|---|
| foreground | (100, 79, 58) | (78, 90, 62) |
| valley floor | (97, 96, 59) | (80, 97, 62) |
| Koenigstuhl | (143, 123, 108) | (103, 118, 114) |
| far left | (128, 114, 107) | (96, 110, 111) |
| right slope | (140, 116, 95) | (98, 111, 100) |

Every point differs and every one the same way: **warm has R > G, cold has G > R.** The two runs
do not differ in whether a class is assigned -- they differ in WHICH class. Bare earth against
vegetation, everywhere at once.

The class grid's own extent is not the answer either, though it explains the far distance:
`Fine_` is 64 cells of 16 m, a half-width of **1024 m**; `Coarse_` is 128 of 64 m, **8192 m**.
The ring runs to the declared sight of 240 km, so everything past 8.2 km is unclassed by
construction -- but the Koenigstuhl is 2 km out and inside both.

`missingLayers=26` at that same rebuild is the one lead left: the classification is built from
vector tiles that do not carry every layer `VegetationTemplates::Layers()` asked for, and which
layers are present decides bare against green.

## What is NOT established, and is the next measurement

Why `ClassField::ClassAt` names nothing at the ring's vertices in the warm run while a structure
of version 3 stands published. The two candidates, neither tested:

Both candidates are now DEAD -- the frames match and the last rebuild does hold version 3 and does
name 253 515 vertices. What is left is why the SAME place gets bare earth on one run and
vegetation on the other, with `missingLayers` reading 26 on the bare one. The measurement:
`MissingLayers()`, `UnknownKinds()` and `UnknownFeatures()` on a COLD run, against the 26 above.
If the cold run reads 0 the answer is that the classification is built before its layers arrive
and nothing makes it wait.

## What will be true

- [ ] A place renders the same picture from a cold cache and a warm one. Negative control: clear
      the cache, render, clear again, render -- both must equal the warm digest.
- [ ] `make shots` states streaming and classification completeness as a PRECONDITION where it
      prints, because a digest taken before the world settled is a snapshot of a race.
- [ ] Every measure published inside `Grounds()` says which rebuild it describes, or is published
      from a state the picture actually used.
