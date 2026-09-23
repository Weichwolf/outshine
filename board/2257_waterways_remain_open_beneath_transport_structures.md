Type: defect
State: ready
Architecture: ready
Parent: 2175
Depends: 2173
Priority: P0
Area: generators, terrain, water, transport
Tags: osm, bridges, contacts, navigation

# Waterways remain open beneath transport structures

## Proven defect and boundary

`Corridors::YieldsOf` marks bridge ribbons `Fills=false`, but still sends their
entire span as a terrain stamp; `EarthworkPress::BidsLand` may cut its bed and banks.
`Corridors::PressesUnder` emits `Fills=true` for every shaped junction, including
one supported by a bridge deck. `EarthworkPress::PressesAt` applies an `EarthworkKind::Basin`
only when no land stamp covers the same point. Thus an elevated junction or
unrelated land feature can fill a waterway. `AppendLakeStamps` covers water
surfaces, while `WaterField::Course` has no matching bed protection in the
terrain press. An OSM `waterway=river` represented only as a course is exposed.

These paths prove a possible obstruction, not its frequency at a particular
place. A dark water texture, an open top view or `Fills=false` on the deck alone
does not establish a free channel. The logical route, 3D deck, water bed,
supports and collision must agree at the same version and coordinates.

## Construction contract

1. Produce a versioned waterway footprint/clearance envelope from `WaterField`
   surfaces and courses: channel width, bed/level, banks and required flow
   opening. Preserve source IDs and report uncertain inferred dimensions.
2. Separate transport deck, approach earthwork, abutment, pier foundation and
   tunnel opening as distinct construction requests. A free span submits no
   terrain request. Abutments and piers are bounded footprints, placed outside
   the protected channel unless an explicit clearance solution permits them.
3. Replace implicit `Basin` versus `LandHeld` precedence with typed local
   constraints: protect channel, excavate, fill and support. Resolve overlaps
   deterministically from feature IDs and construction priority; report
   infeasible footprints and residuals. A generic road or building stamp must
   not silently override a protected bed. Fords/culverts require explicit
   traversable and hydraulic construction, not an accidental embankment.
4. Derive render and contact geometry from the accepted alignment/support
   solution. Validate continuous lane/rail pose, width, grade/curvature,
   crossfall, deck thickness, headroom, collision and portal/abutment seams.
   An actor following the logical edge must remain on the constructed surface;
   changing render LOD must not change routing or contacts.

## Falsifiable acceptance

- Start with a flat analytic channel and one two-point bridge. At every sampled
  interior point and bank section, terrain after pressing equals the protected
  preconstruction bed within the declared numerical tolerance; a deliberate
  `Fills=true` span must fail. Sample between raster nodes, not just on them.
- Repeat with a bridge junction above the channel, a pier near the bank, a
  road/rail overpass, a ford/culvert and a building footprint overlapping water.
  Verify clearance and flow width in cross-section; reject an impossible
  support layout with source IDs rather than filling the river.
- Run a vehicle and a train where their modes apply across the generated
  alignment and its seams. Check wheel/rail contact, collision, legal turns,
  gradients and braking/curvature constraints separately from the image.
- Render above, below and along the bridge at two LODs and after tile eviction;
  inspect PNGs and contact profiles. Published revisions remain atomic.
  Measure CPU/GPU/scratch-memory budgets on the target hardware.

2175 owns transport alignment and supports; 2121 owns terrain constraints;
2145 owns channel bed/surface generation; 2133 owns logical connectivity.
This WI integrates and proves the shared crossing contract. Do not close it
with a single bridge-specific condition in `Corridors`. WI 2259 defines the
continuous and numerical proof obligations for its protected channel.
