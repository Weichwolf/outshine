# Native body-to-terrain contact · #1189

`BuildSurfaceEarthworks` consumes the closed `SurfaceBody` and one explicit
`SurfaceGroundSource` per qualified source index. It uses the actual indexed body
underside, including its configured thickness; it does not guess a ground elevation
from layer order, stair levels or the highest visible deck.

| Source effect | Height-field contribution |
|---|---|
| GroundContact | Cut/fill to the continuous body underside; explicit nonzero contact-group key |
| OverheadClearance | Cut-only ceiling at underside minus declared clearance; never fill |
| NoEarthwork | Explicitly leave the height field unchanged |
| Unspecified | Reject before publishing contacts |

An actual shared indexed face with a declared ground contact keeps its foundation.
Separate sheets at the same XY never become shared contacts. Conflicting ground keys
on a shared face require a producer decision; source order cannot choose a winner.
Each stamp retains its original cap-face index, which resolves back to source masks.
Apron width follows the existing road-contact default. Producers supply the relief
estimate used by the existing apron blender. Exact cap planes and existing planar
support handle contacts between terrain postings; no new global raster is introduced.

The converter visits sources, faces and present owner bits: O(S + F + ownership
incidences), with bounded output and cancellation. Each contributing face produces
one triangular stamp; coplanar merging is not implemented or claimed optimal.
The existing terrain deformation solver owns spatial sampling and blending costs.
An entirely detached input reserves no stamp payload.

```sh
make suite SUITE=outshine/integration/transport/SurfaceEarthworks JOBS=2
```

The modular native test builds actual terrain sheets and their triangle meshes.
Independent ground/overhead sheets, both stamp orders, low ground, ceiling cuts,
shared landings, explicit exclusions, invalid roles, budgets and cancellation are
checked without a renderer. Triangle-BVH samples verify the deformed mesh itself.
These are controlled synthetic DEM sheets, not a real-provider or runtime admission.

`NoEarthwork` does not create a tunnel opening. Terrain voids require a separate
geometry consumer; a height field cannot represent an enclosed tunnel. Source-role
production, native navigation, cache/runtime integration and actual world rendering
remain separate pending work in #1189.

`Osm::ReadSurfacePlacement` reads layer/bridge/tunnel facts from a raw `OsmField`
feature before display filtering. Missing flags stay optional; they are not supplied
negative facts. Invalid/contradictory flags are refused with a typed reason. The live
street producer uses the same reader; text and numeric declarations are supported.
A native planner can inspect tunnel facts even when the legacy display excludes it.
Ground/clearance recipes still need explicit source-role production; a nonzero OSM
layer alone establishes neither a bridge nor a ground contact. Prepared ground region
recipe92 invalidates products built with the earlier placement interpretation.
