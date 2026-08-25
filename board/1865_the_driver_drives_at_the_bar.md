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

## The ledger — rewritten each round from what was SEEN (2026-08-25 15:4x, HEAD 817ea333)

The command, exactly as the architect's brief prescribes, no arguments beyond the door's:

```
make                          -> EXIT 0. liboutshine.a, outshine-driver, outshine-viewer link
build/outshine-driver --headless --into DIR --assets .../apps-driver-f31
   DRIVING 48.13720,11.57560 -> 48.15000,11.59000, 1280x720, headless
   ... 80-odd measured lines, every one carrying unit and population ...
   pieces the graph falls into = 4193 pieces
   REFUSED no chain of ways joins the two ends -- 26853 nodes of 45248 are joined to the start
     by ANY edge, and the search settled 20158 of those
   NO DRIVE -- the picture is what stood without it
   KEPT DIR/refused.png -- a failure is loud, and something is always drawn
   1 still, exit 1
```

The overridden drive that DOES route: `--from 48.13720,11.57560 --to 48.13600,11.58200` ->
`DROVE 1534 frames over 0.282 of 0.302 km, kept 9 still(s)`, nine distinct sha256.

| question | answer | what proves it |
|---|---|---|
| is there a program a user runs? | **yes** | `apps/driver/src/main.cpp`, `-Iinclude` alone |
| does the gate build it? | **yes** | `make` exits 0, both apps link |
| does the DECLARED drive leave its stills? | **NO — one `refused.png`, exit 1** | the graph falls into 4193 pieces; the largest holds 26807 of 45248 nodes and the two ends are not both in it (board:1862) |
| does an OVERRIDDEN drive? | **nine stills, not ten** | 302 m route, `kept 9 still(s)`. The tenth needs `alongM >= routeM` (main.cpp:196-198) and the drive stops at 282 m inside its arrival tolerance; the first needs a tenth of the route, so neither end of the drive is pictured (board:1862) |
| do consecutive stills DIFFER? | **yes, and almost nothing in them moves** | nine distinct hashes; the subject's screen bounding box moves 3 px over 282 m, because the camera is bolted to the body and there is nothing else in the frame to move against |
| is there ground under the car? | **NO** | `the ground did not compose: the scenario declares neither a sphere nor a drive that laid a corridor` — printed AFTER `ROUTED the declared drive` and 823 corridor stations. `const bool overADrive = false;` (src/clients/Engine.cpp:271) nails the branch shut, so the refusal states something the same run has just disproved |
| a horizon behind it? | **NO** | same |
| a sky above it? | **NO** | `SkyStage` is green in CURRENT and this frame never reaches it |
| **what IS behind the car?** | **nothing at all — `alpha = 0`** | every still is RGBA with a transparent background; the car is 38 473 of 921 600 pixels and the declared `fill="0.9"` never reaches the picture (board:1870). What looks like white is the image viewer |
| is the car lit? | **NO, and not only while driving** | subject mean luminance 15.1 -> 12.6 -> 12.8 over along01..07 and **45.3** at along08, with the same triangles on the same pixels. Only the heading changed. A horizontal roof under `elevationDeg="42"` cannot change with heading — the key's up and the body's up are different axes (board:1893). `refused.png` has the same defect: silver flanks, `#000` roof |
| does it cast a shadow? | **NO** | no `.scenario` in the tree declares a shadow radius, so `LightVisibility` is never in the plan (board:1575) |
| does it sit on a surface or float? | **it floats — there is no surface** | |
| is the camera where the scenario put it? | **NO** | `<player view="eyes">` declares a first-person eye at `(-0.494, 1.220, 0.003)` inside the cabin. All nine stills look DOWN at the roof from outside, the body cropped at the bottom edge, occupying the lowest 84 of 720 rows (board:1890) |
| does a key move the car? | **NO** | the engine names no action now (board:1803 half-delivered), and `apps/driver` offers no `Host`, so `Engine::Handles` returns at `S_->Offered == nullptr` (Engine.cpp:422) |
| road markings, guard rails, an oncoming carriageway? | **NO** | nothing declares them |
| buildings, trees, water beside the road? | **declared, not reached** | `Sim.h` has one consumer, `src/clients/Sim.cpp:1` (board:1805) |
| what does the picture do at one kilometre it does not do at another? | **it gets brighter at the corner** | the only difference across 282 m is the lighting defect above |

Against the bar — Gran Turismo 7 on PS4 — nothing on the usual axes can be scored, because
nothing is drawn beyond one glTF on transparency. What CAN be judged: the body reads as painted
metal where it is lit at all, the glass reads as glass, the specular follows the shoulder line
and the tail lamp is the right red. GT7's weakest still has ground, horizon, sky, a shadow and a
second object; this has one of five, and the one it has is lit from the wrong direction.

**NICHT ABGENOMMEN.** Between this picture and the bar, in order:

1. the key's up axis and the body's up axis are ONE axis, so a roof under a 42-degree key is lit
   at `sin(42)` at every heading (board:1893). Nothing in the picture can be judged until it is
2. the declared fill stands behind the subject — a still with an `alpha = 0` background has
   drawn nothing (board:1870)
3. the shipped scenario ROUTES: 6340 crossings without a shared node, told apart from bridges by
   the `bridge`/`tunnel` keys the tiles carry (board:1862), and the weld stops reading an index
   it has invalidated (board:1894)
4. the declared view is TAKEN — a first-person eye inside the cabin (board:1890)
5. ground, sky and horizon: the ring anchored on the corridor's origin, and a refusal that does
   not deny a corridor the same run laid (board:1890, board:1805)
6. ten stills, not nine, spaced at `k/10` for `k = 0..9` so the start and the arrival are both
   in the set, over a route long enough for one kilometre to differ from another
7. the car's shadow on that ground (board:1575)
8. road furniture: markings, guard rails, the second carriageway
9. a key that moves the car (board:1803)
10. draw distance, material response and geometry density measured against a PHOTOGRAPH of the
    place the still claims to be

## What will be true

- [ ] Every row of the ledger says yes, and the architect writes *ABGENOMMEN* in its report.
- [ ] Each still is judged against a PHOTOGRAPH of the place it claims to be, not against its
      own previous version — a picture that agrees only with itself measures nothing.
