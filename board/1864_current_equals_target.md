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
| 2026-08-25 08:17 | 1af2c00b | 44 / 68 = **65 %** | 9 / 12 = **75 %** | `Alignment` entered green and reached; `Engine` green -> amber. No red retired. Down one point: the hour spent itself on the board, the gate and the door. |
| 2026-08-25 11:17 | a3ebe3e0 | 36 / 70 = **51 %** | 9 / 12 = **75 %** | **Down fourteen points, and the cut is what did it.** `test/unit/` was deleted with 170 cases, and with it the ONLY caller of nine green nodes: `Markup`, `Stylesheet`, `Layout`, `Painting` (reach the picture only through `Live`, red), `InputMap`, `InputPump`, `TriggerField`, `ViewBook`, `BusGraph` (no reference outside their own files). `Sim.h` went from one test + one src file to `src/clients/Sim.cpp:1` alone. `Engine` amber -> RED: `Read` + `Assemble` + `Advance` is the documented flow and it stands no picture (board:1881), so the product's headless run leaves zero stills. `OverlayDraw` was drawn and uncoloured for two rounds and is now green and reached. `GroundPatchwork` entered stranded: `Engine::Compose` lays a tile ring and no programme calls it. **Nothing was broken this hour. What broke was the illusion that a subsystem a test calls is a subsystem the product has.** |

## The work order

Ten items, in the order that shrinks the figure fastest, each naming the node it turns green.
Rewritten each round by the review.

| | item | the node it moves |
|---|---|---|
| 1 | board:1869 | the gate runs at all — at a3ebe3e0 one non-compiling source under `apps/viewer` makes `make` AND `test/run.sh` exit 2 having judged nothing. Every other row's proof stands on this |
| 2 | board:1881 | `Engine` red -> amber — one arrival route, so the shipped scenario renders and there is a picture to judge |
| 3 | board:1805 | `GroundPatchwork` reached (the driver calls `Compose`), then `Forest`, `Buildings`, `Water`, `Infrastructure` — and `Sim` red dies with the facade |
| 4 | board:1862 | the tile ring joins, so a five-kilometre route exists; then `ViewBook`, `InputMap`, `InputPump`, `TriggerField`, `BusGraph` leave stranded as the door advances what it accepts |
| 5 | board:1882 | the twelve red claims — three of them ABORT, so the no-comments rule and the device boundary are unguarded right now |
| 6 | board:1826 | `Live` red -> green, and `Markup`, `Stylesheet`, `Layout`, `Painting` leave stranded with it; `GltfStudio` amber -> gone |
| 7 | board:1538 | `World` red -> green (the eye leaves the ground layer), `DrawList` and `Frustum` amber -> green |
| 8 | board:1867 | `SubjectDraw` red -> green and `SUBJ` red -> green — the largest red on both maps |
| 9 | board:1877 | the sky stops being SNAPSHOT-grade; `MediumRadianceStage` gets an oracle |
| 10 | board:1575 | `LightVisibilityStage` amber -> green, and the sun lands in the picture |
