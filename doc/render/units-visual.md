# Units and effects in the picture

**Subject:** drawing what the simulation already has — other aircraft, released stores, missiles,
ground targets, and the effects that belong to them (smoke, flares, chaff). Neighbours:
[`renderer.md`](renderer.md) (pass topology and the encode slots this occupies),
[`../missions/runtime.md`](../missions/runtime.md) (what a unit is and where its pose
comes from), [`../weapons.md`](../weapons.md) (what exists to be drawn).

This file is written spec-first: the contract exists, the implementation does not.

## Spec

| Contract | Acceptance / measurement anchor |
|---|---|
| A frame proof can say something about units | today it cannot — a screenshot shows terrain and HUD only, no matter how many jets fly |
| Units are drawn from the BORROWED registry `FBWorld::Units()`, never from a second source of truth | the registry already reaches `FBWorld` (`SetUnits`); the renderer reads poses, never physics state |
| A pose read for drawing is the published pose | the snapshot barrier (`PublishPose`) already guarantees a completed tick — the renderer must not reach into `fb_fdm_state` of a foreign unit |
| Drawing costs nothing when nothing is there | the NoOp slots stay wired in the encode order (units after terrain, sprites before HUD); an empty registry draws zero calls |
| Camera-relative ECEF like everything else | unit transforms follow `FBCameraBasisEcef`; no separate world origin |
| Effects are data of the unit that owns them, not renderer state | chaff clouds and flares already exist as published signature data (`core/FBCountermeasure.h`) — the sprite stage reads, it does not invent |
| A retired unit disappears | `FBSimUnit::Retire` already clears the signature; the drawing side must honour the same rule |

Open in the spec, to be decided when the round starts: model source for a unit mesh (the JSBSim model
carries no geometry), level of detail policy, and whether ground targets get a silhouette or a marker.

## State

**Nothing is built.** `render/stages/FBUnitsStage` and `FBSpritesStage` exist as NoOp stages, wired
into the encode order (units after the terrain, sprites before the HUD) and drawing nothing.

Consequence, stated plainly: the entire multi-unit simulation — stages 1–6, datalink, radar, BFM,
intercept, weapons and the damage model — exists physically and in the telemetry, and there is no
picture of it. A released store, a missile in flight, a ground target and a burning wingman are all
invisible.

## Gaps

### Open work (from the retired `TODO.md` §4.1)

| # | Thing |
|---|---|
| 4.1 | `FBUnitsStage` / `FBSpritesStage` are NoOp — **the most conspicuous difference between what the simulation can do and what it shows.** `FBWorld` has borrowed the registry since the lib/client split, so the data path is already there. |

The German inventory of the renderer round keeps the same finding in its own words —
[`renderer.md`](renderer.md), Gaps, item 1.

## Knowledge

Nothing derived yet. What the round will need is already documented elsewhere and is not repeated
here:

| Question | Where it is answered |
|---|---|
| Where does a unit pose come from, and when is it safe to read | [`../missions/runtime.md`](../missions/runtime.md) (snapshot barrier) |
| What entities exist and what state they carry | [`../missions/runtime.md`](../missions/runtime.md), [`../weapons.md`](../weapons.md) |
| Which effects are already published as unit data | [`../sim/sensors.md`](../sensors.md) (signature, chaff clouds) |
| Where the stage may draw and where it may not | [`renderer.md`](renderer.md) (pass topology as a contract) |
