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

---

**Learned from the reference study (2026-08-22, primary sources; KSP config, UE docs, re3
decompile, IEEE 1516/1278.1, LWR/Burghout).** The design above holds, with four sharpenings:

1. **Two axes, not one ladder.** KSP separates EXISTENCE (loaded/unloaded) from FIDELITY
   (packed/unpacked) with an ordering constraint between the four thresholds -- hysteresis by
   construction. Our ladder becomes two coupled axes with the constraint pair enforced
   `static_assert`-style; and the currency stays the ERROR LADDER, never metre tables (KSP's
   Physics.cfg is the counterexample: a distance table that misorders FLYING unload < pack and
   deletes wingmen).
2. **Collapse evaluates, it never creates.** Elite/NMS/Minecraft: the concrete is a pure
   function of (coordinates, seed) -- our (kind, params, seed, rung) key is already the right
   shape. Proven breakage modes go into the rules: the GENERATOR VERSION belongs in the key
   (seeds break per algorithm change), generation holds integer/fixed-point (float
   non-associativity breaks cross-platform identity), and the collapse may not consult
   neighbours (order dependence) -- our "no neighbour part" rule, independently rediscovered.
3. **The field side is real engineering, not hand-waving.** LWR conservation with ONE shared
   fundamental diagram on both sides of the seam (Burghout: divergent q(rho) makes phantom
   shocks), emission via flux accumulator carrying the fractional remainder (else vehicles
   leak), congestion pushing back upstream across the seam (else the boundary is an unphysical
   sink). Conservation is bookkeeping, not hope.
4. **Sensors operate on the abstract layer -- materialisation is never transitive.** Sharper
   than my "one rung down": Dwarf Fortress fights its wars abstractly; UE's cost is objects x
   viewpoints. An embodied mind's perception READS the field/rails layer; only declared
   instruments (player, radar) force collapse. And the ANONYMOUS has no per-entity abstract
   propagation at all -- it is pure field (the GTA model); the costed counterexample is GTA IV's
   cancelled everyone-has-a-home, killed by bug volume.

Transitions are a named state with EASING (KSP 1.2's physics easing; DIS convergence blending
instead of snapping; asymmetric on/off-screen hysteresis from GTA; Dwarf Fortress makes the
abstraction VISIBLE so movement reads as intent, not popping). Star Citizen serves as the
negative proof: the quanta->NPC promotion is the part that never shipped.

The cross-finding over all seven references: concrete state is everywhere a CACHE of an
abstract function, valid while divergence is bounded -- and every documented failure is a SEAM
failure, never a failure of the abstraction itself. The seams get the tests.
