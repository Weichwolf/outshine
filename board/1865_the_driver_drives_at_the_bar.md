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

## The ledger — rewritten each round from what was SEEN (2026-08-25 19:1x, HEAD c0de1b18)

The command, exactly as the architect's brief prescribes, no arguments beyond the door's:

```
make                          -> EXIT 0. liboutshine.a, outshine-driver, outshine-viewer link
build/outshine-driver --headless --into DIR --assets .../apps-driver-f31
   DRIVING 48.13720,11.57560 -> 48.15000,11.59000, 1280x720, headless
   ... 80-odd measured lines, every one carrying unit and population ...
   pieces the graph falls into = 4193 pieces
   REFUSED no chain of ways joins the two ends -- 26853 of 45248 joined, 20158 settled
   NO DRIVE -- the picture is what stood without it
   ZERO stills, no message, exit 1   <- DIR did not exist and nobody made it (board:1903)
   with `mkdir -p DIR` first: 1 still, refused.png, exit 1
```

The overridden drive that DOES route: `--from 48.13720,11.57560 --to 48.13600,11.58200` ->
`DROVE 1534 frames over 0.282 of 0.302 km, kept 9 still(s)`.

**Every number below is byte-identical to the last round's.** Same nine sha256, same nine mean
luminances to one decimal, same bounding boxes. The picture did not move this hour.

| question | answer | what proves it |
|---|---|---|
| is there a program a user runs? | **yes** | `apps/driver/src/main.cpp`, `-Iinclude` alone |
| does the gate build it? | **yes** | `make` exits 0, both apps link |
| does the DECLARED drive leave its stills? | **NO — zero, then one after `mkdir`** | the graph falls into 4193 pieces and the two ends are not both in the largest (board:1862); the missing directory is board:1903 |
| does an OVERRIDDEN drive? | **nine, not ten; `--stills 1` keeps zero** | the last still needs `alongM >= routeM` and the drive stops at 282 m of 302 m (main.cpp:196-198, board:1903) |
| do consecutive stills DIFFER? | **yes, and almost nothing in them moves** | nine distinct hashes; the subject's box moves 3 px over 282 m |
| is there ground under the car? | **NO** | `the ground did not compose: the scenario declares neither a sphere nor a drive that laid a corridor`, printed after 823 corridor stations. `const bool overADrive = false;` (src/clients/Engine.cpp:284) |
| a horizon behind it? | **NO** | same |
| a sky above it? | **NO** | `SkyStage` is green in CURRENT and this frame never reaches it |
| **what IS behind the car?** | **nothing — `alpha = 0`** | 17 055 of 18 849 sampled pixels are fully transparent; `fill="0.9"` has never reached the picture (board:1870). The white is the image viewer |
| is the car lit? | **NO — a silver flank and a `#000` roof** | mean luminance 15.1 / 14.3 / 13.5 / 12.8 / 12.6 / 12.5 / 12.8 / **45.3** / 43.8 over along01..09, the same triangles on the same pixels. A horizontal roof under `elevationDeg="42"` cannot triple with heading — the key's up and the body's up are two axes (board:1893). Unchanged by this hour's two repairs |
| does it cast a shadow? | **NO** | no `.scenario` in the tree declares a shadow radius, so `LightVisibility` is never in the plan (board:1575) |
| does it sit on a surface or float? | **it floats — there is no surface** | |
| is the camera where the scenario put it? | **the eye is, the CAR is not** | `<view id="eyes">` at `(-0.494, 1.220, 0.003)` IS taken: raising `offsetY` by 2 m empties the frame entirely, and by 50 m gives the same bytes. The car stands ~6 m below and ~12 m from the seat it is declared under (board:1890) |
| does a key move the car? | **NO, and now the release is gone too** | `apps/driver` offers no `Host`; and `Engine::Handles` filters to `SDL_EVENT_KEY_DOWN` alone (src/clients/Engine.cpp:431), so the pump's release value is dead code and a throttle would latch (board:1803) |
| road markings, guard rails, an oncoming carriageway? | **NO** | nothing declares them |
| buildings, trees, water beside the road? | **declared, not reached** | `Sim.h` has one consumer, `src/clients/Sim.cpp:1` (board:1805) |
| what does the picture do at one kilometre it does not do at another? | **it gets brighter at the corner** | the only difference across 282 m is the lighting defect above |

Against the bar — Gran Turismo 7 on PS4 — nothing on the usual axes can be scored, because
nothing is drawn beyond one glTF on transparency. What CAN be judged: the body reads as painted
metal where it is lit at all, the glass reads as glass, the specular follows the shoulder line
and the tail lamp is the right red. GT7's weakest still has ground, horizon, sky, a shadow and a
second object; this has one of five, and the one it has is lit from the wrong direction.

**NICHT ABGENOMMEN.** Between this picture and the bar, in order:

1. the key's up axis and the body's up axis are ONE axis (board:1893). Two candidates were ruled
   out this hour and the nine luminances did not change by 0.1 — nothing in the picture can be
   judged until this is fixed
2. the declared fill stands behind the subject — an `alpha = 0` background has drawn nothing
   (board:1870)
3. the shipped scenario ROUTES: 6340 crossings without a shared node (board:1862), and the weld
   stops reading an index it has invalidated (board:1894)
4. the car is drawn where the seat is (board:1890) — the eye is right, the mesh is ten metres off
5. ground, sky and horizon: `overADrive` stops being a literal `false` (board:1890, board:1805)
6. the acceptance command leaves its stills without a `mkdir`, and keeps N of N (board:1903)
7. the car's shadow on that ground (board:1575)
8. road furniture: markings, guard rails, the second carriageway
9. a key that moves the car, press AND release (board:1803)
10. draw distance, material response and geometry density measured against a PHOTOGRAPH of the
    place the still claims to be

## What will be true

- [ ] Every row of the ledger says yes, and the architect writes *ABGENOMMEN* in its report.
- [ ] Each still is judged against a PHOTOGRAPH of the place it claims to be, not against its
      own previous version — a picture that agrees only with itself measures nothing.
