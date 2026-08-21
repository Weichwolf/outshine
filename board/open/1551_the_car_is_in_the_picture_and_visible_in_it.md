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

## BISECTED AGAIN -- it draws ALONE and vanishes JOINED

**At the corrected scale, drawn as the only subject from (0, 1.5, 7) -- its own declared chase
offset -- the F31 is there and recognisable**: a 3 Series Touring from behind, dark body, pale glass
roof, rear window, red tail light, roof aerial.

**Joined to the ground and the corridor, from the same offset, it is gone.** So the defect is not the
placement, not the scale, not the camera and not the vehicle asset. **It is what happens when a file
subject and a built subject stand in one picture.**

That is the same shape as `board:1529`'s road, which drew alone and vanished under the ground -- and
this one is not occlusion, because a 4.6 m car seven metres ahead cannot hide behind a road surface
0.35 m proud of the formation it sits on.

**Where to look, in order:**

| | |
|---|---|
| ~~`Live::Build`'s surface table~~ | **RULED OUT.** Measured at the join: `parts 260  slots 24  partSlot 260`, and the material-slot ceiling is 16 777 216. Every table is the length it must be |
| ~~`Live::Advance` rebuilding the geometry~~ | **RULED OUT.** The asset declares `animations: 0` and `skins: 0` over its 519 nodes and 258 meshes, so `Moves_` is false, `Pose` does nothing and `Advance` rebuilds nothing |
| `Joined_` | **printed as 258 of 260**, so the car's parts are in the picture AND take the body placement. Recomputed inside `Build` from `Geometry_.Parts().size()` minus the built subject's, and read by `Carry` to decide which parts take the body |

**All three suspects the bisection named are now eliminated by measurement.** The car's 258 parts are
in the picture, carry the vehicle placement, and have a surface slot each in a table of the right
length; the geometry is not rebuilt because the asset has no animation; and nothing is capped.

**What is left is what happens between the draw list and the device**, and the next instrument is the
one this session never built: **how many draws the frame actually submitted**, against how many the
list holds. `SubjectDraw::DrawCount()` already sums `batch.Draws` and nothing reads it.

## THE INSTRUMENT ANSWERS -- every part is submitted

```
DRAWS the stage submits 260 for a picture of 260 parts
PARTS total 260 carried 258  car at -34.9 -0.9 -167.5
```

**260 draws for 260 parts.** The device is asked to draw every one, the car's 258 included, seven
metres ahead of a camera pointed at it. So it is not geometry, not placement, not scale, not
selection and not submission -- **every one of those is now measured rather than argued.**

**What is left is CONTRAST, and the reviewer measured it two rounds ago**: the car's pixels sit at
luminance p50 **76** and the ground surface at R = G = **76.6**, saturation p50 **0.038**. A vehicle
drawn against ground of the same brightness is submitted, rasterised, and indistinguishable. That is
his S5 -- *one single material colour for the whole of Germany* -- and it was never a missing draw.

## CONFIRMED IN THE PICTURE

**Looked at**: the ground is green, the carriageway a dark line across it, and the car a clearly
distinguishable dark shape standing ON the ground. The first frame of this session in which a person
would say *there is a car on a road*.

**Against the reviewer's own instruments, on his own measure:**

| | round 2 | now |
|---|---|---|
| saturation p50 | **0.038** | **0.31** |
| luminance band 96-223 | 0.35 % | **93.6 %** |
| pure white | 0.00 % | 0.00 % |

**The car was never missing a draw.** 260 draws were submitted for 260 parts all along; what was
missing was a second declared surface, so the ground wore the road's asphalt and a dark car stood
against ground of its own brightness.

## STILL OPEN, AND SHARPER -- visible when FRAMED, absent from the CHASE

**Both looked at, in the same run, at the same station:**

| view | what is in it |
|---|---|
| `000-framed-by-the-engine` | green ground, a dark road across it, and **the car, plainly distinguishable** |
| `km0017.3-third` | the same road and the same green ground, from 7 m behind the car -- **and no car** |

**So contrast is no longer the explanation.** The picture holds the car, 260 draws are submitted for
260 parts, the scale is right, and a camera framing the whole patch from kilometres out shows it while
a camera seven metres behind it does not.

**Two more ruled out in the same run.** `Restand` does not drop it -- the framed view at km 17.3, after several re-standings, still carries the car as a black shape on green ground. And the two driver cameras are genuinely distinct: `km0017.3-first` and `-third` differ in hash and in size, so `Look`'s declared branch is aiming them separately.

**A ninth: the frames agree.** Both the vehicle placement and the eye subtract the same `originM` -- `body[12+axis] -= originM[axis]` at both sites, and `where.EyeM[axis] -= originM[axis]` for the camera -- so the car is not sitting in absolute world coordinates while the camera works in local ones.

**What that leaves**: something about the chase placement or the near field, and it is now a much
narrower question than when this item opened -- geometry, scale, submission, surface and contrast are
each measured and each sound.

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
