Type: bug
State: active
Parent: 1953
Area: render
Tags: measured, picture, regression

# One pre-view translation per frame, and every matrix reads it

Measured on the reference drive at HEAD, with the light matrix's own output printed:

    the shadow radius it stood on        2.63727 m     (the car's extent -- correct)
    its centre, east / up                -0.0811 / 523.038 m
    the residency anchor, east / up      6.37814e+06 / 0 m
    the first cast batch, in LIGHT CLIP  x -171.381   y 213.871   z 15.2579

The clip volume is [-1,1] in x and y and [0,1] in z. The first caster lands **171 units** outside
in x and **214** in y -- times the 2.637 m radius, that is 452 m along the light's right and
**564 m** along its up. The car is four metres long.

So the frustum is built for the car, correctly, and the casters arrive half a kilometre away from
it. Nothing lands inside and the atlas comes back empty.

## What it was before, and why the number moved

board:1921 closed with `least 0.000, most 0.618, 779086 texels` on a subject that was the car
ALONE. The ground ring joined the subject afterwards (board:1890), and with it the residency's
anchor and the placements changed. Two readings since:

- with the ring casting: every one of 4194304 texels at depth **1** -- an 815 m ring drawn into a
  2.6 m frustum, every fragment beyond the near plane and clamped
- with only the carried parts casting (`CastsBelow`, this session): **0** texels

Both are wrong and they are the same defect seen from two sides: what the frustum is centred on
and what the casters' translations are do not stand in one space. 564 m is close to the ring's own
relief span (489.789 m lowest vertex against a car at 522.802 m) which points at an ALTITUDE
datum, but that is a lead and not a finding -- the number that will settle it is the light-space
coordinate of a caster whose world position is independently known.

## REPAIRED, and measured on the drive rather than in a fixture

The two spaces are one. `LightVisibilityStage::Build` centres the frustum on the placements
`SubjectResidency` already holds, filtered to the casting slots -- the same array `Cast` reads --
instead of on a value `Live::PlacedBounds` computed separately in world-ASL. `ShadowCentre` and
`LightVisibilityStage::Frame` are deleted with it: a second source for one number is what the
defect was.

A second defect, found by looking at the picture once the atlas filled: **the shadow test was
inverted for reversed-Z.** The atlas clears to 0, which is FARTHEST, and `subjectLit.msl` asked
`lit.z - bias > nearest`. A texel nothing wrote reads 0, so every unwritten texel came back
SHADOWED and the frustum's whole footprint was a black rectangle on the ground with the true
silhouette inside it. It asks `lit.z + bias < nearest` now.

    the shadow atlas, least depth   0.000
    its most                        0.511894
    texels above the clear          1165450    of 4194304
    ground in the car's shadow      R 15.1  G 22.0  B 22.0
    ground 260 px beside it         R 63.7  G 78.4  B 59.2
    apart                           56.4 counts

against the stakeholder's 0.2 to 0.8 counts for the same measurement.

## Why this is not yet CLOSED, and what two attempts cost

The proving case is not written. It wants one receiver and one caster under one key, differing
only in whether something stands in front of the other. Two attempts, and the second is the
informative one because board:1952 removed the first attempt's blocker:

    the wall alone      brightest 163, dimmest DRAWN 163
    under a caster      brightest 163, dimmest DRAWN 163
    the light           casts 2 batches, 3 shadowed frames, 1525080 texels written

The atlas is written, the subject is drawn shadowed, and EVERY drawn pixel is the same value in
both arms. So the receiver never reads a shadowed texel, in a scene where the driver's own ground
does -- 56 counts apart, measured above.

What differs between the two: on the drive the receiver is the ground ring, which does NOT cast
(`CastsBelow`), and the caster is the car. Here both parts cast, so the wall writes itself into
the atlas at its own depth. That is a lead, not a finding.

And a second lead in the same neighbourhood, found by reading rather than measuring, so it is
recorded as a suspicion: `SubjectDraw.cpp:741` applies `Anchor - ctx.Eye` to the model matrix
ONLY when `Placed_` is empty, and `LightVisibilityStage::Cast` applies it ALWAYS. The two agree in
both branches only if a non-empty `Placed_` already carries the shift. Whether it does is exactly
the question the head box asks -- one declared pre-view translation would make it unaskable.

The fixture cost is itself a finding: standing a two-part scene with a shadow through the door
took four refusals from the glTF reader on the file route (each correct, listed in board:1952) and
two full diagnostic rounds on the handed route. A door whose simplest lit scene is that hard to
stand in a case is a door that is hard to USE.

## WHAT RAGE AND UNREAL DO, and it is more than the repair

Unreal keeps ONE origin per frame -- `FViewMatrices::PreViewTranslation`. It is chosen once and
applied to EVERY matrix the frame builds: the view, the light, the instance transforms. No
subsystem translates for itself; the translation is a property of the FRAME. RAGE is
reconstructed and the shape is the same, camera-relative with one origin a frame.

That is exactly what was missing. `Live::PlacedBounds` worked in world-ASL and
`LightVisibilityStage::Cast` applied `Anchor - eye` to the placements: two subsystems, each with
its own translation, agreeing by luck until the ground ring changed the anchor.

The repair below centres the light on the placements the residency already holds, which is
correct and LOCAL. The benchmark's answer is general and this item now asks for it: one declared
pre-view translation, read by everything that builds a matrix, so a third subsystem cannot invent
a fourth space. CLAUDE.md already names the boundary -- *precision has ONE boundary and it is the
camera* -- and this is the same sentence seen from the matrix side.

## What will be true

- [ ] The frame declares ONE pre-view translation, and every matrix it builds reads that one --
      view, light and instance alike. Unreal's `PreViewTranslation` is the shape; a subsystem
      that subtracts an origin for itself is the defect, whoever gets it right.
- [x] The light frustum is built in the space its casters are in: the stage centres on the
      placements `SubjectResidency` holds, and `ShadowCentre` and `Frame` are deleted because a
      second source for one number was the defect.
- [ ] Proving case: a subject at a known world position casts into an atlas whose written texels
      lie where the closed form puts them, and a caster moved by a known distance moves the
      written region by the corresponding number of texels. Negative control: the anchor added on
      one side only, and the region leaves the atlas.
- [ ] `CastsBelow` stays or goes with a reason: an 815 m ring must not cast into a frustum sized
      for a car, but the answer may be cascades (board:1926) rather than a filter.
