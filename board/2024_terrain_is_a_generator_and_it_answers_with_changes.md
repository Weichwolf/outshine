Type: feature
State: active
Parent: 2017
Area: world, generators

# Terrain is a generator, and it answers with CHANGES

**Benchmark** — Cesium: `Cesium3DTilesSelection::Tileset::updateView(camera)` walks a quadtree, refines a node when its geometric error projected to the screen exceeds `maximumScreenSpaceError`, draws a node whose children have not loaded, and returns a DELTA -- tiles to render, tiles to load, tiles to unload. Unreal: landscape components pick a LOD by screen-space error and World Partition streams cells by distance; Nanite builds its cluster DAG OFFLINE at import. RAGE: sectors stream in and out with SLOD proxies standing in for distance. **Taking Cesium**, because georeferenced streaming terrain is exactly what it is FOR and the other two solve a bounded map.

**THE OWNER STATED THE API, and it is Cesium's shape in this tree's own words:** at start you tell
the terrain generator WHERE you are and HOW FAR you want to see -- that is all. On movement you hand
it the position. It answers with CHANGES, and only when a tile boundary is crossed or a LOD
switches. Terrain belongs in `src/generators/` beside the forest and the buildings, because a
generator is what turns a place into geometry.

**Preload is an ENGINE capability, not a client loop.** No player wants to watch a world assemble.
From then on it streams, and only what is NEW is processed. `Engine::preload` and
`Engine::loadProgress` are the door for that -- Filament spells the wait `flushAndWait`, Cesium
answers load progress so the client can draw a bar.

## What the cascade in the tree today is, and why it goes

`Around::Levels` lays a fixed 4x4 block per level, one zoom coarser each level. It works and it is
measured -- 76 to 128 tiles, 6.3 to 407 km of reach, curvature exact to 0.003 per cent at 388 km --
and it is still the wrong shape, for reasons the measurements themselves produced:

- **it has no screen-space error.** Levels are geometric. This tree ALREADY has an error cut in
  `DagSelect`; two LOD mechanisms that do not talk is the defect
- **it asks for everything at once.** At 240 km of declared sight it requests 128 tiles and loaded
  0 per cent in eight seconds; at 25 km it requests 64 and loads about 70 per cent
- **it rebuilds rather than deltas.** Every re-lay costs a full vertex build, a `Gltf::Subject`
  assemble and a `Restand`, where Cesium hands back a list of what changed
- **priority was inverted and had to be repaired by hand.** Sorting the queue by distance alone let
  a coarse tile covering hundreds of kilometres -- centred on the camera, so distance ~0 -- beat
  every fine tile the eye resolves. Measured: 73 per cent of tiles loaded and 59 m of relief where
  the rim-to-river drop is over 1 500 m. Depth now leads and distance breaks the tie, which is a
  stand-in for the error term rather than the thing itself

## What board:2017 handed over when it closed

2017 closed on its own scope, measured: the frame is the camera's tangent frame (curvature 3.32 m
at 6.5 km, 1 008.07 m at 113 km, 11 814.26 m at 388 km, each within 0.3 per cent of d^2/2R and each
reading exactly 0.0 on the plate carree it replaced); the seam closes to 0.00 m over 4 832 shared
vertices with no skirt; nothing on the terrain path waits on IO; the reach is DECLARED through
`WorldSettings::SightM`; a declared camera steers the picture; and the Grand Canyon renders 2 508 m
of relief.

Two of its predicates were NOT met and they are this item's:

- the cut is geometric rather than by screen-space error
- a re-lay is a full rebuild rather than a delta

One of its measurements was never taken: the horizon's subtended angle read off the rendered image.
The curvature number derives from the same geometry and is stronger, but the image-side check would
have caught a projection error that the vertex-side one cannot see. It is listed here rather than
quietly dropped.

## THE BLOCK CASCADE CANNOT NEST, AND THREE MEASUREMENTS SAY SO

Not an opinion about elegance -- three configurations were built and measured at the Grand Canyon
with 240 km of declared sight, and every one of them is unusable:

    rule                        tiles   skipped   overlapping   seam apart   what the frame shows
    coverage by rectangle          112        0            24     1495.59 m   coarse through fine
    coverage by MASK, skip full     99       13            11     1495.59 m   coarse through fine
    ... and drop on TOUCH           88       24            15        47.54 m   holes of ellipsoid
    block 6 wide instead of 4      235       17            16         0.01 m   3x the tiles, still over

The cause is arithmetic rather than a bug. Each level snaps to its own zoom's tile grid, and for
level k's block to sit exactly inside level k+1's inner quarter, every level's origin would have to
satisfy a parity condition simultaneously -- which no choice of starting origin gives. Widening the
block does not help: at 6 tiles the overlap is unchanged and the count triples.

So a coarse tile is either drawn through a finer one -- two surfaces 1 495.59 m apart and 100.75 deg
apart in normal, fighting for the same pixels -- or dropped and leaving a hole. The tree stands on
DROP-ON-TOUCH, because a visible gap reads as "not loaded" while interpenetration reads as a broken
engine, and because it takes the seam from 1 495.59 m to 47.54 m.

**A QUADTREE CANNOT PRODUCE EITHER**, and that is the whole argument: it PARTITIONS. A node is
refined -- its four children drawn -- or it is drawn itself. Never both, never neither. Cesium's
`Tileset` is exactly this walk, and the 47.54 m residual becomes a screen-space-error threshold
instead of an accident of two zoom levels sampling the same ground.

## What will be true

- [ ] terrain is a generator under `src/generators/`, told a place and a sight once and a position thereafter
- [ ] it answers with a DELTA -- what entered, what left, what changed level -- and says nothing when nothing changed
- [ ] the cut is by SCREEN-SPACE ERROR against one declared threshold, not by a level table
- [ ] `Around::Levels`, `kBlockTiles` and `SphereTile` are gone, and the bare ellipsoid survives only as the answer for a node with no resident ancestor
- [x] `TilePool::Focus` has a caller. It had none, so `TileDistance` measured from the pool's construction origin whatever the camera did
- [ ] **the pool holds what it built.** `Done_` is a one-shot MAILBOX, not a cache: the walk that builds geometry consumes each finished tile and drops its key from `Posted_`, so the next walk that asks finds neither and re-posts it. Every tile reads "pending" again the moment it has been used, which is why a progress fraction taken from the queue answered 0 per cent while 88 of 128 tiles carried ground. A generator that answers with CHANGES cannot be built on a mailbox -- it has to know what it already handed over

## The measurements that would show I am wrong

1. **A standing camera does no work.** After preload, with the eye still, the generator must answer "nothing changed" and `advance` must not enter a rebuild. The negative control is today's tree, where a stack sample of a standing camera lands in `Grounds -> Restand -> Live::Stand` on every frame
2. **Crossing one tile boundary changes a bounded number of tiles.** Walk the eye slowly across a boundary: the delta must name a handful, not the whole set. A delta the size of the set is a rebuild wearing a new name
3. **The reach is affordable.** 240 km of declared sight must reach a resident picture within the same patience that 25 km does today, or the priority is still wrong. Today 25 km loads about 70 per cent in 8 s and 240 km loads none
