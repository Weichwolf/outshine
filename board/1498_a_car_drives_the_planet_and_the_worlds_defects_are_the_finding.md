Type: feature
State: active
Area: world
Tags: instrument, perf
Supersedes: 1501, 1502, 1504, 1518, 1524

# A car drives the planet, and the world's defects are the finding

**Benchmark** — Unreal: a level is authored, so a defect in it is a content bug. RAGE: the same. **Neither faces this** — OSM is a source of shape rather than a specification, so a corner the graph demands is a finding only where it is implausible or geometrically wrong.

The engine has no instrument for its WORLD. The render corpus decides subjects against an
oracle, the frame suite decides cost, and nothing decides whether the thing built out of OSM is
a place a body can move through. **A car on a road is that instrument, and it needs nobody to
look at it**: *did the wheels stay on the surface* is a number per millisecond.

Two failure modes and neither subsumes the other. **Having to swerve is already the defect** —
the deviation from the ideal line is continuous and finds a road that is BAD long before one
that is impossible; 40 cm of forced correction is a finding. **And the crash is still needed**,
because the worst defects produce no deviation at all: a tunnel nobody recognised gives a road
that climbs the mountain instead of going through it — laterally perfect, zero deviation, and a
40 % gradient no vehicle can take.

The vehicle is the sensor and the road is the subject: four wheels with suspension travel, a
load per wheel, longitudinal and lateral tyre forces. The crash is geometrically free — a body
moves only by forces, so a wheel in free fall is a normal force of zero and an impossible
gradient is a longitudinal force the tyres cannot deliver. The threshold is DERIVED, not chosen:
the link's own limit says what counts, the tick rate says what can be seen, and both are
published beside every finding.

## What will be true

- [ ] **The synthetic road is driven first** and its telemetry is the instrument FLOOR, quoted
      beside every count; no defect smaller than the floor is reported.
- [ ] A route is a SEED — two endpoints and the path derived from them — so a run is named by
      one number and re-driven exactly.
- [ ] **The headless run links no renderer at all**: world, corridors and physics, nothing else.
      That is what makes it fast and it is the first real test of the claim that this engine is
      a library rather than a thing welded to a GPU. How much faster than real time is published.
- [ ] A finding is CLASSIFIED, never counted: discontinuity, gap, gradient, curvature,
      connectivity, overlap — each has a different owner and a different repair.
- [ ] **A hundred real routes worldwide, each declaring the STRATUM it is the hard case for**
      (untagged structure, stacked junction, hairpin geometry, ...), its data pin, its vehicle
      and its expected refusals — a route that is in the set because it was convenient teaches
      nothing and dilutes every rate computed over the set.
- [ ] The cheap tiers run first, because a million parts cannot be looked at: unspellable (free,
      board:1499), metamorphic relations (O(n), no oracle — tile independence, order
      independence, axis swaps), geometric invariants (manifold, support, interpenetration,
      clearance, gradient), then the drive, then held-out tags, then eyes.
