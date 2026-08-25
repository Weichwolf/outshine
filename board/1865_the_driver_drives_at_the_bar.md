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

## The ledger — rewritten each round from what was SEEN (2026-08-25 11:17, HEAD a3ebe3e0)

The command, exactly as the architect's brief prescribes:

```
make                          -> EXIT 2, apps/viewer does not compile, but build/outshine-driver was linked first
build/outshine-driver --headless --into DIR --assets .../apps-driver-f31
   DRIVING 0.00000,0.00000 -> 0.00000,0.00000, 1280x720, headless
   NO DRIVE DECLARED
   STOPPED after 0 frames: no scenario is standing, so there is nothing to advance
   0 stills
```

| question | answer | what proves it |
|---|---|---|
| is there a program a user runs? | **yes** | `apps/driver/src/main.cpp`, 199 lines, `-Iinclude` alone, `--help` answers |
| does the gate build it? | **it builds; the gate does not finish** | `make` and `test/run.sh` both exit 2 on `apps/viewer/EveryCaseTheTreeDeclaresConfigures.cpp:8` (board:1869). `build/outshine-driver` is linked before the failure |
| did the drive leave its stills? | **NO — zero** | the run above. `Read` + `Assemble` + `Advance` stands no picture (board:1881) |
| do consecutive stills DIFFER? | **not answerable — there is one image or none** | with `--from 48.137,11.576 --to 48.200,11.600` the route refuses and exactly one `refused.png` is written |
| is there ground under the car? | **NO** | `refused.png` is a car on white at `fill="0.9"`. `Engine::Compose` would lay a 3x3 tile ring and no programme calls it (board:1805) |
| a horizon behind it? | **NO** | same image |
| a sky above it? | **NO** | `SkyStage` is green in CURRENT and the driver's frame never reaches it |
| is the car lit — does it cast a shadow? | **lit, no shadow, no receiver** | the key at `elevationDeg="42"` shades the body: specular along the shoulder line, the glasshouse dark, the wheel arches reading. Nothing receives |
| does it sit on a surface or float? | **it floats — there is no surface** | |
| road markings, guard rails, an oncoming carriageway? | **NO** | nothing declares them |
| buildings, trees, water beside the road? | **declared, not reached** | `Sim.h` is included by `src/clients/Sim.cpp` and nothing else (board:1805) |
| what does the picture do at one kilometre it does not do at another? | **nothing — the drive never starts** | the route refuses: *19406 nodes of 65615 were reachable from the start* |

What DID improve, and it is real: the refusal now draws. *A failure is loud, and something is
always drawn* — the engine refuses the route, keeps the frame, and writes a picture of what stood
without it (board:1870 delivered). The car is a clean, correctly shaded glTF stand-up. That is
the whole of this hour's visible movement.

**NICHT ABGENOMMEN.** Between this picture and the bar, in order:

1. a still at all from the shipped scenario — one arrival route through the door (board:1881)
2. ground, sky and horizon under the car — the driver calls `Engine::Compose` (board:1805)
3. a tile ring that joins, so the route exists to be driven (board:1862)
4. ten stills spaced by distance, that DIFFER
5. the car's shadow on the surface it stands on (board:1575)
6. road furniture: markings, guard rails, the second carriageway
7. draw distance, material response and geometry density measured against a PHOTOGRAPH of the
   place the still claims to be

## What will be true

- [ ] Every row of the ledger says yes, and the architect writes *ABGENOMMEN* in its report.
- [ ] Each still is judged against a PHOTOGRAPH of the place it claims to be, not against its
      own previous version — a picture that agrees only with itself measures nothing.
