/* FlightBox renderer — terrain level-of-detail policy: the quadtree's tuning constants and the
 * texture-LOD ramp. Split out of world3d.h; included from within its W3_USE_OSM streaming block. */
#ifndef W3_TERRAIN_LOD_H
#define W3_TERRAIN_LOD_H
#ifndef W3_ROOTZ
#define W3_ROOTZ 8 /* coarsest level: one chunk ~96 km at 52 N */
#endif
#ifndef W3_MAXZ
#define W3_MAXZ                                                                                    \
  14 /* finest level. The albedo bake stops here: fb-tiles' vector                                 \
      * source (VersaTiles Shortbread) has no z15. */
#endif
/* --- texture LOD ramp -----------------------------------------------------------------------
 * The albedo used to be ONE fixed size (512) at every level -- "what makes it a chunk". But a
 * chunk right under the camera fills far more screen than one at the horizon, so a single size is
 * wrong at both ends: 512 stretched over a near chunk is blurry, 512 baked for a far one is wasted
 * VRAM and a wasted server bake. The size is now chosen per draw from the chunk's on-screen size
 * (w3_chunk_px -> w3_lod_for_px): the smallest ramp step that still covers it.
 *
 * The two callers -- w3_walk (the chunk's own distance) and w3_children_ready (the parent asking on
 * the children's behalf) -- can pick different steps for the same tile. That disagreement is the
 * OLD 1-FPS thrash: back then `tex` was a size passed BESIDE an unkeyed single texture, so a hit
 * returned the wrong size and the "fix" was a re-bake, every frame, both ways. The cure is to make
 * the step part of what a texture IS: tex[mode][lod] holds every size the tree has asked for, side
 * by side. A disagreement then costs one extra bake, ONCE, and is a cache hit forever after --
 * monotone state cannot flutter. Release stays at exactly one place, w3_cache_trim.
 *
 * The sizes are the server's bake sizes: the JS bake cache is already keyed by tex (tilesrc_js.h),
 * and the server bakes per-tex, so a step maps straight to a bake request. 2048 is safe by the
 * WebGL2 spec (MAX_TEXTURE_SIZE >= 2048); w3_lod_cap trims the table to what the context reports,
 * so a spec-minimum context simply never uses a step it cannot hold. */
#ifndef W3_NLOD
#define W3_NLOD 4
#endif
static const int w3_lod_px[W3_NLOD] = {256, 512, 1024, 2048};
static int w3_lod_cap = W3_NLOD; /* trimmed at gfx init: min(hardware cap, policy ceiling below) */

/* The ramp uses its full range (256..2048); the only cap is the hardware MAX_TEXTURE_SIZE. Load
 * time is second to quality -- a big bake is paid ONCE and cached, and progressive loading (below)
 * means it never blocks the first, visible image (a fresh 2048 OSM bake is 7-20 s against the live
 * server; the 256 shows at once and the tile sharpens up the ladder behind it). 2048 is justified
 * for BOTH albedos, each for its own reason, both MEASURED: PHOTO carries real z16 sub-metre
 * imagery detail; OSM is VECTOR, and the MVT source extent is 4096 -- finer than a 2048 texture --
 * so rasterising at 2048 keeps street and footprint edges HARD (the max luma gradient is identical
 * at 1024 and 2048; an upscale of 1024 would soften it). Sharper edges, less aliasing -- not
 * upscaled. (An earlier note here claimed 2048 OSM was detail-free upscaling because "the source
 * ends at z14". That was a measurement error: z14 is the TILE zoom, not the vector precision --
 * frequency/area averages miss edge sharpness, which is exactly where vector rasterisation wins.
 * Corrected against the extent and the gradient.) The only real ceiling above 2048 is the extent
 * (4096), which is VRAM-bound, not a source limit. W3_LOD_MAXSTEPS stays a knob only for a
 * spec-minimum context that cannot hold the full ramp; normal contexts (>=2048 by the WebGL2 spec)
 * use every step. */
#ifndef W3_LOD_MAXSTEPS
#define W3_LOD_MAXSTEPS W3_NLOD
#endif

/* Smallest ramp step whose texels meet the chunk's on-screen pixels (>= px), clamped to the caps.
 * Under-supply (a small texture stretched over a big screen area) is the visible defect; over-
 * supply is absorbed by the mipmap chain, so round UP. */
static int w3_lod_for_px(double px) {
  int lod = 0;
  while (lod < w3_lod_cap - 1 && (double)w3_lod_px[lod] < px)
    lod++;
  return lod;
}

/* Proof scaffolding, both compiled out of production (default 0):
 *   W3_LOD_INJECT -- force w3_children_ready to request one step away from w3_walk, so the
 *                    side-by-side slots are actually exercised by a disagreement.
 *   W3_LOD_NOKEY  -- collapse the ramp back to a single stored texture per mode that re-bakes on
 *                    a size change: this reproduces the OLD thrash, and is the control the middle
 *                    line of the proof is measured against. See w3_bake / the mipmap counter. */
#ifndef W3_LOD_INJECT
#define W3_LOD_INJECT 0
#endif
#ifndef W3_LOD_NOKEY
#define W3_LOD_NOKEY 0
#endif
#ifndef W3_LOD_SPIN
#define W3_LOD_SPIN 0 /* proof only: walk every frame (see world3d_stream_at) */
#endif
#if W3_LOD_NOKEY
#define W3_LODIDX(lod) 0
#else
#define W3_LODIDX(lod) (lod)
#endif
#ifndef W3_TERR
#define W3_TERR                                                                                    \
  22 /* chunk geometry: 22x22 quads. The texture carries the detail;                               \
      * the height field only has to carry the shape. */
#endif
#ifndef W3_EPS
#define W3_EPS 2.0f /* the ONLY quality knob: allowed screen-space error, pixels. */
#endif
#ifndef W3_REACH
#define W3_REACH                                                                                   \
  240000.0f /* how far the ground is drawn. Just a parameter -- the tree                           \
             * covers whatever it is given; the horizon at 4000 m is 226 km. */
#endif
#ifndef W3_VIEWH
#define W3_VIEWH 480 /* the height SSE is measured in: the camera FBO (NTSC 480). */
#endif
/* SSE = error * K / distance, with K = viewport_height / (2 tan(fov/2)). This is the standard
 * chunked-LOD projection of a vertical error onto the screen. */
#define W3_SSE_K ((float)(W3_VIEWH) / (2.0f * 0.8390996f)) /* tan(40 deg) for W3_FOV=80 */
/* A budget is needed because the error comes from the DATA: over Hameln (96 m of relief) the z8
 * chunks stop splitting immediately, but the same threshold at Everest keeps splitting even at the
 * horizon (2093 m error -> 2.65 px at 226 km). Flat terrain costs nothing, mountains would run
 * away. So: cap the resident set and spend it on the worst error first. */
#ifndef W3_BUDGET
#define W3_BUDGET                                                                                  \
  128 /* max chunks drawn/resident. 128 * 512^2 RGB * (mips) * 2                                   \
       * albedos ~ 268 MB -- about what the three rings cost. */
#endif
/* Camera near/far. near was 2 m: at that ratio a 16-bit depth buffer cannot resolve 700 m at
 * 10 km. Nothing is ever within 20 m of the camera -- it sits in the aircraft -- and moving the
 * near plane out is the single cheapest precision win there is (10x, for one number). */
#ifndef W3_NEAR
#define W3_NEAR 20.0f
#endif
#ifndef W3_FARPLANE
#define W3_FARPLANE 240000.0f /* past the z8 ring; the horizon at 4000 m is 226 km */
#endif
#endif
