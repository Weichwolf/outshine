Type: feature
State: ready
Architecture: ready
Parent: 2258
Depends: 2173
Priority: P0
Area: generators, simulation, testing
Tags: geometry, proof, numerics, determinism

# Generators publish checkable geometric certificates

## Scope of proof

For each physical construction family in 2258, state a mathematical model,
assumptions, admissible input domain, numerical representation and output
invariants before treating its mesh as correct. A theorem about this model is
conditional; it cannot prove which bridge, foundation or material exists in
reality when OSM omits those facts. Artistic plausibility needs visual review.
Structural load capacity is out of scope; closed surfaces, convincing visible
contacts and functional route geometry are in scope. Do not infer required
support size from appearance: slender reinforced concrete can be credible.

Each `ConstructionResult::Built` carries source IDs, inferred parameters,
solver residuals, numeric error bounds and a validation record. Consumers may
mark a route drivable or a structure collidable only if its relevant certificate
passes. Rejection preserves logical OSM identity with a precise cause. A generic
fallback has the same obligations as a specialized builder.
The logical graph for AI/NPC/navigation/minimap stays independent of visible
meshes. Its stable edge IDs connect to a certified 3D alignment; a render LOD
or tile eviction cannot create or delete a legal turn. The contact mesh must
remain within a declared error of that alignment during traversal.

## Proof obligations

| Family | Analytic claim | Independent check |
|---|---|---|
| Shared frame/tiles | identical world position and seam data at common IDs; bounded float projection error | double-coordinate oracle, two tile orders and camera shifts |
| Alignment/lane/rail | continuous position/tangent where declared, bounded grade, curvature, crossfall and width for a stated actor class | analytic line/arc/spiral derivatives plus interval extrema, moving contact trace |
| Bridge/underpass | deck support footprint and required free volume are disjoint; deck thickness and clearance exceed stated minima | cross-section interval bounds, exact/robust overlap predicates, negative filled-channel case |
| Tunnel | route lies inside a connected void with valid portal and headroom | volume/portal intersection and collision traversal |
| Terrain/water | protected channel bed never rises; cut/fill stays within declared earthwork bounds; connected water surface/bed obey level constraints | pointwise inequalities including cell interiors and tile seams |
| Building/support | floor, visible base and openings meet without gaps or false contact | mesh closure and contact/clearance predicates |
| Mesh/material | finite vertices, valid indices/orientation, closed solids where required, BRDF parameter and color-space contracts | independent mesh validator and material oracle |

Derive continuous-extremum bounds where possible. Sampling pixels or raster
nodes alone cannot prove inequalities between samples. Use interval arithmetic
or robust predicates for bounded nonlinear cases; subdivide until a declared
error bound is met or reject within a work budget. Explicitly separate exact
analytic claims, interval-certified approximations and empirical checks.
Tests must include a mutation/negative control for each invariant; green output
alone is not a proof. Numerical tolerance carries units, derives from input
resolution and floating-point error, and is not loosened to hide failures.

## First vertical slice

Prove a two-point bridge over an analytic river in 2257: derive the protected
channel polygon and deck/support volumes, certify zero positive terrain delta
in the channel, minimum vertical clearance and continuous road contact at both
abutments. Make the deliberate fill/closed-deck variants fail. Compare native
products from one-shot and sliced builds across several slice sizes, then run
the same predicates on a real OSM/DEM crossing and inspect above/below PNGs.
Publish no `drivable` claim if a required predicate or numeric bound is unknown.
Vehicle and train models, not building statics, set the movement constraints.
