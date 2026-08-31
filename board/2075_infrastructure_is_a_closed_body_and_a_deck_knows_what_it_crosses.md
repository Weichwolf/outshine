Type: feature
State: active
Area: world, generators
Tags: infrastructure, osm, measured, picture

# Infrastructure is a CLOSED BODY and a deck knows what it crosses

**GOAL.** Infrastructure is geometry held to a building's standard: every road, bridge, ramp, tunnel
portal and retaining wall is a CLOSED body whose vertices are snapped to a shared grid, so two
bodies that meet SHARE their corners rather than approaching them, and no body passes through
another -- a ramp meets a carriageway at a joint. The one intersection allowed is a body dipping
into the ground tile, because a terrain mesh coarser than the structure makes that unavoidable.
Where OSM states a fact it decides; where OSM is silent -- the usual case -- the missing number is
INFERRED from what is structurally plausible and the inference carries its rule, declared in a table
with origins the way `ground-materials.json` declares an albedo, never coded into a generator's
body. And it is built at a level of detail a frame can carry: one body per tile is the wrong unit,
so an infrastructure body is cut into sub-tile pieces that stream and cull on their own, the way
RAGE's map sectors and Unreal's HLODs do.

**Benchmark** — Unreal: a road is a Landscape Spline carrying Spline Meshes, and the landscape is
deformed to meet it; the spline holds width, and the mesh holds the cross-section. RAGE: roads and
their structures are authored map geometry in sectors that stream independently. **They agree that a
road is geometry with a cross-section and that the unit of streaming is smaller than a road.** Where
they differ is authoring: RAGE's is drawn by hand, Unreal's is a spline an artist lays. **Neither
faces this problem**, because neither builds its roads from a public map at run time -- so the
inference below is this tree's own and the reason is written where each number is.

## What the data actually holds, measured

| tag | present | measured on |
|---|---|---|
| `bridge` | **yes**, numeric | 349 ways at Heidelberg, 1020 at Koehlbrand |
| `tunnel` | **yes**, numeric | 183 features at Heidelberg |
| `layer` | **NO** -- absent as a number AND as a string | 0 of 25 763 ways at Koehlbrand |
| `lanes`, `width` | through the rule table, per KIND rather than per way | `vegetation.json` |

**`layer` is not in the vector tiles.** That is the one tag that would have said which way passes
over which, and it is not there. So an overpass has to be inferred from the plan crossing plus
`bridge`, and the inference is the item's substance rather than a detail of it.

## What was built and what it proved

A road is a swept solid: plan, profile, GRADE and sweep as four separable steps, `RoadMesh` beside
`BuildingMesh` rather than a loop inside `Picturing`. Three cross-sections, chosen from what the
rule table already declares -- unsealed gets shoulders that dip into the ground, sealed with two
lanes gets kerbs 0.14 m proud, sealed without gets a plain band, all three with the standard 2.5 per
cent crossfall.

**And the first grade rule was measured WRONG.** A deck was made to run straight between its own two
ends. That is right for a bridge whose ends sit on the ground and it produces NOTHING at the
Koehlbrandbruecke: OSM splits the structure, the tagged way is the main span alone, and its ends are
already 50 m up in reality while our terrain has them at 5. **Looked at: the frame that should be
dominated by a 55 m span shows every road lying flat.**

So a deck's height is NOT derivable from the deck. It comes from what the deck CROSSES and from the
ramps that reach it, and both are questions about a NETWORK.

## What will be true

- [ ] `RoadHarvest::Reap` is reached. It reads `bridge`, `tunnel` and `layer` and builds a
      `Path::Network` and has no caller today, which is exactly the graph the rest of this needs
- [ ] A crossing is DETECTED: where two ways cross in plan and one is a bridge and the other is not,
      the bridge is above. Where both or neither, they meet at grade and that is a junction
- [ ] A deck clears what it crosses by a DECLARED clearance -- 4.50 m over a road (lichte Hoehe,
      RAS-Q), 5.70 m over rail, a navigable water's own figure over a fairway -- each with its origin
- [ ] A ramp reaching a deck climbs at no more than its class's `MaxGradient`, which `Rule` already
      carries and nothing reads, and its curvature respects `MinRadiusM`, likewise
- [ ] Bodies are CLOSED and their vertices SNAP: two ways that share a node share their corner
      vertices, and no two bodies interpenetrate. A body may dip into the ground tile
- [ ] The unit of geometry is a SUB-TILE piece, not a way and not a tile, so a piece outside the
      frame costs nothing
- [ ] Measurement that shows this is wrong: the Koehlbrandbruecke's deck height above the water at
      its centre span. It reads ~0 m today and the structure is 55 m. And Kaiserberg's four levels
      must stand at four heights, which a plan view cannot show and an oblique one can

## What this does NOT cover

Rail. The tiles carry `rail`, `tram`, `subway` and `light_rail` as street kinds with widths, so they
sweep as roads today; a track bed, ballast and catenary are their own item.
