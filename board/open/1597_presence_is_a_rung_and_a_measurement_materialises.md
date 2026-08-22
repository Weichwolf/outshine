Type: feature
Parent: 1583
Area: core
Tags: scope, scene

**Presence is a rung, and a measurement materialises**

The owner's principle (2026-08-22): an engine of this size cannot compute everything always.
Determination happens AT MEASUREMENT -- only what is seen or measured (the picture, a radar, a
collision probe, an AI's perception) is computed and fixed. The player materialises reality not
only graphically but physically and for the actors. Schroedinger as an engineering rule.

## The derivation into this architecture

The engine already lives half of this: tiles stream on demand, generators produce from
(kind, params, seed, budget), the budget is a screen-space-error ladder, and nothing draws what
no camera asks for. What is NEW is extending the same shape to PHYSICS and MINDS -- and
recognising that **measurement is more general than rendering**: a radar that sweeps must
determine positions it never draws.

**The presence ladder** (one ladder, quantised like the budget ladder):

| rung | what exists | advanced by | cost |
|---|---|---|---|
| `field` | a statistical quantity per tile edge -- cars per metre-second on a road, walkers per platform, no individuals | conservation laws (flow in = flow out), closed form | O(tiles) |
| `rails` | an individual with analytic state -- position(t), velocity(t) along its corridor, no contacts, no mind tick | closed-form advance from (seed, entry state, corridor, t) | O(individuals), no integration |
| `body` | the full actor chain -- rig, contacts, integrated forces, a ticking mind possessing the seam | the physics and the mind, per frame | the real thing |

**Measurement is a declared query with a rung demand.** The picture demands `rails` (visible,
placed) or `body` (near, shadow-casting); a radar demands `rails` (position, velocity); a
collision probe demands `body`; an AI's sight cone is a measurement like any other. The engine
holds the SET of standing measurements and keeps every individual at the CHEAPEST rung that
satisfies all of them -- degrade on detail, refuse on existence, hysteresis like the streaming
budget so the boundary does not flicker.

**Collapse is a pure function, or observation leaks.** field -> rails materialisation is
(field state, tile seed, place, time) -> individuals, deterministic: measuring twice yields the
same cars in the same order; two observers agree. rails -> field dissolution conserves the
counted quantities (a car that entered the tunnel observed exits it consistent -- count and
flow are the invariants, audited per tile like cut/fill is audited per metre).

**Second-order observation is bounded.** A materialised mind measures too (its perception cone
is a query). Unbounded, one observer cascades the world awake. The rule: a measurement made by
an entity at rung R demands at most rung R-1 of what it measures -- an embodied NPC sees rails
neighbours, a rails individual sees only the field. The player (and any declared instrument)
measures at full demand. This is the boundedness clause the frame path requires.

**Where it lands in the classes:**

- the SCENE STORE: presence as a component on the entity (its current rung + the standing
  demands against it); promotion/demotion are store operations a system runs, not scattered ifs
- the FIELDS: the `field` rung IS a field beside height/class/water -- per-tile, streamed,
  conserving; traffic density comes from the same OSM edges the corridor reads
- MEASUREMENT declarations: the camera's frustum, a radar's sweep, a probe's radius -- each a
  declared volume + rung demand, registered like a Writes row, queryable, published in telemetry
- the DRIVER: NPC traffic is the first consumer -- board:1573's missing-traffic finding becomes
  "the traffic field exists everywhere; cars materialise where measured"

## References (study running; confirmed ones move up)

KSP on-rails physics bubble · Unreal Significance Manager + MassEntity simulation LOD ·
RAGE/GTA ambient population (statistical zones, spawn radii) · Star Citizen Quantum ·
The Sims / RimWorld off-screen abstraction · interest management / DDM (HLA) as the formal
frame · macroscopic traffic models (LWR) for the field's conservation law.

## What must become true (slices)

- [ ] the presence rung stands in the store (component + promotion/demotion ops, hysteresis)
- [ ] a measurement is a declared, registered query volume with a rung demand
- [ ] collapse determinism proven: measure-twice-agree, by test, with the seed spelled
- [ ] conservation proven: field->rails->field round trip keeps count and flow per tile
- [ ] the cascade bound holds: an embodied mind's measurement demands at most rails
- [ ] first consumer: ambient traffic in the driver -- the field everywhere, cars where measured
