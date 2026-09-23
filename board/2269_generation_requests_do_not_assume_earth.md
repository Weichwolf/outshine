Type: design
State: proposed
Architecture: ready
Parent: 2188
Depends: 2268
Priority: P1
Area: include, scenario, generators
Tags: api, groundless, coordinates

# Generation requests do not assume Earth

## Defect

`include/generation/Generate.h::Request` gives every generator WGS84
`LatitudeDeg`, `LongitudeDeg` and `ExtentM`, and declaration fills them from
`scenario.Ground.Origin` even when Ground is absent. A space shooter or 2.5D
scenario can render native geometry and run simulation, but its generator
receives invented geodetic coordinates at 0/0. Null `Ground` alone does not
make this a generic generation contract.

## Decision

Use an explicit generation-space value at the public boundary: no spatial
window, a local native-space volume, or an Earth geodetic window. A generator
that needs a window checks the declared variant and returns a clear error for
an unsupported one. Keep seed, detail request, settings and borrowed terrain
sampler independent. Terrain sampling is available only with an Earth window
and an actual open source; absence is not zero elevation. Importers and local
generators build the same native Geometry, with no ECEF/ENU vocabulary in the
generic asset contract. Ground/world conversion stays in the Earth adapter.

The scenario declares the generator's space explicitly where the producer
needs one. A generator needing no window may use the no-window variant. Do
not infer a local region from the current camera or an Earth location from
default numbers. Migrate built-in generators by capability, preserving their
existing geographic behavior when Earth is declared. Do not change the
simulation coordinate frame or imported glTF placements as a side effect.

## Acceptance

1. Public probe receives no geodetic window for a groundless declaration;
   local volume values roundtrip through the scenario and producer. An Earth
   generator receives its declared geographic region and refuses a local-only
   request clearly. No generator receives invented lat/lon at 0/0.
2. A groundless native-geometry space fixture and a 2.5D fixture assemble,
   advance and render without terrain work; an Earth fixture still renders.
   Failures retain the previous published declaration. Check ownership and
   frame-time cost, run `make format`, focused suites and `make lint`.
