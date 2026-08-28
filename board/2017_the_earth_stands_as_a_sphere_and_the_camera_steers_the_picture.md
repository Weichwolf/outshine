Type: feature
State: active
Area: world, render
Tags: measured

# The earth stands as a sphere and the camera steers the picture

**Benchmark** — Unreal: Cesium for Unreal's `CesiumGeoreference` rotates ECEF into an ENU tangent frame at a declared origin and `FLargeWorldRenderPosition` keeps the render frame camera-relative; the landscape is a quadtree cut by screen-space error. RAGE: a flat local heightfield grid with SLOD proxies outward, because GTA's map is 8 x 8 km and curvature there is under a metre. **Taking Unreal/Cesium** because this world is EARTH: at the ring's present 8.9 km the drop is already 1.55 m, and a place is supposed to look like itself.

STEP ONE OF THREE, and it carries NO ELEVATION. A perfect sphere, tiles laid over it, camera at
the centre. Height data is 2018 and OSM is 2019. The order is the owner's and the reason is that
each step has its own visible verdict: a sphere with no heights is either round or it is not, and
nothing else can be blamed.

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

## What will be true

- [ ] the render frame is the CAMERA's: one pre-view translation per frame, every terrain vertex a `float` offset from the eye's own ECEF position, rotated by the ENU basis at the eye's lat/lon. The `(void)standing` line is gone and so is every `mPerDeg` term on the terrain path
- [ ] tiles are laid on the ellipsoid with no elevation at all: a tile's grid vertex is `GeoToEcef(lon, lat, 0)` and nothing else
- [ ] the ring reaches the horizon and gets coarser outward -- one zoom per level, constant tile count per level, so the count grows with the LOGARITHM of the reach
- [ ] **the reach is over 100 km**, and the case that states it is the Jura: from the Chasseral at 47.132 N, 7.059 E, 1607 m, the Bernese Alps stand about 100 km away across the Mittelland and MUST be in frame. This is the owner's own bound and it is what sizes the cascade
- [ ] a declared camera steers the picture: moving the eye or turning it changes the image

## The measurements that would show I am wrong

1. **Curvature is a NUMBER, not an impression.** The ring's farthest vertex must sit below the eye's tangent plane by d^2 / 2R: 1.55 m at 4.45 km, 70.6 m at 30 km, 785 m at 100 km. A flat frame reads 0.0 at every distance -- so this measure has a negative control that goes red on the current tree
2. **The horizon is where geometry says.** From height h the terrain's far edge must subtend `acos(R / (R + h))` below level: 0.0906 deg at 8 m, 1.434 deg at 2000 m. Read it off the rendered image's horizon row and it must agree within a pixel
3. **A sphere with no heights has no relief.** With elevation switched off, `so the relief it carries` must read the curvature drop across the ring and NOTHING else. If it reads hundreds of metres, heights are leaking into a step that declared it has none
4. **The seam closes, and the skirt is what proves it.** Two adjacent tiles' shared edge must agree to the raster's precision, so the skirt can shrink to zero and the picture must NOT change. A skirt that is still load-bearing is a seam that is still open

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
