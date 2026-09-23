Type: feature
State: ready
Architecture: ready
Parent: 2258
Depends: 2173
Priority: P1
Area: generators, simulation, testing
Tags: geometry, validation, numerics, plausibility

# Construction products carry checkable functional and visual evidence

## Scope

The target is a plausible, usable and appealing generated world, not a proof
of the real object's dimensions or structural capacity. OSM/DEM often admit
several defensible constructions. Record the chosen rule, inferred parameters,
source IDs and uncertainty. Never label an unknown as a measured real value.

Use hard geometric checks where a violated condition breaks function or is
visibly impossible: route continuity, vehicle envelope, free passage, protected
water, terrain contact, mesh validity and cross-tile seams. Choose explicit
tolerances from actor size, source resolution and numerical precision. An
unresolved hard condition yields a diagnosed construction result, not a
`drivable` or `collidable` claim. Material/style/shape plausibility needs
image and motion review; an analytic check cannot certify its appearance.

Each `ConstructionResult::Built` carries a compact validation record: input
revision, assumptions, measured minima/maxima, tolerances and pass/fail for
the contracts relevant to that product. Do not attach irrelevant certificates
to semantic-only features. The logical AI/NPC/map graph stays independent of
render LOD; stable route IDs refer to a validated 3D alignment and contact.

## Checks by construction family

| Family | Functional check | Visual/motion check |
|---|---|---|
| Shared frame/tiles | common positions and seam normals within derived metre/radian tolerances | crossing a streamed tile seam has no visible pop or contact jump |
| Road/lane/rail | continuous alignment; bounded width, grade, curvature and contact for stated actor | camera and vehicle traverse without floating, sinking or wrong-level turns |
| Bridge/underpass | deck and support avoid required free volume; actor envelope clears | deck, underside, supports and approaches read as one credible structure |
| Tunnel | connected void, portal and headroom along route | entrance, lighting and terrain enclosure remain convincing in motion |
| Terrain/water | protected river channel is not filled; earthwork joins intended structure | banks, cuts and embankments have plausible slopes and materials |
| Building/support | finite closed required solids, usable openings and ground contact | base, façade, roof and access look coherent from several distances |
| Mesh/material | finite vertices, valid indices/orientation and Metallic-Roughness ranges/colorspaces | shading, scale and texture frequency survive lighting/time variants |

Analytic extrema are useful for simple curves, planes and swept envelopes.
Use bounded subdivision or robust predicates where sampling can miss a
functional gap; use sampled motion and image review where perceptual quality
is the actual requirement. State the resolution and residual uncertainty.
Tests include deliberate mutations for each hard check. Do not reject a
visually plausible solution solely because an unobservable detail differs
from the real object or an arbitrary prefab.

## First slice and acceptance

Build a two-point bridge over an analytic river in 2257. Protect the channel
from positive terrain fill, check a stated vehicle envelope under/over the
deck as applicable, and trace continuous road contact through both abutments.
Deliberately fill the channel, close the passage and introduce a road gap;
each must fail its relevant check. Compare one-shot and sliced products, then
apply the same checks to a real OSM/DEM crossing. Open above/below PNGs and
drive a camera through the transition; note appearance defects separately
from functional failures. Report work/memory cost, `make format`, focused
tests and `make lint`. Vehicle and train envelopes set movement constraints;
building statics are outside scope.
