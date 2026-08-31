Type: feature
State: active
Parent: 1573
Area: apps
Tags: driver, acceptance, product

# The driver drives at the bar, and its picture is judged on the stills it takes itself

**Benchmark** — Unreal: a producer signs off a build against a look target. RAGE: the same. **Both agree** — the picture is judged by someone who did not build it, on shots they chose.

the driver client (deleted) is outshine's ONE integration test and simultaneously its product. Everything it
uses is library, and what the library owes is corpus cases against invariant oracles. Emergence is
judged HERE, on the picture, from what was SEEN by RUNNING the programme.

**The day the driver is a driving simulation at Gran Turismo 7's level in an OSM world and the
owner accepts it, outshine's integration test has passed.**

## The ledger -- HEAD bb9472db, own worktree, four runs, 32 stills

Two routes chosen to ask different questions, each driven in first person and in chase:

    Ludwigstrasse  48.1420,11.5800 -> 48.1518,11.5820   a boulevard walled on both sides
    the Isar       48.1310,11.5820 -> 48.1290,11.5930   the route crosses a river
    build/outshine-driver --headless --every --frames 1200|1800 --stills 8 --into DIR

| question | answer | what proves it |
|---|---|---|
| is there a program a user runs? | **yes** | both routes drove and left 8 of 8 stills, into directories that did not exist |
| do consecutive stills DIFFER? | **yes, and too little** | 2.1 % to 56 %. City chase 05->06: **3.8 %** of pixels over 60 m at 90 km/h. Nothing at a finite distance to have parallax against |
| is there a sky? | **yes, a gradient** | 149 at the top of the band, 127..131 at the horizon; identical across all 8 stills of a route on which the car turns |
| is there GROUND under the car? | **yes, and it is one flat painted colour** | a 170x25 px box reads 61..62 in R with no variation. Present, but with no texture, no albedo variation and no normal detail |
| **is the ground ONE lit surface?** | **NO** | a straight terminator crosses it, 15:1 in two pixels, sweeping across the drive; stills 01..04 of the river route render the whole ground 4 stops under its own sky (board:1935) |
| **is there a ROAD?** | **NO** | no carriageway, no asphalt distinct from the verge, no centreline, no edge line, no kerb, no oncoming lane. The car drives on the same painted plane as the field (board:1505) |
| is the car lit? | **badly** | roof (16,30,54) and tailgate (17,32,60) read the SAME, and both a quarter of the grass beside them at (64,79,60). ~3.5 stops adrift, no directional term (board:1934) |
| **does it cast a contact shadow?** | **NO** | ground under the car (62.4,77.0,56.0) against ground 200 px to the side (63.1,77.9,57.0). **0.7 counts.** 259 shadow batches drawn per frame, atlas least depth 1, most 1 (board:1575) |
| does it sit on the surface or float? | **it floats** | nothing under it says otherwise |
| is the glass glass? | **NO** | through the rear window (64.0,78.2,58.1), the ground beside the car (64.0,79.0,59.5). A hole (board:1934) |
| **is there a world beside the road?** | **NO** | no building on a boulevard walled with them, no tree, no water where the route crosses the Isar, though the reader prints 76 bridge ways (board:1936) |
| is there a sun? | **judged by consequence: NO** | no disc reachable by any route the driver offers, and neither of its consequences is present -- no highlight on the paint, no cast shadow on the ground (board:1868, 1575) |
| how bright is the frame? | **peak 130 of 255 first person, 149 chase** | exposure a constant 5.20833e-05 1/(cd/m2) on BOTH routes regardless of content. Nothing in any of 32 stills reaches white |
| what does the picture do at one km it does not do at another? | **it changes its ground by 15x** | same clock, flat route, 400 m apart |
| does a key move the car? | **NO** | the driver client (deleted) offers no `Host` (board:1803) |
| can the acceptance command choose the view? | **NO** | `--help` offers no view flag; the declared `chase` view was reachable only by copying the scenario and editing it. Half of every declared still is unlit cabin |

Two of the five routes chosen refused to lay a corridor: `48.1392,11.5875 -> 48.1368,11.5965`
(Maximiliansbruecke) with *the bend over vertices 2..2 ... carries only 4.205535 m, tighter than
the 4.901673 m this vehicle can bend to*, and `48.1400,11.5860 -> 48.1360,11.5975` with *no chain
of ways joins the two ends* over a graph in which 43 751 of 45 800 nodes reach BOTH ends
(board:1916).

Against the bar -- GT7 on PS4 -- the gap is in KIND, not degree. GT7's weakest still has a
carriageway with paint on it, a kerb, a verge and something standing beside the road. This has a
car with no shadow on a painted plane with a light-visibility seam across it, under a gradient.

**NICHT ABGENOMMEN.** Between this picture and the bar, in the order it is wanted:

1. a ROAD in the picture: carriageway surface, centreline, edge line, kerb, verge, oncoming
   carriageway (board:1505, 1499)
2. something standing beside it: buildings, trees, the water the route crosses (board:1936)
3. the ground is ONE lit surface -- the terminator goes (board:1935)
4. a contact shadow under the car (board:1575)
5. the key reaches the paint, and the glass stops being a hole (board:1934, 1868)
6. the frame uses its range: an exposure derived from what is in the scene (board:1868, 1908)
7. a bridge above what it crosses (board:1813)
8. a view flag and a key that moves the car (board:1803)
9. each still judged against a PHOTOGRAPH of the place it claims to be

## What will be true

- [ ] Every row of the ledger says yes, and the owner writes *ABGENOMMEN* in its report.
- [ ] Each still is judged against a PHOTOGRAPH of the place it claims to be, not against its
      own previous version -- a picture that agrees only with itself measures nothing.
