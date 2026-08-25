Type: feature
State: open
Area: architecture
Tags: measurement, review

# CURRENT equals TARGET

The distance between the two maps in `CLAUDE.md` is the work list, and this item is the
definition of done. The figure is `green-and-reached / total`: a green node whose only path to a
client runs through a red one draws no pixel, so it counts in the denominator and not the
numerator. Both numbers are DERIVED — counted from CLAUDE.md's CURRENT diagrams as they stand,
and again from `git show <last review commit>:CLAUDE.md`. Nothing is stored that could be
counted.

## What will be true

- [ ] Every node of every CURRENT diagram is green and reached, and the two maps are the same
      drawing.

## The distance, one row per round

| round | HEAD | class structure | render plan | what moved it |
|---|---|---|---|---|
| 2026-08-25 07:17 | b2fbf22d | 44 / 67 = **66 %** | 9 / 12 = **75 %** | first measurement |
| 2026-08-25 08:17 | 1af2c00b | 44 / 68 = **65 %** | 9 / 12 = **75 %** | `Alignment` entered green and reached (`Fit.cpp:158` calls `Align`, and `Fit` is reached from `CorridorLay` and `DriveAssembly`); `Engine` went green -> amber, because the door that lays the drive also makes a failed route fatal to the frame (board:1870) and recovers its refusal by grepping prose (board:1621). No red retired. **Down one point: the hour spent itself on the board, the gate and the door, and moved no node out of red.** |

## The work order

Ten items, in the order that shrinks the figure fastest, each naming the node it turns green.
Rewritten each round by the review.

| | item | the node it moves |
|---|---|---|
| 1 | board:1870 | the product draws SOMETHING again — the precondition for every screenshot verdict there is |
| 2 | board:1805 | `Forest`, `Buildings`, `Water`, `Infrastructure` — four green nodes nothing outside `src/` reaches, and `Sim` red dies with the facade |
| 3 | board:1862 | `Engine` amber -> green; `DriveAssembly`, `CorridorLay`, `DriveTick` reached through the door rather than by tests |
| 4 | board:1538 | `World` red -> green (the eye leaves the ground layer), `DrawList` and `Frustum` amber -> green, and TARGET's `Compositors` gets its first line |
| 5 | board:1867 | `SubjectDraw` red -> green and `SUBJ` red -> green — the largest red on both maps |
| 6 | board:1826 | `Live` red -> green, `GltfStudio` amber -> gone, `src/clients/` dissolved |
| 7 | board:1869 | the gate stops being silenceable, which is what every other row's proof stands on |
| 8 | board:1795 | TARGET's `Alignment` amber -> green, and the corridor stops planning 12 km/h on a trunk road |
| 9 | board:1574 | `GLASS` red -> green, and the frame path stops allocating at every relay |
| 10 | board:1575 | `LightVisibilityStage` amber -> green, and the sun lands in the picture |
