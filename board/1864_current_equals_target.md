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

| 2026-08-25 13:50 | 235e3f47 | 37 / 73 = **51 %** | 9 / 12 = **75 %** | **Flat, and three new green nodes are what hid it.** `Typeface`, `Pointer` and `Unwired` entered the map this hour; `Unwired` is reached (`Engine.cpp:184` builds one when the roots say offline), `Typeface` and `Pointer` are NOT — `outshine-viewer --show four-lines.scenario` answers *the layout holds no box, so there is nothing to paint*, windowed and headless alike, so no picture in this tree carries a glyph or takes a click, and they join the eleven already stranded. Not one red retired: `World`, `SubjectDraw`, `Sim`, `Live`, `Engine` all stand where they stood. What DID move and is not on this axis: `make` exits 0 again and links both apps (board:1869 delivered), nine of the twelve red claims are green (board:1882), and the shipped scenario declares its own drive. The picture is still a car on white. |

## The work order

Ten items, in the order that shrinks the figure fastest, each naming the node it turns green.
Rewritten each round by the review.

| | item | the node it moves |
|---|---|---|
| 1 | board:1887 | the corridor lays at a junction — `CorridorLay` amber -> green and, for the first time, a still of a ROAD. A 91.4-degree street corner is refused today because the fit's tolerance is the tile's 0.597 m quantum |
| 2 | board:1805 | `GroundPatchwork` reached (the driver calls `Engine::Compose`), then `Forest`, `Buildings`, `Water`, `Infrastructure` leave stranded — and `Sim` red dies with the facade. Ground, sky and horizon under the car |
| 3 | board:1862 | components counted before they are blamed, and a snap derived from what two tile grids can separate — `Wayfinding` keeps its green and the route exists to be driven |
| 4 | board:1880 | `Typeface`, `Pointer`, `Markup`, `Stylesheet`, `LayoutUi`, `Painting` leave stranded — the viewer paints a box, so six green nodes draw a pixel |
| 5 | board:1826 | `Live` red -> green, and the overlay reaches the picture without it; `GltfStudio` amber -> gone |
| 6 | board:1882 | `TheBuildDeclarationAuditsItself`'s four negative controls seed against what the listing HOLDS, so the audit's green means something again |
| 7 | board:1886 | one preparer digest over the whole corpus, so `EveryOracleWasPreparedByThisPreparer` stops being a standing red nobody prices |
| 8 | board:1538 | `World` red -> green (the eye leaves the ground layer), `DrawList` and `Frustum` amber -> green |
| 9 | board:1867 | `SubjectDraw` red -> green and `SUBJ` red -> green — the largest red on both maps |
| 10 | board:1575 | `LightVisibilityStage` amber -> green, and the car's shadow lands on the ground board:1805 laid |
