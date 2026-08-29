Type: bug
State: open
Parent: 1946
Area: world
Tags: measured

# The building soup is bounded, and every vertex in it belongs to one frame

**Benchmark** — Unreal: streamed geometry is owned by the cell that streamed it and goes when the cell goes. RAGE: a drawable lives in the streaming heap under the `strIndex` of the map sector that requested it, and `CStreaming`'s eviction frees it with that sector -- geometry has no lifetime of its own. **They agree**, so the matter is closed: geometry belongs to the thing that brought it and is bounded by it. Cesium is cited rather than counted: a tile's buffers are freed when the tile unloads, which is the same rule under a third name.

MEASURED, from the telemetry the places now attach:

    t=0.0 INFO world buildings added=3228 total=5140 osmHeight=3136 defaultHeight=2004 vertsMB=59.7692

`BuildingField::Verts_` GROWS on every `Restand` and is never trimmed. At Rothenburg it reached
59.8 MB; at Shibuya the meshed count reads 6 121 139 triangles, which is roughly 600 MB of soup.
Folding that into the ring's own arrays -- a bisection, not a design -- crashed all six places with
SIGNAL, which is how the growth was found.

The field already carries the answer and nobody uses it: `AddedFirst()` and `AddedCount()` mark the
range added by the last build, exactly so a consumer can take a DELTA. Reading `Verts()` whole is
what makes the size unbounded.

## THE ANCHOR HYPOTHESIS IS DEAD, killed by the number this item demanded

It read: vertices meshed before an anchor move would be read against the wrong anchor and land
elsewhere, leaving a handful of survivors as the two pixels board:1946 measured. The measurement it
named was the anchor's ECEF against the frame's origin.

    buildings: their anchor lies from the frame's origin    0 m

Zero. The anchor and the render frame's origin coincide exactly, so no vertex is read against the
wrong point and this explains nothing about board:1946. WITHDRAWN.

What the same measurement did show is the size:

    floats in the soup                14 942 304    = 1 867 788 vertices = 622 596 triangles
    the field's last delta began at    5 682 768
    and ran for                        9 259 536    -- 62 per cent of the array, in ONE build
    footprints the field holds             5 140    -- 121 triangles each

## What will be true

- [ ] the soup is bounded: a build takes the delta the field already marks, or the field trims what it replaced
- [ ] every vertex handed over is in the frame of the anchor it is read against, and the anchor's movement is a published number
- [ ] Shibuya does not mesh 600 MB of buildings to draw a square kilometre of them


## IT BLOCKS NOTHING -- THAT CLAIM WAS MINE AND IT WAS WRONG

I wrote that Rothenburg's triangle count moved between runs of the same code: 604 309, 582 147,
601 897. Run properly, twice, on ONE code state it reads 601 897 both times, with 5 140 footprints
and 5 vector tiles. Those three numbers came from three different code states -- the ring tidy, an
area cutoff, an aspect cutoff -- and I compared them as though the code had stood still. Withdrawn.

`added=3228 total=5140` is not duplication either: the vector ring is 3 x 3, five of its tiles
settled, and 5 140 is their sum. `TileWatermark` takes each tile once.

What remains true and worth the item: `Verts_` is never trimmed, so it holds every tile the field has
ever ingested for as long as the field lives. At Shibuya that is 5.8 million triangles. The bound is
still owed; the drift was not.

## What distance does to OSM geometry, and what the cluster DAG does instead

**These are TWO mechanisms and conflating them is the mistake to avoid.** The generator decides what
geometry EXISTS at a distance; the cluster DAG decides which of it to DRAW and at what detail. One is
about content, the other about screen-space error, and neither can do the other's job.

**Benchmark** — Unreal: `World Partition` + HLOD, two or three layers, each merging a cell's actors
into ONE drawable with a shared atlas; Nanite then runs a screen-space-error cut over the clusters of
whatever those drawables contain. RAGE: an entity carries `lod`, `slod1`, `slod2`, `slod3` -- the
higher tiers are merged, simplified groups of buildings drawn per map block -- and the LOD switch is
distance-driven while the rendering is not. **They agree on the split and on the tier count**, so the
matter is closed: three coarse tiers above the detailed one, merged per spatial cell, one buffer
each.

    tier   what it is                                   one draw per
    L0     full architecture per building               near cell, per material class
    L1     an extruded prism per footprint, flat top    tile
    L2     footprints of one block dissolved into one mass at the block's mean height   tile
    L3     the tile's built-up area as a single mass    tile

The near bound is derived from the lens rather than chosen: at 720 px over 55 deg a pixel is
0.076 deg, so a feature of size `w` covers one pixel at `w / 1.33e-3`. A gable, a chimney or an eaves
band is of the order of a metre and is inside ONE pixel beyond about 750 m; a whole 27 m building
still covers 20 px at 20 km. So architecture stops at roughly a kilometre and MASS carries on.

**The measurement that would show this is wrong**: `apps/bench` and the places' own triangle counts.
At Shibuya the full path meshed 12.9 M triangles and 313 MB of vertices from ONE vector tile, the
terrain never got a core to build on, and 112 tiles stood bare -- which is the number this design has
to move. If the tiers are in and that number does not fall, the tiers are not where the cost was.
