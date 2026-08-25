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

| 2026-08-25 15:0x | d4c8784c | 45 / 73 = **62 %** | 9 / 12 = **75 %** | **Up eleven points, the largest single-round movement since the axis was first measured — and NOT on the driver.** Six nodes left stranded because a real caller appeared for each: `Markup`, `Stylesheet`, `LayoutUi`, `Painting`, `Typeface` and `Pointer` draw the viewer's own face (`frame003.png`: corpus column, case list, highlighted row, status line, three faces at two sizes), and `Pointer` takes a click through `Engine.cpp:420`. `ViewBook`, `InputMap` and `InputPump` are stood by `Engine::Declare` and advanced by `Engine::Rides` and `Engine::Handles`. Two nodes moved the other way and are the honest cost: `TriggerField` green -> AMBER — reached, probed every tick, and a box of 1e7 m extent the body cannot be outside of fires nothing (board:1891); `Typeface` enters AMBER not green — it draws correctly and rasters lazily inside the draw, two `SDL_Surface` allocations per first-sight glyph (board:1892). Not one red retired: `World`, `SubjectDraw`, `Sim`, `Live`, `Engine` all stand. **The driver's own scenario still does not route** and its still is still a car on white; the eleven points were earned on the UI and input axes while the picture stood still. |

## The work order

Eleven items, in the order that shrinks the figure fastest, each naming the node it turns green.
Rewritten each round by the review.

| | item | the node it moves |
|---|---|---|
| 1 | board:1862 | components counted on the UNFILTERED graph and a snap derived from what two tile grids can separate. The turn filter is measured at 8.2 of the 59 unreachable points, so the snap is where the work is. `Wayfinding` keeps its green and, for the first time, the SHIPPED scenario routes |
| 2 | board:1890 | the ground ring anchored on the corridor's origin. `GroundPatchwork` leaves stranded, then `Forest`, `Buildings`, `Water`, `Infrastructure` follow and `Sim` red dies with the facade (board:1805). Ground, sky and horizon under the car |
| 3 | board:1890 (camera half) | the declared view is TAKEN: a first-person eye inside the cabin instead of a camera looking down at its own roof — `Live::Eye` in the same space as `Live::Carry`. `ViewBook` earns the green it now has |
| 4 | board:1826 | `Live` red -> green, and the overlay reaches the picture without it; `GltfStudio` amber -> gone. Six nodes stop counting on a red parent |
| 5 | board:1803 | `throttle`/`brake`/`steer` reach the seat and `Engine::Acts` dispatches on an id — `Engine` loses one of the four reasons it is red |
| 6 | board:1893 | the driven subject is LIT — the picture-level defect that makes every drive still a silhouette. No node changes colour and it is the largest single thing standing between the driver and a judgeable picture |
| 7 | board:1892 | `Typeface` amber -> green: the sheet baked from a declared repertoire, `Shape` a pure lookup |
| 8 | board:1891 | `TriggerField` amber -> green: a volume declared where the body passes fires once per crossing |
| 9 | board:1538 | `World` red -> green (the eye leaves the ground layer), `DrawList` and `Frustum` amber -> green |
| 10 | board:1867 | `SubjectDraw` red -> green and `SUBJ` red -> green — the largest red on both maps |
| 11 | board:1575 | `LightVisibilityStage` amber -> green, and the car's shadow lands on the ground board:1890 laid |
