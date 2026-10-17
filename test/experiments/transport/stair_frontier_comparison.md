# Bounded original-source discovery and actual interior roots (#537)

The experiment discovers alternative source modules from the unchanged original
Wien provider input and actual cached interior profiles. It preserves the
verified 43-source parent from #535. The reusable discrete operation is
[`transport.frontiers`](../../../lib/frontiers.md); automatic admission and
independent replay are `stair_frontier_experiment.py` and
`stair_frontier_verify.py`. `stair_frontier_compare.py` compares indexed lookup
with complete node scanning on cached Zürich, Basel and Midtown Manhattan data.

Original sources and semantic reasons remain separate from proposals and actual
geometry acceptance. The current ordinary-ground adapter excludes indoor,
non-ground levels, bridge/tunnel recipes and unsupported profiles. A root is an
explicit original vertex in an actual saved parent road, with unchanged original
axis, actual three-point cross section and covered whole profile. Provider-node
coincidence alone is not an exact OSM topology claim.

New endpoint and through profiles share one complete root hull with the frozen
old cross section. Whole actual source surfaces, junctions, bodies, Double and
Float32 profile contacts, terrain boundaries/fields and all old pier contacts
remain mandatory. Optional per-source overhead checks use actual lower facets,
including conservative whole borrowed old-cap facets, and whole bridge facets:
foot/cycle require 2.5 m, the current minor-road profile requires 4.5 m. The old
strict bridge-overlap exclusion remains the default family.

The first prototype `177fe724a` generated a locally valid mesh but its independent
saved-product reader rejected partial root pads. It is not a passed acceptance.
`bind_root_pad` now includes frozen, through and branching profiles in the same
whole hull. A three-profile counterexample protects that correction. Proposals
that fail geometry retain the old valid world and an explicit reason; no
boundary epsilon, terrain/pier contact or solver budget was relaxed.

SIGGRAPH starting point: Chen et al., *Interactive Procedural Street Modeling*
(2008), page 5 figures 7–8 and section 6.1, inspected visually. Separating
graph/seed topology from geometric construction is useful here. This is an
original bounded discrete provider-source traversal, not their tensor-field
generator. No SDK code or dependency is used.

Current scope is one automatically selected module of at most eight whole
sources, at most eight candidate roots, at most three actual geometry attempts.
Alternative proposals may overlap. Original input limits and all older body,
source-owner, parent-cache, solver and contact limits remain unchanged. Complete
worldwide source coverage, global module/cache graphs, reservations/LOD products,
all route/vehicle dynamics, complete road-generator equivalence and native
integration/device acceptance remain #434/#50/#48.

Measured frozen-product, installed-package, image and cost results are recorded
with their exact generation/reader commits below after successful acceptance.
