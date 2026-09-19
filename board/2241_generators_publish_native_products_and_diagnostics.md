Type: refactor
State: active
Architecture: ready
Parent: 2139
Depends:
Priority: P0
Area: generators, scenario, diagnostics, include, test
Tags: api, generators, modules

# Generators publish native products and diagnostics

## Evidence

`generators/road/Corridors.h` includes `scenario/Event.h` for generic metric rows. `Shipped` and
road code also inherit engine/scenario/audio/render public dependencies through world headers.
Generator products therefore expose composition-layer types even though generators must remain a
standalone library below engine and scenario.

## Decision

Put structured metric samples in the diagnostics/base boundary with owned name, value and unit.
Corridor generation returns native geometry, navigation yields and those metric samples. Scenario
events translate or forward diagnostics above the generator boundary; generators never include a
scenario declaration. Remove transitive engine dependencies by consuming the world and detail
contracts established in WI 2240, then inspect every generator public/internal header for the same
direction error. Do not hide violations by widening generator `reaches`.

`Shipping` remains composition of concrete generator implementations. It may depend on generator,
world and content contracts, never Engine, Scenario, Audio or renderer execution configuration.

## Acceptance

- [ ] Generator sources contain no engine facade, scenario or audio public dependency.
- [ ] Corridor geometry, navigation yields and metrics retain their values and ownership.
- [ ] The generators archive still links and operates without engine/runtime objects.
- [ ] Exact WI 2241 layer findings are removed; generator and corridor suites plus `make lint` pass.
