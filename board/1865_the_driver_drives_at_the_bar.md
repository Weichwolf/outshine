Type: feature
State: open
Parent: 1573
Area: apps
Tags: driver, acceptance, product

# The driver drives at the bar, and the architect signs it off

`apps/driver` is outshine's ONE integration test and simultaneously its product. Everything it
uses is library, and what the library owes is corpus cases against invariant oracles. Emergence is
judged HERE, on the picture, from what was SEEN by RUNNING the programme.

**The day the driver is a driving simulation at Gran Turismo 7's level in an OSM world and the
architect accepts it, outshine's integration test has passed.**

## The ledger -- 2026-08-25 22:5x, HEAD a32c4919, own worktree

The acceptance command, exactly as the brief prescribes:

```
make                                                        -> EXIT 0, both apps link
build/outshine-driver --headless --into DIR --assets .../apps-driver-f31
   ... 80-odd measured lines, each with unit and population ...
   REFUSED no chain of ways joins the two ends -- 26853 of 45248 joined, 20158 settled
   NO DRIVE -- the picture is what stood without it
   ZERO stills, DIR not created, no message, exit 1          <- second round running (board:1903)
```

The overridden drive that routes: `--from 48.13720,11.57560 --to 48.13600,11.58200` ->
`DROVE 1533 frames over 0.282 of 0.302 km, kept 9 still(s)`.

| question | answer | what proves it |
|---|---|---|
| is there a program a user runs? | **yes** | `apps/driver/src/main.cpp`, `-Iinclude` alone |
| does the gate build it? | **yes** | `make` exits 0 |
| does the DECLARED drive leave its stills? | **NO -- zero, and no directory** | the graph falls into 4193 pieces (board:1862); the missing `mkdir` is board:1903, unfixed for two rounds |
| does an OVERRIDDEN drive? | **nine of ten** | the tenth needs `alongM >= routeM` (board:1903) |
| do consecutive stills DIFFER? | **yes, and barely** | nine distinct hashes; 0.5 % of pixels change between consecutive stills, 1.5 % at the corner |
| **is there any COLOUR in the frame?** | **NO. The picture is BLACK** | exactly two alpha values exist -- 255 on 570 941 px, 102 on 350 659 px -- and `max(R,G,B)` averages **0.45 of 255**, with 0.77 % of pixels above 32. What looked like a silver car in every round before this one was an image viewer compositing the ALPHA channel over its own white page (board:1870, board:1893) |
| is there ground under the car? | **NO** | `the ground did not compose: ... the ground is APPENDED to the vehicle's own glTF` -- an honest refusal now, after 823 corridor stations. `const bool overADrive = false;` (src/engine/Engine.cpp:283) |
| a horizon behind it, a sky above it? | **NO** | same. 258 batches drawn, all of them the car |
| where is the camera? | **in the cabin, and that is NEW this hour** | `eye - mesh = 0.279 m`; the view offset is subtracted from the centre of mass now, and the still shows the A-pillar, the windscreen surround and the near door mirror from within (board:1890) |
| is the car lit? | **NO** | 99 % of its own pixels below RGB 32 under a 40 000 lux key (board:1893) |
| does it cast a shadow? | **it casts 258 batches onto nothing** | the shadow radius derives from the subject's extent now (board:1867) and there is no surface to receive it |
| does it sit on a surface or float? | **it floats -- there is no surface** | |
| does a key move the car? | **NO** | `apps/driver` offers no `Host` (board:1803) |
| road markings, guard rails, an oncoming carriageway? | **NO** | nothing declares them |
| buildings, trees, water beside the road? | **declared, not reached** | 37 of 150 sources are linked by no suite, and the generators are most of them (STATE.md) |
| what does the picture do at one kilometre it does not do at another? | **it gets 4x brighter at the corner, from 0.45 to 1.7 out of 255** | the heading changes and the shading changes with it, which a horizontal surface under a fixed key cannot do (board:1893) |

Against the bar -- Gran Turismo 7 on PS4 -- nothing on the usual axes can be scored, because the
frame carries no colour. GT7's weakest still has ground, horizon, sky, a shadow and a second
object, all of them lit; this has a cabin interior at RGB 0.

**NICHT ABGENOMMEN.** Between this picture and the bar, in order:

1. the frame carries COLOUR: the clear colour is declared and the car is lit as declared
   (board:1870, board:1893). Until then no still in this tree can be judged against anything
2. ground, sky and horizon under the car: `overADrive` stops being a literal, and the three
   named lines that move 258 batches to 517 land (board:1890)
3. the shipped scenario ROUTES (board:1862, board:1894)
4. the acceptance command leaves its stills without a `mkdir`, and keeps N of N (board:1903)
5. the car's shadow lands on that ground (board:1575)
6. road furniture: markings, guard rails, the second carriageway
7. a key that moves the car, press AND release (board:1803)
8. draw distance, material response and geometry density measured against a PHOTOGRAPH of the
   place the still claims to be

## What will be true

- [ ] Every row of the ledger says yes, and the architect writes *ABGENOMMEN* in its report.
- [ ] Each still is judged against a PHOTOGRAPH of the place it claims to be, not against its
      own previous version -- a picture that agrees only with itself measures nothing.
