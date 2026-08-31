Type: feature
State: active
Area: world, generators
Tags: infrastructure, osm, measured, picture

**Filed as 2075 in error and renumbered to 2082.** 2075 had already been ISSUED and closed, and this
tree's rule is that a number is issued ONCE and never again -- taking it from the DIRECTORY, which
remembers only what is open, reuses the number of something closed and two things then share an
identity for good. `ACommitSayingClosedRemovedTheItem` caught it: 31 closures announced since the
claim was written, one of them still standing on the board.

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

**`layer` is not in the vector tiles** -- and the sentence that followed it here was WRONG and is
corrected. I wrote that OSM does not carry it. **OSM carries it on 94.3 per cent of the 7 111 049
ways tagged `bridge=yes`**; what does not carry it is the SHORTBREAD schema that
`https://tiles.versatiles.org/tiles/osm/` implements, which exposes `bridge` and `tunnel` as
booleans and drops the ordering. Only 0.03 per cent carry a `height`, so a deck's height was never
in OSM to begin with -- but which way passes over which was, and this tree throws it away at the
tile.

That changes the shape of the answer: the ordering is a SOURCE question with a known answer, and
only the height is an inference. Wilkie, Sewall and Lin (IEEE TVCG 2012) publish a greedy method for
over- and underpasses that works WITHOUT `layer`, which is what to use until the source carries it.

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

## A DECK STANDS AT ITS CLEARANCE AND A RAMP CLIMBS TO IT

The chain is built and each link is measured on a place chosen to show it.

**The crossing.** `Path::Network` is reached at last -- it lays every ribbon way, sweeps for
crossings, and already refuses to JOIN one where either side spans. That same judgement, drawn to a
different end, IS the overpass: a crossing it leaves alone is one way passing over another.

    Heidelberg   11 873 crossings found,   242 decks raised
    Kaiserberg   18 555 crossings found,   718 decks raised

**The clearance.** It is DECLARED per road kind in `vegetation.json` beside the width and the lane
count that were already there, with its origin in the file's own `origins` block -- 4.50 m over a
road is the German carriageway clearance (RAS-Q, RIL 800), 5.70 m over an electrified standard-gauge
railway is the structure gauge to the catenary. **`make` deletes comments in `src/`, so a table of
origins cannot live there at all**; the tree's own rule forces the declaration into the asset file,
which is where the goal says it belongs.

    the most a deck stands over what it crosses: 6.70 m = 5.70 rail + 1.00 road-above

**The ramp.** A deck at its clearance with the ways either side on the ground is a cliff -- looked
at, at Kaiserberg, and it is exactly what the first raised deck showed. `maxGradient` is declared
for 16 of 29 road kinds and was read by NOTHING: motorway 4.5 per cent, primary 5.5, residential 10,
track 15, the German design figures. A relaxation over way ENDPOINTS pins a deck's two ends and
pulls every neighbour up until no way exceeds its own class's gradient. Heights only rise, so a
pinned deck cannot be dragged down, and the sweep still refuses to sink below the ground: a ramp
that has run out of length meets the hill and follows it.

    Kaiserberg   1 472 ways lifted off the ground, the most by 14.878 m

**Looked at, and the cliff is gone.** The relaxation runs over way endpoints rather than the
network's nodes, which stay behind their own door -- widening it would have bought nothing this
needs.

## FOUR HYPOTHESES ABOUT ONE ARTEFACT, and the fifth was a measurement

A dashed black line runs along every carriageway's far edge. Guessed and refuted in turn: the ground
poking through between stations; a saw-tooth from my own chord lift accumulating over adjacent
segments; the drape's 32 m quantisation disagreeing with the drawn mesh; the sweep's end caps at
every way boundary. **Each was tested by changing it and looking, and none of them moved the
artefact.**

Sampling the pixels settled it in one step: each band reads road (140,145,160) -> BLACK (20,23,19)
-> ground, one to two pixels wide. That is the slab's own far side face, correctly in shadow at a
ratio of 0.14, aliased at its width. **Not a defect.** The two experiments were taken back.

The cost of guessing four times before measuring once is written here because it was paid here.

## THE STANDARD THIS IS HELD TO

Stated by the owner and it is the right one for a world built from a patchy map: **the generated
world need not be TRUTHFUL, it must be PLAUSIBLE**, and the compromise that is right 99 per cent of
the time beats the rule that is exactly right on the cases OSM happens to describe.

That is why the camera is the oracle here and no ground truth is invoked: there is none for most of
the Earth, and a rule that only works where OSM is complete is not a rule.

## THE SHAPE THIS MAY WANT, and it is being researched rather than guessed

A bridge over WATER gets no height today, and the reason is structural rather than a missing case:
the deck's height comes from what the network says it crosses, and water is not in the network. The
Koehlbrandbruecke crosses a POLYGON, not a course, so there is no centreline to cross.

Rather than bolt a water test onto the side of it, the question was put to research: **has anyone
solved OSM -> real 3D infrastructure, and is there a corpus to be scored against?** The owner's read
is that it lands on a heuristic mapping from OSM to ASAM OpenDRIVE's ROAD MODEL -- not its XML, but
its shape: a reference line, lane sections, an elevation profile and junctions as first-class
objects with connecting roads. That is a description of the very thing this item is assembling by
hand, and if its example files are public they are a corpus with real geometry rather than an eye.

**Held here deliberately until that answer lands**, because building the water case now would be
building it twice.

## TWO BODIES THAT MEET SHARE 3.4 PER CENT OF THEIR CORNERS

The goal asks for vertices snapped to one grid so a junction is a SHARED corner rather than two that
nearly touch. The sweep snaps every position to the millimetre, so two stations at the same place
now produce the same bytes. Measured at Kaiserberg:

    vertices two bodies SHARE      94 068
    vertices in all             2 736 516      = 3.4 per cent

**The snap is not the problem and raising its resolution would not move that number.** Two ways
meeting at an OSM node have DIFFERENT directions, so their profile rings are rotated against each
other and their corners are simply not at the same place. Snapping can only weld what already
coincides, and at a junction almost nothing does.

So the number says what the next piece of work is rather than what is broken: **a junction is a
SURFACE and it needs geometry of its own**, built from the ends of every way that meets there. That
is the one thing in this item neither reference solves for us -- RAGE authors intersections by hand
and Unreal's splines are laid by an artist -- which is why it went out to research rather than being
invented here.

## A DECK OVER WATER STANDS ON WHAT IT SPANS

A channel is a POLYGON, so `Path::Network` finds no crossing there and a deck over the Koehlbrand
would stand on the terrain -- which at a harbour IS the water. The bridge is its own measurement
instead: **how far it runs over a water CLASS is what says whether a punt or a container ship passes
under it**, and the clearance for that span is declared in `vegetation.json` with the headroom it
comes from.

    runM <= 20      0.00 m   a culvert; no headroom is required
    runM <= 60      4.40 m   CEMT class I-III
    runM <= 150     5.25 m   CEMT class IV
    runM <= 300     9.10 m   CEMT class Vb
    beyond         42.00 m   the clearance every high bridge over the Nord-Ostsee-Kanal holds

Measured at Koehlbrand:

    stations under a bridge asked   4 575
    of those a class named          2 232
    and of those, water             1 241
    decks a WATERWAY raised           229
    the clearance the widest took   42.00 m

**UNDER-BUILT BY DESIGN AND NAMED IN THE FILE**: the Koehlbrandbruecke leaves 53 m over a 325 m
channel and this table gives it 42. A per-fairway figure would be truthful; this is plausible, which
is the standard the owner set for a world built from a patchy map.

## AND THE MEASUREMENT WAS PUBLISHED BEFORE THE WORK IT MEASURED

`decks a WATERWAY raised` read **0** while the mechanism was already raising 229 of them. The
counters were published beside the crossing measures, which are computed BEFORE the sweep loop, so
they were read while still zero. The digest was identical across the "fix", which is what said the
geometry had never been wrong.

That is the second measurement defect of this kind in this item, after the four hypotheses about the
dashed line. Both cost the same way: a number was believed before it was asked where it came from.

## THE CULLING HALF OF "SUB-TILE PIECES" IS ALREADY FINER THAN THE ASK

The goal says one body per tile is the wrong unit and an infrastructure body must be cut into
sub-tile pieces that stream and cull on their own. **Half of that is already true, by a mechanism
finer than the ask**, and building parts for it would buy draws and no culling:

    Kaiserberg   cook: clusters in all        56 302
                 ring: a frustum would keep   11 997      79 per cent thrown away
                 cull: jobs it swept          56 302      one per cluster, on the device
                 parts the geometry holds          4      ground, walls, roofs, water, streets

`CookClusters` cuts the whole world into clusters of about ninety-five triangles and
`subjectCull.msl` tests every one against the frustum and the depth pyramid. A PART is a material
boundary here, not a cull unit -- so cutting the streets into sub-tile parts would add draw calls
and remove nothing from the cull that is not already removed.

**What is NOT met is STREAMING.** The world's geometry is assembled and handed over as ONE unit per
rebuild -- 5.36 M street triangles among it -- so a piece the camera cannot see still costs its
share of a 2.0 s rebuild. That is the rebuild architecture rather than the road's, and it belongs to
board:2056 with the phases already measured there.

Recorded rather than built, because a part that culls nothing is a part that only costs.


## FOLDED IN, because they were facets of this and not items of their own

**2087 — laid the way civil engineering lays it.** Neither reference derives a road; the road CAD
packages (Civil 3D, OpenRoads, Vectorworks Landmark) agree completely: alignment, profile, assembly,
**daylighting**, corridor surface. We have the first two, the cross-section is too thin, daylighting
is missing. That one gap is why a road inherits the hillside instead of being level across it: the
batter is what absorbs the slope. Numbers to verify: crossfall min 2.5 % (RAS-Q; A9 measures 2.76),
superelevation max 6 % (RAA; A9 p95 5.8), `e = v²/(127R) − f`, batter 1:1.5.

**2084 — a generator returns a STAMP and the ground applies it.** In the door already:
`Stamp{ring, plateau, falloff}` and an optional `stamps()` defaulting to nothing. The generator
declares because it does not own the ground; the ground applies, in declared order, because two
overlapping stamps disagree. **Blocked by tessellation**: 90.8 % of footprints are narrower than a
25 m ground cell, so moving existing vertices cannot express nine buildings in ten.

**2085 — perfect geometry is decided by a walk.** `--audit` already walks it and is red: 8 308 hole
edges, 83 298 non-manifold, 64 degenerate, 97 needles. Of six properties the tree decides three;
consistent winding, self-intersection and BODY-VERSUS-BODY OVERLAP are undecided, and the last is the
owner's rule. Ceilings are blocked by board:2086.

**2076 — the junction is a surface.** Built: thicken, intersect, trim back, polygon from the trimmed
ends (osm2streets, SUMO `NBNodeShapeComputer`, StreetGen, Wilkie/Sewall/Lin). 8 131 junction bodies,
shared corners 87 504 → 188 822. Open: 3 581 ends still cross, and the answer is that a corner too
tight to drive is a NODE, not a corner — board:1499 measured that every fit refusal is that kind.

**2077 — a deck scored against a measured one.** Hamburg refuted: `HH_WFS_Brueckenbauwerke` is a 2D
inventory, 1 624 footprint polygons and no height, filter `stadium = Bauwerk unter Verkehr`. It
answers PRESENCE and PLAN EXTENT. Heights fall to Duisburg LiDAR class 17, not fetched.
