Type: feature
State: open
Parent: 1573
Area: apps
Tags: driver, acceptance, product

# The driver drives at the bar, and the architect signs it off

`apps/driver` is outshine's ONE integration test and simultaneously its product. Everything it uses is
library, and what the library owes is corpus cases against invariant oracles. Emergence is judged
HERE, on the picture, by the hourly architect — from what it SAW by RUNNING the programme, never
from reading the implementation.

**The day the driver is a driving simulation at Gran Turismo 7's level in an OSM world and the
architect accepts it, outshine's integration test has passed.**

## The ledger — rewritten each round from what was SEEN (2026-08-25 15:0x, HEAD d4c8784c)

The command, exactly as the architect's brief prescribes, no arguments beyond the door's:

```
make                          -> EXIT 0. build/liboutshine.a (164 objects), build/outshine-driver,
                                 build/outshine-viewer all link
build/outshine-driver --headless --into DIR --assets .../apps-driver-f31
   DRIVING 48.13720,11.57560 -> 48.15000,11.59000, 1280x720, headless
   ... 70-odd measured lines, every one carrying unit and population ...
   turns the search refused as too sharp for the car = 34618 turns
   REFUSED the network holds both ends but no chain of ways joins them --
     18374 nodes of 45248 were reachable from the start
   NO DRIVE -- the picture is what stood without it
   KEPT DIR/refused.png
   1 still
```

**The shipped scenario still does not route, and that is unchanged from last round.** A 700 m
hop up one street refuses the same way. What DID change: the 136 m `--from`/`--to` variant now
ROUTES and keeps eight stills, all eight byte-distinct — the corridor fit is no longer the
blocker (board:1887 closed), the search is (board:1862).

| question | answer | what proves it |
|---|---|---|
| is there a program a user runs? | **yes** | `apps/driver/src/main.cpp`, `-Iinclude` alone |
| does the gate build it? | **yes** | `make` exits 0, both apps link |
| did the DECLARED drive leave its stills? | **NO — one `refused.png`** | the search refuses; 40.6 % of a 45248-node Munich graph reachable from its own start |
| did an OVERRIDDEN drive leave stills? | **YES — eight, and all eight differ** | `--from 48.13720,11.57560 --to 48.13500,11.57200` -> `DROVE 795 frames over 0.116 of 0.136 km, kept 8 still(s)`; eight distinct sha256 |
| do consecutive stills DIFFER? | **yes, and barely visibly** | the eight differ by a few hundred bytes of PNG; the car's roof shifts a few pixels between `along01` and `along08` |
| is there ground under the car? | **NO** | white to the frame edge. `Engine::Compose` is called by `Assemble` now and refuses at `Engine.cpp:274`: *the scenario declares no sphere*. No `.scenario` in the tree declares `<ground>` (board:1805, board:1890) |
| a horizon behind it? | **NO** | same |
| a sky above it? | **NO** | `SkyStage` is green in CURRENT and this frame never reaches it |
| is the car lit — does it cast a shadow? | **NOT WHILE IT DRIVES** | `refused.png` (studio, no drive) shades the body properly: specular along the shoulder, dark glasshouse, the tail lamp red. The SAME asset under the SAME `<lighting>` on the 136 m drive is a BLACK SILHOUETTE in all eight stills, chase view and first person alike — a roof under a 42-degree 40000 lux key reading `#000` (board:1893). Nothing receives a shadow either |
| does it sit on a surface or float? | **it floats — there is no surface** | |
| **is the camera where the scenario put it?** | **NO — this is the round's new defect** | `<player view="eyes">` declares a FIRST-PERSON eye at `(-0.494, 1.220, 0.003)`, inside the cabin. All eight stills look DOWN at the car's roof from outside and above-left, the body cropped at the bottom edge. `Engine::Rides` builds the eye in metres (Engine.cpp:836-841), `Live::Carry` scales the placement into model units (Live.cpp:539-547), and `Live::Eye` is handed the unscaled one (board:1890) |
| does a key move the car? | **NO** | five bindings declared, one has an effect. `Engine::Acts` (Engine.cpp:779) is `if (named != "next-view")` (board:1803) |
| road markings, guard rails, an oncoming carriageway? | **NO** | nothing declares them |
| buildings, trees, water beside the road? | **declared, not reached** | `Sim.h` has one consumer, `src/clients/Sim.cpp:1` (board:1805) |
| what does the picture do at one kilometre it does not do at another? | **nothing distinguishable** | the drive is 116 m long and the camera does not follow the road |

Against the bar — Gran Turismo 7 on PS4 — nothing on the usual axes can be scored, because
nothing is drawn beyond one glTF on a flat fill. The two things this picture CAN be judged on
are both defensible: the body reads as painted metal, the glass reads as glass, the specular
follows the shoulder line and the tail lamp is the right red. GT7's weakest still has ground,
horizon, sky, a shadow and a second object; this has one of five, and that one is the car.

The second product in the tree is a different matter and it moved this hour:
`outshine-viewer --headless --show apps/driver/src/f31.scenario --frames 3` now paints a real
face — corpus column, case list with a highlighted row, status line, three faces at two sizes,
the F31 beside it. That is the first text this engine has ever put on a frame.

**NICHT ABGENOMMEN.** Between this picture and the bar, in order:

1. connected components counted on the UNFILTERED graph, and a snap derived from what two tile
   grids can separate — the turn filter explains 8.2 of the 59 unreachable percentage points
   and no more, so the SHIPPED scenario routes (board:1862)
2. the driven car LIT — the same body the studio frame draws correctly, not a silhouette
   (board:1893). Nothing about this picture can be judged until it is
3. the declared view actually taken — a first-person eye inside the cabin, a chase camera seven
   metres behind (board:1890)
4. the ONE remaining space seam: the ground ring anchored on the corridor's origin, so
   `<ground>` composes for a drive and the first tile reaches a frame (board:1890, board:1805)
5. ten stills spaced by distance over a route long enough for one kilometre to differ from
   another
6. the car's shadow on the surface (board:1575)
7. road furniture: markings, guard rails, the second carriageway
8. a key that moves the car (board:1803)
9. draw distance, material response and geometry density measured against a PHOTOGRAPH of the
   place the still claims to be

## What will be true

- [ ] Every row of the ledger says yes, and the architect writes *ABGENOMMEN* in its report.
- [ ] Each still is judged against a PHOTOGRAPH of the place it claims to be, not against its
      own previous version — a picture that agrees only with itself measures nothing.
