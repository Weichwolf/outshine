Type: feature
State: open
Parent: 1583
Area: core
Tags: scope, scene, perf
Supersedes: 1475

# Presence is a rung, and a measurement materialises

**Benchmark** — Unreal: an actor exists or it does not; HLOD stands in for it at range. RAGE: entities have LOD states including a dummy. **Both agree** — presence is a RUNG rather than a boolean, and the coarse rung is what the horizon is made of.

An engine of this size cannot compute everything always. Determination happens AT MEASUREMENT:
what is seen, radared, probed or perceived is computed and fixed; the rest is a conserving
field. The engine lives half of it already — tiles stream on demand, generators produce from
(kind, params, seed, budget), nothing draws what no camera asks for. What is new is extending
the same shape to PHYSICS and MINDS, and recognising that measurement is more general than
rendering.

| rung | what exists | advanced by | cost |
|---|---|---|---|
| `field` | a statistical quantity per tile edge — cars per metre-second, walkers per platform | conservation, closed form | O(tiles) |
| `rails` | an individual with analytic state along its corridor, no contacts, no mind tick | closed-form advance from (seed, entry state, corridor, t) | O(individuals) |
| `body` | the full actor chain — rig, contacts, integrated forces, a ticking mind | physics and mind, per frame | the real thing |

Materialisation is a PURE EVALUATION and never transitive: minds read the abstract layer, only
declared instruments collapse it.

## What will be true

- [ ] The two thresholds (existence, fidelity) are ordered and quantised on the same ladder the
      budget uses — never a distance ratio.
- [ ] A tick is a declared RATE, not a frame: an actor states how often it wants to run and the
      scheduler answers how often it did. The number that ticks in one frame is BOUNDED and the
      bound is a number somebody chose.
- [ ] An actor that did not tick is not an actor that stopped: it carries `dt` since it last ran.
- [ ] The schedule is DETERMINISTIC — two runs of one scenario tick the same actors in the same
      order.
- [ ] Crossing a rung conserves: the population materialised equals the population the field
      carried, and the ledger says so.
