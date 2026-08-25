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

## The ledger — rewritten each round from what was SEEN (2026-08-25 13:50, HEAD 235e3f47)

The command, exactly as the architect's brief prescribes, no arguments beyond the door's:

```
make                          -> EXIT 0. build/liboutshine.a (163 objects), build/outshine-driver,
                                 build/outshine-viewer all link
build/outshine-driver --headless --into DIR --assets .../apps-driver-f31
   DRIVING 48.13720,11.57560 -> 48.15000,11.59000, 1280x720, headless
   ... 71 measured lines, every one carrying unit and population ...
   REFUSED the network holds both ends but no chain of ways joins them --
     18374 nodes of 45248 were reachable from the start
   NO DRIVE -- the picture is what stood without it
   KEPT DIR/refused.png
   1 still
```

| question | answer | what proves it |
|---|---|---|
| is there a program a user runs? | **yes** | `apps/driver/src/main.cpp`, 223 lines, `-Iinclude` alone |
| does the gate build it? | **YES — this is the hour's real movement** | `make` exits 0 and links both apps. Last round it exited 2 on `apps/viewer` |
| does the shipped scenario declare the drive? | **yes** | `apps/driver/src/f31.scenario` carries it; the run above took no `--from`/`--to` |
| did the drive leave its stills? | **NO — one `refused.png`** | the route refuses. Three distinct refusals were reproduced by hand: 1.78 km -> the search (board:1862); 400 m -> the corridor fit at a 91.4-degree junction corner (board:1887); 250 m -> `a corridor is fitted through 2..N vertices and this one carries 1`, start and destination snapping to one node and the message blaming the corridor for it |
| do consecutive stills DIFFER? | **not answerable — there is one image** | |
| is there ground under the car? | **NO** | `refused.png` is a car on white. `Engine::Compose` is declared in the public door (`include/Outshine.h:54`), lays the tile ring at `src/clients/Engine.cpp:280`, and `grep -rn Compose src include apps test` finds NO caller anywhere (board:1805) |
| a horizon behind it? | **NO** | same image |
| a sky above it? | **NO** | `SkyStage` is green in CURRENT and this frame never reaches it |
| is the car lit — does it cast a shadow? | **lit, no shadow, no receiver** | the key shades the body: specular along the shoulder, the glasshouse dark, the wheel arches and the tail lamp reading. Nothing receives |
| does it sit on a surface or float? | **it floats — there is no surface** | |
| road markings, guard rails, an oncoming carriageway? | **NO** | nothing declares them |
| buildings, trees, water beside the road? | **declared, not reached** | `Sim.h` is included by `src/clients/Sim.cpp` and nothing else (board:1805) |
| what does the picture do at one kilometre it does not do at another? | **nothing — the drive never starts** | |

Against the bar — Gran Turismo 7 on PS4 — the gap is not lighting or material response, which
are the only two things this image can be judged on and which are both defensible: the body
reads as painted metal, the glass reads as glass, the specular follows the shoulder line. The
gap is that **there is no scene**. No ground plane, no horizon, no sky, no shadow catcher, no
second object. GT7's weakest still has all five. Draw distance, geometry density and shadow
quality cannot be scored at all because nothing is drawn beyond one glTF.

**NICHT ABGENOMMEN.** Between this picture and the bar, in order:

1. a corner that is BUILT rather than reproduced, so a city route lays at all (board:1887)
2. a graph whose components are counted before they are blamed, so the search finds its end
   (board:1862)
3. ground, sky and horizon under the car — the driver calls `Engine::Compose` (board:1805)
4. ten stills spaced by distance, that DIFFER
5. the car's shadow on the surface it stands on (board:1575)
6. road furniture: markings, guard rails, the second carriageway
7. draw distance, material response and geometry density measured against a PHOTOGRAPH of the
   place the still claims to be

## What will be true

- [ ] Every row of the ledger says yes, and the architect writes *ABGENOMMEN* in its report.
- [ ] Each still is judged against a PHOTOGRAPH of the place it claims to be, not against its
      own previous version — a picture that agrees only with itself measures nothing.
