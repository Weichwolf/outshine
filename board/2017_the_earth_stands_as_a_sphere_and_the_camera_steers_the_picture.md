Type: feature
State: active
Area: world, render
Tags: measured

# The earth stands as a sphere and the camera steers the picture

**Benchmark** — Unreal: Cesium for Unreal's `CesiumGeoreference` rotates ECEF into an ENU tangent frame at a declared origin and `FLargeWorldRenderPosition` keeps the render frame camera-relative; the landscape is a quadtree cut by screen-space error. RAGE: a flat local heightfield grid with SLOD proxies outward, because GTA's map is 8 x 8 km and curvature there is under a metre. **Taking Unreal/Cesium** because this world is EARTH: at the ring's present 8.9 km the drop is already 1.55 m, and a place is supposed to look like itself.

STEP ONE OF THREE, and it carries NO ELEVATION. A perfect sphere, tiles laid over it, camera at
the centre. Height data is 2018 and OSM is 2019.

**THE ORDER IS NOT A BUILD ORDER, IT IS THE RUNTIME ORDER, and that is the owner's correction:**
render the terrain NOW as a sphere, ASK for elevation and displace when it arrives, ASK for imagery
and texture when it arrives. Nothing waits. The sphere with no heights is therefore not a test
scaffold that gets replaced -- it is the FIRST FRAME, and every frame after it is the sphere plus
whatever has landed.

**NOTHING MAY EVER BLOCK THE ENGINE.** `Around::Awaited` is set true and `LayPatchwork` calls
`tiles.MeshAwaited`, so the frame waits on the network. CLAUDE.md names this as an invariant --
four things run independently and IO is the fourth, because a fetch BLOCKS and a blocking wait on
a compute path is a worker doing nothing while holding a slot -- and the terrain path breaks it.
The 122 s stalls in `outshine/places` were never a slow fetch: they were the frame path waiting,
per tile, with the patience bound applied per tile rather than per ring. The owner's word is that
this was already built correctly once.

## What is wrong now, measured

**A cause written into this item did not survive its own measurement, and it was mine.** I recorded
that the picture does not follow the camera, on the evidence that moving the eye from 60 m to
2000 m left the image bit-identical. It did not: the case binary was stale and I compared an old
image with itself. Rebuilt, the eye reads 4125.8 m up -- 2125.8 m of ground plus the declared 2000
-- and the picture changes completely. The camera steers. That claim is withdrawn.

What the flight-altitude picture actually shows, and it is worse than a wrong camera:

**The tiles do not meet.** Every place except Shibuya renders as a QUILT: each tile a plateau whose
border droops, with a black trench along every shared edge. The trench is the SKIRT --
`ChunkMesh.h` drops the border ring radially by `2 * err`, at least 5 m -- and a skirt is meant to
be invisible edge-on. Seeing into it from 4125 m means it is hiding a real gap, not a hairline:
measured off the Grand Canyon frame, the bands are about 20 px at roughly 3 km, and at 0.0972 deg
per row that is on the order of 100 m of drop. Adjacent tiles disagree at their shared edge by
tens of metres because each one takes its own DAG cut and nothing reconciles the seam.

**The frame is a plate carree, not a sphere.** `Picturing.cpp` builds the ENU basis -- `east`,
`north`, `upward` from the frame's lat/lon -- applies it to the NORMALS, and then discards it for
the POSITIONS: `(void)standing;` stands in the source, and each vertex takes
`(lon - lon0) * mPerDegLon`, `AltM`, `-(lat - lat0) * mPerDegLat` instead. `EnuFrame::TryFromGeo`
is the same approximation, so it is not the fix. Every tile is unrolled onto a flat map and the
sphere the tiles were built on is thrown away between the mesher and the renderer.

**The ring is one square at one zoom.** `over.Ring` -- a number with no origin -- gives a single
(2R+1)^2 block at the finest zoom: 9 tiles and 2966 m at 1, 81 tiles and 8898 m at 4. The
geometric horizon at 8 m of eye height is sqrt(2 R h) = 10.1 km; at 2000 m it is 160 km. The ring
must reach that and get COARSER outward, which it cannot do at one zoom.

**The Grand Canyon renders as night at 10:36 in the morning.** Local time at the five places, taken
from this machine's clock: Tokyo 02:36, Phoenix 10:36, Berlin 19:36. Shibuya being black is
therefore CORRECT and not a placement error -- what is missing there is a moon, stars and city
light, which no item yet claims. But the canyon stands in mid-morning sun and renders blue-on-olive
with the terrain lit by ambient alone. That is a lighting defect and it is filed apart, because it
would still be there over a perfect sphere.

## WHAT I BUILT HERE IS A PLACEHOLDER, AND THE FOUR SAY WHY

Audited against Filament, Cesium, RAGE and Unreal rather than against whether the picture improved.
Three of the four things in the cascade do not survive that.

**1. A fixed 4x4 block per level is not a screen-space-error cut, and nobody does it that way.**
Cesium's `Tileset` walks ONE quadtree and refines a node when its geometric error, projected to the
screen, exceeds `maximumScreenSpaceError` -- one knob, no level table. Unreal's landscape and Nanite
both cut by screen-space error. RAGE selects a sector's LOD by distance banded against its own
proxy. So the cascade is a GEOMETRIC approximation of a cut that should be an ERROR cut, and worse:
this tree ALREADY has an error cut in `DagSelect`, inside a tile. Two LOD mechanisms that do not
talk to each other is the naive part -- the outer one should not exist. The tile pyramid IS the
quadtree; `Around::Levels` and `kBlockTiles` are a table standing in for walking it.

**2. `SphereTile` throws away data the tree already holds.** Cesium covers a node whose children
have not loaded with the node itself -- real terrain, coarser. The old flightbox streamer wrote the
same rule down: "a chunk that has not arrived is covered by its parent". I substitute a FLAT
ELLIPSOID PATCH, which is correct in place and curvature and carries no elevation at all. Whenever
a coarser ancestor is resident, that is strictly worse than what both references do. It is only the
right answer for the very first frame at a place where NOTHING is resident yet.

**3. The whole terrain is rebuilt every frame.** `Grounds()` re-lays every tile, rebuilds the vertex
and index arrays, re-assembles a `Gltf::Subject` and calls `Restand` -- per frame. Cesium's
`updateView` returns a DELTA: what to render, what to load, what to unload. Unreal keeps landscape
components resident and changes only their LOD. RAGE streams sectors in and out. CLAUDE.md already
carries the rule -- the work a declaration causes is proportional to what CHANGED -- and a full
rebuild per frame is the opposite of it.

**What the honest structure is**, and it is one thing rather than three: a quadtree walked from a
root that covers the declared sight, refining by screen-space error, stopping where children are not
resident and requesting them, emitting a delta, never blocking. That is Cesium's shape, the old
flightbox streamer's shape, and it subsumes the cascade, the bare tile and the per-frame rebuild.
The cascade stays only until it is written, and it stays MEASURED so the replacement has numbers to
beat: 76-128 tiles, 128-388 km of reach, 1 846 ms to stand, 2.56 ms a frame.

## THERE IS NO RING. THERE IS ONLY STREAMING.

The owner's framing, and it is a NAME question as much as a design one: a game engine is always a
streamer, and a camera that does not move is the degenerate case of streaming -- not a different
operation with a different shape. This tree says `LayPatchwork`, `Around::Ring`, "the ring's
farthest vertex", and every one of those words promises a fixed thing laid once around a fixed
point. That promise is what let `Composes` lay it a single time at assemble and let nothing re-lay
it, which is why 128 of 128 tiles stood bare and no elevation could ever land.

So the vocabulary follows the design rather than leading it: when the streamer is real, `ring`
leaves with it. Renaming it before then would be the blind rename this tree already paid for once.

## What will be true

- [ ] the render frame is the CAMERA's: one pre-view translation per frame, every terrain vertex a `float` offset from the eye's own ECEF position, rotated by the ENU basis at the eye's lat/lon. The `(void)standing` line is gone and so is every `mPerDeg` term on the terrain path
- [ ] a tile that has not arrived is laid on the ELLIPSOID rather than skipped: its grid vertex is `GeoToEcef(lon, lat, 0)` and nothing else, so the ring is complete on the first frame
- [ ] `Around::Awaited` is gone and no terrain call waits on IO. The picture is drawn from what has landed, and what lands later refines it
- [ ] the ring reaches the horizon and gets coarser outward -- one zoom per level, constant tile count per level, so the count grows with the LOGARITHM of the reach
- [ ] **the reach is DECLARED, not coded, and its default is 240 km.** The owner's 100 km was a rough figure and the real bound is the geometric one: from height h the horizon is sqrt(2Rh), so 240 km is what an eye at 4 520 m sees, and equally what a 4 000 m peak is visible from at 225 km. `WorldSettings::SightM` carries it and the cascade derives its level count from it -- `1 + ceil(log2(sight / (4 * tileSpan)))` -- so no number in the source decides how far the world goes
- [ ] the case that states it is the Jura: from the Chasseral at 47.132 N, 7.059 E the Bernese Alps stand 95.4 km away across the Mittelland -- derived from the Jungfrau's coordinates, 66 124 m north and 68 768 m east, bearing 133.9 deg -- and MUST be in frame
- [ ] a declared camera steers the picture: moving the eye or turning it changes the image
- [ ] the terrain is re-laid EVERY FRAME around wherever the eye now is, not once around where the scenario was declared -- you can move around the whole world
- [ ] the client can ask the engine whether it has settled, and take its picture then. The engine never waits; the CLIENT's patience is the client's own

## The measurements that would show I am wrong

1. **THE FIRST FRAME IS INSTANT.** Time the case's `assemble` on a cold cache with the network unreachable: it must stand and draw a sphere in milliseconds. Today it stalls to the runner's 122 s bound, and that number is the negative control
2. **Curvature is a NUMBER, not an impression.** The ring's farthest vertex must sit below the eye's tangent plane by d^2 / 2R: 1.55 m at 4.45 km, 70.6 m at 30 km, 785 m at 100 km. A flat frame reads 0.0 at every distance -- so this measure has a negative control that goes red on the current tree
3. **The horizon is where geometry says.** From height h the terrain's far edge must subtend `acos(R / (R + h))` below level: 0.0906 deg at 8 m, 1.434 deg at 2000 m. Read it off the rendered image's horizon row and it must agree within a pixel
4. **A sphere with no heights has no relief.** With elevation switched off, `so the relief it carries` must read the curvature drop across the ring and NOTHING else. If it reads hundreds of metres, heights are leaking into a step that declared it has none
5. **The seam closes, and the skirt is what proves it.** Two adjacent tiles' shared edge must agree to the raster's precision, so the skirt can shrink to zero and the picture must NOT change. A skirt that is still load-bearing is a seam that is still open

## Why a cascade, with the arithmetic

One square at one zoom cannot reach 100 km: at roughly 1 km per tile that is 200 tiles across, so
40 000 tiles. A cascade costs the logarithm instead. Level k sits at zoom `z - k` and lays a 4x4
block, minus the inner 2x2 that level `k-1` already covers -- and those align exactly, because 4
tiles at zoom `z-k+1` span 2 tiles at zoom `z-k`. So level 0 costs 16 tiles and every level after
it costs 12.

    level   zoom    reach                tiles   running
      0     z         4 km                 16       16
      1     z-1       8 km                 12       28
      2     z-2      16 km                 12       40
      3     z-3      32 km                 12       52
      4     z-4      64 km                 12       64
      5     z-5     128 km                 12       76

Reach = 4 * 1 km * 2^k, so 100 km lands inside level 5 and the whole world to the Alps costs 76
tiles -- fewer than the 81 the present single square spends to reach 8.9 km. The screen-space error
of a level is bounded by its own zoom, which is the same argument Unreal's landscape LOD and RAGE's
SLOD both make, and it is why the count may stay flat while the reach doubles.

## What the flight camera is, and what it is not

The five cases were moved to 2000 m above ground at -35 deg of pitch because at eye height the ring
fills the frame edge-on and a defect the size of a tile cannot be told from a defect the size of the
world. From altitude the whole ring lies in frame and its edge is visible. This is a VIEWING choice
for judging, not a claim that the engine works better from up there.

## What this item does NOT decide

Nothing here judges how the terrain LOOKS -- no shading, no material, no seams between tiles as a
visual matter. It decides where a vertex STANDS. The five place pictures are the gate that follows
and they are not a proof.
