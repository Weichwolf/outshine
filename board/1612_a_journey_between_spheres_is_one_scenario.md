Type: feature
State: open
Parent: 1611
Area: core
Tags: scope

# A journey between spheres is one scenario

**Benchmark** — Unreal: one `UWorld` per level and travel between them; large worlds via World Partition and origin rebasing. RAGE: one map. **Taking Unreal** — origin rebasing is the mechanism, and it is the same sentence as this tree's one pre-view translation.

The proof of generality: declare a constant-1g-thrust rocket, board it on Earth, fly to the
Moon in hours, disembark, move under local conditions. (Check: 1g brachistochrone Earth->Moon
is `2*sqrt(3.844e8 / 9.81)` = 3.5 h.) The architecture already carries the pieces — the rocket
is a BODY with one actuator; boarding is a POSSESSION RELINK on the same `DrivenBy` relation
the player and the autopilot share; each sphere's sky is that sphere's declaration; presence
lets the departure sphere dissolve to field while the arrival sphere materialises.

## What will be true

- [ ] A scenario declares a SYSTEM of spheres, each with radius, gravity, providers and sky —
      one scenario, several worlds, and the library knows none of them by name.
- [ ] Gravity is a queryable field of the declared system (dominant-sphere rule, patched rather
      than n-body, until a measurement demands more).
- [ ] Free flight is a physics regime: a body with no surface contact integrates thrust and
      gravity, and GROUND is absent rather than empty.
- [ ] Proving test: one scenario, two spheres, one mind, two possessions — and the high jump on
      the smaller sphere is measured, never coded.
