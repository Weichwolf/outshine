Type: bug
Area: render
Tags: bug

**The car is in the picture AND VISIBLE IN IT**

**Measured, in one frame, from the driver's own tool:**

```
PARTS total 260  carried 258   car at -34.9 -0.9 -167.5
EYE third   at  -33.3  0.6 -160.7   fwd -0.221 0.008 -0.975
```

| What the numbers say | |
|---|---|
| the car's parts | **258 of the picture's 260**, and they carry the vehicle placement |
| eye to car, projected on the view axis | `(-1.6, -1.5, -6.8) . (-0.221, 0.008, -0.975)` = **+6.97 m** |
| so the car stands | **seven metres dead ahead** of a camera pointed at it, 1.5 m up |

**And it is not in the frame.** The chase view is pixel-for-pixel the driver's view: the same
carriageway, the same embankment, no vehicle.

## What is already ruled out

| Guess | Ruled out by |
|---|---|
| the car is not in the subject | 258 parts of 260, printed |
| the placement table is short, so its parts fall back to slot 0 | `Stand()` seeds identity for every part and `BuildDrawList` gives each its own slot -- that was `board:1548` and it is closed |
| the chase camera is not behind the car | +6.97 m along the view axis, printed above |
| the road hides it | the road is 0.35 m proud of a formation the car stands ON, and the car is 4.6 m long |

## BISECTED -- the drawn car is 65 TIMES TOO BIG

**Drawn alone, from (0, 1.5, 7) looking down -Z -- its own declared chase offset -- the frame fills
with black geometry. The camera is INSIDE the car.** Measured:

```
CARBOUNDS  span 132.728 x 94.267 x 297.584 m
an F31 is         1.811 x  1.440 x   4.624 m
           ratio    73.3 x    65.5 x    64.4
```

**The scenario declares `wheelbaseM="2.810"` and `board:1511` records that the asset's scale was
derived from that dimension** -- for the PHYSICS. `Live::Build` reads `scene.gltf` raw and applies
nothing, so the drawn subject is a 300 m object and every camera that should see a car is standing
inside one.

**And it corrects an attribution I made earlier in the same session.** The vertical span of -60.9 to
33.3 m that I read off the picture's bounds and put down to the corridor was **always the car**. A
94 m tall thing was in every frame from the moment the F31 was joined, and nothing said so because
nothing measured the subject it stood up.

## What must be true

- [ ] **A picture that carries a subject's parts draws them**, and a case decides it: stand the F31 up
      alone, aim a declared camera at it from its own declared chase distance, and assert the opaque
      fraction is not zero
- [ ] **Whatever the answer is, it is not another hypothesis** -- this one is settled by bisection,
      the way `board:1529`'s road was: draw the car alone, with no ground and no corridor, from the
      same camera

## Comments

**The session that produced this measured thirteen hypotheses about one picture and refuted eleven by
running them.** The two that held were both settled by BISECTION rather than by reasoning: the
placement order in `board:1548`, and the road-under-ground occlusion in `board:1529`. That is the
lesson worth carrying -- halve the picture before reasoning about it.

**Everything around the car now works**: the carriageway runs to the vanishing point with its
embankment shaded, the ground is graded to the formation with a reach the RAA prescribes, the exposure
is derived from the declared illuminance, and the frame is 50.59 % opaque against 1.09 % when this
began.
