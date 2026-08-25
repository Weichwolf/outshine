Type: feature
State: open
Area: architecture
Tags: measurement, review

# CURRENT equals TARGET

The distance between the two maps in `CLAUDE.md` is the work list, and this item is the
definition of done. The measurement itself lives in `CLAUDE.md` under *The distance to TARGET*,
one row per diagram per round, rewritten by the hourly architect from the file as it stands and
from `git show <last review commit>:CLAUDE.md` — nothing is stored that could be counted.

`green-and-reached / total` is the figure: a green node whose only path to a client runs through
a red one draws no pixel, so it counts in the denominator and not the numerator.

## What will be true

- [ ] Every node of every CURRENT diagram is green and reached, and the two maps are the same
      drawing.

## The work order (2026-08-25, after the owner's cull to 47 items)

Ten items, in the order that shrinks `green-and-reached / total` fastest, each naming the node
it turns green:

| | item | the node it moves |
|---|---|---|
| 1 | board:1805 | `Forest`, `Buildings`, `Water`, `Infrastructure` — four green nodes nothing outside `src/` reaches, and `Sim` red dies with the facade |
| 2 | board:1862 | `Engine` amber -> green; `DriveAssembly`, `CorridorLay`, `DriveTick` reached through the door rather than by tests |
| 3 | board:1538 | `World` red -> green (the eye leaves the ground layer), `DrawList` and `Frustum` amber -> green, and TARGET's `Compositors` gets its first line |
| 4 | board:1867 | `SubjectDraw` red -> green and `SUBJ` red -> green — the largest red on both maps |
| 5 | board:1826 | `Live` red -> green, `GltfStudio` amber -> gone, `src/clients/` dissolved |
| 6 | board:1859 | the product runs from anywhere; blocks every measurement board:1803 owes |
| 7 | board:1795 | TARGET's `Alignment` amber -> green, and the corridor stops planning 12 km/h on a trunk road |
| 8 | board:1574 | `GLASS` red -> green, and the frame path stops allocating at every relay |
| 9 | board:1575 | `LightVisibilityStage` amber -> green, and the sun lands in the picture |
| 10 | board:1583 | the component-model diagram entire: `SceneStore` -> `CAN`/`MAY`/`INTERACTS` |
