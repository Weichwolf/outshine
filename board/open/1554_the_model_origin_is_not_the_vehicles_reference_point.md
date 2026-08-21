Type: bug
Area: clients
Tags: bug

**The model origin is not the vehicle's REFERENCE POINT, and the declaration says where it is**

**Measured off the drawn subject, at the scale `board:1551` derived (0.015550 m per unit):**

| about the model origin | |
|---|---|
| x | **-0.097 .. +1.967 m** |
| y | **-0.948 .. +0.518 m** |
| z | **-1.835 .. +2.792 m** |

**The bodywork lies almost entirely to ONE SIDE of its own origin** -- about a metre across, half a
metre forward -- and reaches 0.948 m below it.

**The scenario places the body at the CENTRE OF MASS**, declared `<centreOfMass y="0.55"/>`, so the car
spans **-0.398 to +1.068 m about the ground**: its underside sits **0.398 m INSIDE the ground it stands
on**, which is deeper than the carriageway's own 0.35 m thickness.

**And the physics does not agree with it.** The same scenario declares four contacts at
`y = 0.333` -- a tyre radius -- and `z = +-1.405`, `x = +-0.774`: a body frame **centred between the
axles at wheel-hub height**. `board:1511` measured those positions off the asset's `f31_gum` material
and they are right. What is wrong is that the DRAWN model is placed by its own origin, which is
somewhere else entirely.

## What must be true

- [ ] **The declaration carries the offset from the model's origin to the vehicle's reference point**,
      the way it now carries `assetWheelbase` -- measured once, declared beside the dimension it was
      measured with, never assumed
- [ ] **The drawn car and the contacts stand in one frame**, which is the same rule the goal states
      for the road: *a road the physics agrees with but nobody can see is two roads*
- [ ] **A car sunk into its own carriageway is a refusal, not a picture** -- the geometry knows the
      contacts' height and can say so

## Comments

**This is the tenth thing measured on `board:1551` and the first that is wrong rather than merely
ruled out.** Nine causes were eliminated -- draws submitted, parts present, placement applied, scale
derived, surfaces separated, cameras distinct, frames shared, `Restand` faithful, contrast fixed --
and the answer was in the asset's own bounding box the whole time, unmeasured because nothing had
asked where the model sits relative to the point it is placed by.
