Type: feature
State: ready
Architecture: ready
Parent: 2101
Depends: 2173
Priority: P0
Area: world, generators, simulation
Tags: osm, construction, provenance, determinism

# Every OSM feature receives an explicit construction result

## Contract

Implement a total, versioned interpretation of valid OSM primitives and tag
combinations. The useful function is
`Construct(OSM, DEM, context, rules, seed) -> ConstructionResult`, not a claim
that OSM alone describes the real building. The result is one of: constructed
physical products; an explicitly nonphysical semantic feature; a deferred
feature missing a declared dependency; or a diagnosed contradiction/invalid
input. A known physical feature must not silently vanish or become a flat
texture because its special case was not written. Unknown tags retain source
identity and may use only a conservative generic construction whose physical
contract is satisfied; otherwise diagnose them. Never invent a navigable road,
support or water obstruction from an administrative or metadata feature.

Products share stable source/structure IDs and a revision: logical topology,
3D alignment, terrain requests, supporting/enclosing structure, render mesh,
Metallic-Roughness material, collision/contact and interaction semantics.
Consumers derive LOD independently; no navigation from render triangles.
Uncertainty, inferred dimensions and the rule/seed used are explicit. WI 2259
requires a checkable geometric certificate before physical validity is claimed.

## Reality-to-OSM case matrix

The rows are composable construction families, not a closed tag whitelist.
Relations, open/closed ways, nodes, multipolygons, split tiles and conflicting
tags all enter the same interpreter. Absence of a tag is not evidence that a
support, portal or culvert is unnecessary.

| Real construction | Typical OSM evidence | Required product/constraint |
|---|---|---|
| Ground road, path, steps | `highway=*`, `surface`, `width`, `incline`, `steps` | graded and drained contact; mode-specific traversability |
| Bridge, viaduct, flyover | `bridge=*`, `layer`, connected ways | continuous deck, plausible visible contacts, free underpass and driving surface |
| Ford, culvert, causeway | crossing road/waterway, `ford=*`, `tunnel=culvert`, embankment | passable crossing and maintained hydraulic opening |
| Tunnel, covered road, underpass | `tunnel=*`, `layer`, covered/indoor hints | portal, excavation/void, enclosure, headroom and route continuity |
| Rail, tram, switch, level crossing | `railway=*`, gauge/tracks, crossing nodes | continuous rail pose, switch/road conflict rules and clearance |
| River, stream, canal, lake, coast | `waterway=*`, `natural=water`, water polygons/coastline | bed, banks, surface/flow corridor; no accidental fill |
| Building on slope or over passage | footprint/relation, levels, height, `building:part`, `min_height` | closed visible base, openings, access and stable floors |
| Quay, pier, retaining wall | `man_made=*`, embankment/cutting hints | supported edge, water/terrain contact, no universal earth skirt |
| Vegetation, rock, land cover | natural/landuse areas and point features | material/biome and instances with scale, exclusion and LOD |
| Address, boundary, route metadata | relations and nonphysical tags | semantic product only; optional map overlay, no collision |

For each physical row, add compound fixtures: on steep DEM, over/under water,
at a tile edge, with missing dimensions, with incompatible tags and with a
second feature competing for the same volume. Do not equate a crossing in XY
with a connected junction; OSM node/way/relation topology and `layer` constrain
the interpretation, while `layer` alone is not a metric elevation.

## Implementation order

1. Normalize OSM primitives, source IDs, units and topology once in the
   import/provider boundary. Preserve raw evidence and unknown tags. 2173 owns
   semantic transport through that boundary; 2133 owns logical connectivity.
2. Dispatch by composable roles: route/watercourse/footprint, ground/deck/void,
   surface/enclosure/support. Use a bounded local construction/constraint solve,
   not one handwritten class per possible tag combination. Reuse native math,
   material and geometry contracts; no glTF-shaped runtime model.
3. Resolve shared terrain, water, structure and route constraints before
   meshing. 2121 owns earthwork contacts, 2145 water, 2175 transport alignment,
   2257 protected water crossings. Report unresolved conflicts with feature IDs
   and residuals; do not silently choose whichever stamp happens to run last.
4. Publish the whole product set atomically with revision, ownership and
   bounded streaming jobs. Unknown or incomplete constructs remain visible in
   diagnostics and map semantics; do not claim physical or driving validity.

## Acceptance

- An inventory maps every supported OSM class and combination to its rule,
  products, assumptions and tests. Every ingested primitive ends in exactly
  one explicit result state; zero silently dropped physical features.
- Analytical fixtures for the table and pairwise conflicts check geometry,
  material, ground/water contact, free-space clearance, logical route and
  collision. Negative controls fail for a filled river, closed tunnel, floating
  road, wrong-level turn and unsupported building. Vary source order and LOD.
- Vehicles/trains/people traverse only routes whose constructed alignment and
  contact are valid. Measure grade, curvature, width, clearance and seams over
  motion; an attractive still image is not a functionality proof.
- Property/fuzz tests vary valid tag combinations and missing attributes within
  declared bounds. All terminate within budgets and yield a typed result;
  no claim of reconstructing the unique real object from incomplete OSM.
