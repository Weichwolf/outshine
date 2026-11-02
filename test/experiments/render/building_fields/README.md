# Building-field experiments

Native capture, metadata, cache and pose prototypes. No scenario consumer.
Types live in `outshine::Experiments`; CMake target: `test_experiments_building_fields`.
The four native studies share `test_harness_building_source_capture`.

Run the retained cases with
`make suite SUITE=outshine/integration/experiments/building_fields`.
Study inputs and commands: [native fields](../native_building_source_fields.md),
[impostors](../native_building_impostors.md).
Runtime separation: [#1937](https://github.com/Weichwolf/outshine/issues/1937).
