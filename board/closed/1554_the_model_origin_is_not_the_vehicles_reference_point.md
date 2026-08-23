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

## MEASURED AND DECLARED

**Measured through the engine's own flatten**, over the car's 258 parts:

```
PATCHES 11492 points within 3 u of the lowest, centred at x 60.104 z 22.847 u (lowest y -60.939 u)
```

**The caveat first**: 11 492 points within 3 units of the lowest is the car's whole UNDERSIDE, not the
four tyre patches -- `board:1511` could filter by the `f31_gum` material and this cannot. For a
symmetric car the x centroid is the same either way; the z centroid is the underbody's, not the
axles'. **And x is confirmed independently**: the bounding box runs -0.097 to +1.967 m, whose midpoint
is 0.935 m, exactly the 60.104 u the patches give.

**The scenario carries all three now** -- `assetGround="-60.939"`, `assetCentreX="60.104"`,
`assetCentreZ="22.847"` -- and the driver carries the model onto that point through the body's own
rotation, so the shift turns with the car rather than being added in world axes.

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

---

## Repaid (2026-08-23)

All three boxes are ticked, and the third is ticked by making the sunk car UNSPELLABLE
rather than by adding a check beside it.

The offset was already measured and declared -- `assetWheelbase`, `assetGround`,
`assetCentreX`, `assetCentreZ` on `tools/driver/f31.scenario` -- but the ARITHMETIC that
turns those numbers into a placement lived in the driver
(`tools/driver/stills/StillsAreTakenAlongTheDriveForTheEye.cpp:367-373`), where the engine
could not see it and no unit case could reach it. `Vehicle::AssetGround` had no consumer in
`src/` at all: read by the scenario reader, used by nobody.

`Sim::Stand` now owns it. `Rigged` carries:

| field | what it is |
|---|---|
| `StandsAtM` | the plane the CONTACTS touch: `min(contact.y) - tyreRadius`, never an assumed zero |
| `MetresPerAssetUnit` | `wheelbaseM / assetWheelbase` -- the model carries no scale, so it is derived from the one dimension measured off the asset (board:1511, 1551) |
| `ModelShiftM[3]` | the offset from the model's origin to the vehicle's reference point |

and it REFUSES a vehicle that draws an asset without saying what it measures
(`assetWheelbase`) or where its own ground is (`assetGround` must be negative -- a model's
lowest point stands below its origin). The driver reads the three numbers instead of
computing a second copy of them.

**Measured**, F31, all values derived not assumed:

| | |
|---|---|
| metres per asset unit | 2.810 / 180.71 = 0.015549775884 |
| standing plane from the contacts | 0.333 - 0.333 = 0.000 m |
| lift | 0 - (-60.939 x 0.0155498) - 0.55 = **+0.397588 m** -- exactly the 0.398 m the body was sunk by |
| lateral shift | -60.104 x 0.0155498 = -0.934604 m |
| longitudinal shift | -22.847 x 0.0155498 = -0.355266 m |

- **Proving test**: `test/unit/sim/TheDrawnCarAndItsContactsStandInOneFrame` -- twelve checks:
  the derived scale, the standing plane read off the contacts, the model's lowest point
  landing EXACTLY on that plane, the three shift components, both refusals, and a vehicle
  whose hubs sit 0.167 m above the tyre radius, whose drawn body rises with them.
- **Negative controls**, both run:
  - the shift computed against 0 instead of `standsAt` -> `FAIL so the drawn body rises with
    them -- the frame is the contacts', never a constant the engine assumed`.
  - the `assetGround` refusal disabled -> `FAIL and an asset that does not say where its own
    ground is refuses too`.
- Gate 232/232. `test/run.sh tools` stands exactly as it did before this change (2 PASS,
  1 FAIL, 2 TIMEOUT, 4 BUILD -- all pre-existing, and the four viewer build failures are
  filed as board:1766).
