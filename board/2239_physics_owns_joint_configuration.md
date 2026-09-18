Type: refactor
State: active
Architecture: ready
Parent: 2139
Depends:
Priority: P0
Area: actor, scenario, include, test
Tags: api, physics, modules

# Physics owns joint configuration

## Evidence

`src/actor/body/Prismatic.h` includes the whole scenario document and aliases
`Scenario::Prismatic`. That makes the physics tier depend transitively on world, audio and render
configuration merely to evaluate one spring/damper. The alias preserves importer ownership inside
runtime physics and defeats the native-configuration boundary.

## Decision

Add a small native physics joint descriptor under `include/physics/` or the actor public boundary.
Use names with units for reach, travel, stiffness, damping, stop stiffness and load limit. Physics
operations consume this type directly. `Scenario::Document` composes it when serialization
semantics match; otherwise scenario import converts once before publication. Delete the alias and
the actor-to-scenario include. Do not duplicate `Press` or retain a compatibility type.

The descriptor is scalar, trivially copyable and independently validatable. Validation rejects
nonfinite values and physically impossible negative magnitudes before simulation state mutates.
Keep `Reaction` and `Approach` as runtime results, outside scenario.

## Acceptance

- [ ] Actor sources and public physics headers contain no scenario include or `Scenario::` type.
- [ ] Direct and XML declarations reach the same native descriptor and validation errors.
- [ ] Analytic zero, elastic, travel-stop, damping and load-limit cases remain covered.
- [ ] The exact WI 2239 layer findings are removed; `make lint` passes.
