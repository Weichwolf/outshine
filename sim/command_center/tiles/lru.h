/* FlightBox renderer — the baked-tile LRU cache, keyed by z/x/y. Holds one VBO plus tex[mode][lod]
 * per chunk, trims what the tree did not ask for this pass, and takes the worker mesh callbacks.
 * The one place a tile texture/VBO is created; w3_cache_trim is the one place it is freed. */
#ifndef W3_TILES_LRU_H
#define W3_TILES_LRU_H
/* ---- baked-tile cache (LRU, keyed by z/x/y) ----
 * Crossing a tile boundary used to DELETE and re-bake all 34 tiles: a multi-hundred-ms
 * freeze every ~44 s in a 1000 m orbit (the video froze then jumped = the "kick"). But an
 * orbit flies over the SAME tiles again and again, so we keep baked VBO+texture around and
 * only bake genuinely new ones. Sized to hold a full orbit -> after the first lap nothing
 * is re-baked at all. Eviction is stalest-first, gated on VRAM pressure (see w3_vram_budget below). */
/* The cache holds every chunk the tree may draw, plus room for the ones it is refining INTO.
 * A chunk that is still being replaced by its children must stay resident -- that is what lets the
 * parent keep drawing until all four children have landed, instead of a hole appearing.
 * Eviction is "the tree stopped asking" -- but with a GRACE PERIOD, not instantly. At the far-field
 * margin (Alps at the horizon) the SSE/budget split decisions flicker frame to frame, so the asked
 * set's boundary oscillates; instant eviction destroyed resident tiles on a 1-frame flicker and the
 * refetch takes seconds -> visible holes + permanent re-fetch churn ("waiting on fb-tiles" forever).
 * Eviction is instant, refetch is not -- the asymmetry demands hysteresis. A chunk must go unasked
 * W3_TRIM_GRACE consecutive passes before it is even ELIGIBLE for trim -- whether trim actually
 * frees it is a second, independent question, answered by VRAM pressure (w3_vram_budget below).
 * A working set that fits the budget never evicts past GRACE; one that does not grows the cache
 * (dynamic, demand-scaled) until it either fits or pressure starts recycling the stalest slots. */
/* DYNAMIC cache: grows on demand (doubling realloc) — the tile buffer scales with the view-distance
 * setting and the terrain's actual demand, never a compile-time cap. */
static int w3_cache_n = 0;
#define W3_TRIM_GRACE 900 /* ~15 s at 60 passes/s -- far longer than any split-decision flicker;
                            * the FLICKER minimum, not the eviction trigger -- see w3_vram_budget. */
/* Eviction budget (bytes): a grace-expired chunk stays warm as long as VRAM is under this. Default
 * 1.5 GiB; FB_VRAM_MB overrides (cc.cpp cfg_from_js), same pattern as FB_VIEW_KM/w3_view_m. A closed
 * orbit/loiter ring revisits every chunk well within one lap; GRACE alone (~15 s) is shorter than a
 * slow ring's period (Alps-loiter measured ~225 s), so time-only eviction aged out the ring's far
 * side every lap and re-fetched/re-baked it -- the re-bakes cost more passes, which age chunks
 * faster, which re-bakes more (soak test: cache_bakes 0.37->2.14/s, texvram 585->950 MB, no
 * plateau). Gating eviction on real pressure (not just elapsed passes) lets a ring that fits the
 * budget converge to zero re-bakes after the first lap. */
static long w3_vram_budget = 1610612736L; /* 1.5 GiB = 1.5 * 1024^3 */

/* Per-pass GPU upload budget: caps how many w3_bake calls (stbi decode + glTexImage2D +
 * glGenerateMipmap, all synchronous) run before the rest wait a frame. Amortized uploads are
 * standard streaming-texture pacing -- without a cap, several worker deliveries landing in the
 * SAME frame multiply frame time (measured: median 102ms -> 390ms, max 652ms at baked>=6 bakes/
 * frame; sim-critic v14). A bake that misses the budget is not lost: the slot stays MESH (or READY
 * at its current, coarser step) and w3_ensure_tex is simply asked again next pass -- the same
 * "retry, never block" contract w3_cache_get already gives the rest of the pipeline. */
#define W3_BAKE_PER_FRAME 2
static int w3_bake_budget = 0; /* reset each pass in world3d_stream_at (terrain/stream.h) */

/* tex[mode][lod] holds BOTH albedos at EVERY size the tree has asked for. [mode] is the ground
 * source: [W3_GROUND_OSM] = the OSM render, [W3_GROUND_PHOTO] = the aerial photo -- both baked
 * while streaming, so switching the ground is an index change, not a re-bake (the moment you need
 * the camera fallback is the moment you cannot afford to build it). [lod] is the texture size step
 * (w3_lod_px): a moving camera asks the same tile for different sizes as it nears, and each is kept
 * side by side rather than re-baked over the last -- that is what turns the old size-disagreement
 * thrash into one extra bake. The VBO (the expensive part) is shared across all of it -- the
 * geometry is LOD-independent and does not care what size is painted on it.
 * Any tex[mode][lod] is 0 until baked; the draw picks the nearest resident size (w3_pick_lod), and
 * PHOTO falls back to OSM. */
/* A slot's LIFECYCLE lives IN the slot, not in a side channel. The mesh is built OFF the main
 * thread now (tileworker.c), so a tile is no longer either absent or done -- it passes through
 * "asked the worker" and "worker delivered, not yet on the GPU". Putting `state` beside the entry
 * it describes is the same lesson as `tex` belonging IN the cache key: a per-entry fact kept next
 * to the entry instead of inside it is the bug this project has already paid for twice. It also
 * keeps the coming tex[mode][lod] change orthogonal -- that adds a dimension to `tex`, it never
 * touches `state`. */
enum {
  W3_SLOT_FREE = 0, /* unused */
  W3_SLOT_WAIT,     /* posted to the worker; no mesh back yet */
  W3_SLOT_MESH,     /* worker delivered verts (in mverts, main heap); not yet uploaded */
  W3_SLOT_READY
}; /* vbo uploaded + OSM albedo present -> drawable (the old `valid`) */
typedef struct {
  int z;
  uint32_t x, y;
  GLuint vbo, tex[2][W3_NLOD];
  int nverts;
  unsigned touch;
  int stale;              /* consecutive trim passes the walk did NOT ask for this chunk */
  int state;
  int photo_none;         /* the server has no photo here, ever (size-independent) */
  int want_lod_max;       /* highest ramp step this chunk justifies THIS pass (its SSE
                           * target). trim frees resident steps above it -> a receding
                           * chunk gives its fine steps back. Per-pass max, distance-only. */
  float err;              /* measured geometric error, metres -- drives the LOD */
  float bmin[3], bmax[3]; /* AABB, read only by the draw-time frustum cull */
  float *mverts;
  int mnverts;
  float merr;       /* worker's mesh, awaiting GL upload */
  double origin[3]; /* per-tile ECEF origin; trans = origin - cam_ecef each frame */
#if W3_LOD_NOKEY
  int texpx[2]; /* proof control: the size stored in tex[mode][0], to detect
                 * a size change and force the old re-bake */
#endif
  int want_yoff;
} w3_cent; /* this WAIT was the origin-elevation probe */
static w3_cent *w3_cache = 0;
static int w3_cache_grow(void) {   /* returns index of the first NEW (FREE) slot, or -1 on OOM */
  int nn = w3_cache_n ? w3_cache_n * 2 : 512;
  w3_cent *nc = (w3_cent *)realloc(w3_cache, (size_t)nn * sizeof(w3_cent));
  if (!nc) return -1;
  memset(nc + w3_cache_n, 0, (size_t)(nn - w3_cache_n) * sizeof(w3_cent));
  int first = w3_cache_n;
  w3_cache = nc;
  w3_cache_n = nn;
  return first;
}
static unsigned w3_touch = 0;

/* The ONE place a resident texture or VBO is freed, in both directions:
 *  - a chunk the tree did NOT touch this pass is unreachable -> free it whole. `touch` is stamped
 * by w3_cache_get, so "not asked for" and "not reachable" are the same question, answered by the
 *    tree. This is DISTANCE-based (the walk covers the 360 deg ring in range), NOT view-based: a
 *    chunk turned out of frame but still near keeps its textures, so turning back is instant.
 *  - a chunk the tree DID touch, but now at a lower SSE target than before (it is receding), frees
 *    its ramp steps ABOVE want_lod_max -> VRAM tracks the resolution actually needed, symmetric
 * with w3_climb building it up. The floor (l=0) is never above any target, so a drawable tile never
 *    loses its last texture here.
 *
 * Each state frees exactly what it owns. A WAIT slot owns nothing yet -- its worker request may
 * still be in flight; when the reply lands, w3_worker_mesh finds no matching WAIT slot (this freed
 * it) and frees the verts there. A MESH slot owns heap verts the worker delivered and the GPU
 * never received. A READY slot owns GL objects. Miss one and it leaks silently. */
/* Bytes a single READY slot holds on the GPU -- both albedos, every resident ramp step. Shared by
 * w3_texvram (the total) and the eviction loop (the delta a chosen victim frees). */
static long w3_slot_bytes(const w3_cent *c) {
  long b = 0;
  for (int m = 0; m < 2; m++)
    for (int l = 0; l < W3_NLOD; l++)
      if (c->tex[m][l]) {
        long s = w3_lod_px[l];
        b += s * s * 3 * 4 / 3;
      }
  return b;
}

/* Real resident texture VRAM in bytes -- summed from what is actually on the GPU, not estimated.
 * This is the number w3_cache_trim compares against w3_vram_budget to decide whether eviction runs
 * at all. */
static long w3_texvram(void) {
  long b = 0;
  for (int i = 0; i < w3_cache_n; i++)
    if (w3_cache[i].state == W3_SLOT_READY) b += w3_slot_bytes(&w3_cache[i]);
  return b;
}

static void w3_cache_trim(unsigned mark) {
  for (int i = 0; i < w3_cache_n; i++) {
    w3_cent *c = &w3_cache[i];
    if (c->state == W3_SLOT_FREE) continue;
    if (c->touch >= mark) {
      c->stale = 0;
      /* touched: keep the chunk, but release ramp steps finer than it now justifies */
      if (c->state == W3_SLOT_READY)
        for (int m = 0; m < 2; m++)
          for (int l = c->want_lod_max + 1; l < W3_NLOD; l++)
            if (c->tex[m][l]) {
              glDeleteTextures(1, &c->tex[m][l]);
              c->tex[m][l] = 0;
            }
      continue;
    }
    c->stale++; /* unasked this pass -- eligible for eviction once > GRACE, see below */
  }

  /* Eviction is PRESSURE-driven, not time-driven: a chunk past GRACE stays resident as long as VRAM
   * is under budget -- a closed loiter ring then converges to zero re-bakes after the first lap
   * (every ring-side chunk ages past GRACE every lap, but nothing evicts while under budget). Only
   * real pressure frees anything, and it frees the STALEST chunk first, one at a time, stopping the
   * moment the budget is met again -- never the whole expired set in one pass. */
  long vram = w3_texvram();
  while (vram > w3_vram_budget) {
    int victim = -1, victim_stale = W3_TRIM_GRACE;
    for (int i = 0; i < w3_cache_n; i++) {
      w3_cent *c = &w3_cache[i];
      if (c->state == W3_SLOT_FREE || c->stale <= W3_TRIM_GRACE) continue;
      if (c->stale > victim_stale) {
        victim_stale = c->stale;
        victim = i;
      }
    }
    if (victim < 0) break; /* nothing past GRACE left -- over budget, but nothing evictable */
    w3_cent *c = &w3_cache[victim];
    if (c->state == W3_SLOT_READY) {
      vram -= w3_slot_bytes(c);
      glDeleteBuffers(1, &c->vbo);
      for (int m = 0; m < 2; m++)
        for (int l = 0; l < W3_NLOD; l++)
          if (c->tex[m][l]) glDeleteTextures(1, &c->tex[m][l]);
    } else if (c->state == W3_SLOT_MESH) {
      free(c->mverts);
      c->mverts = 0;
    }
    c->state = W3_SLOT_FREE;
    w3_frame.cache_evict++;
  }
}

/* The worker delivered a finished mesh for (z,x,y). Called from JS (the worker's onmessage copied
 * the transferred vertex buffer into `verts`, a main-heap malloc we now OWN). Move the matching
 * WAIT slot to MESH; if the tree stopped asking and trim already freed the slot, there is nobody
 * to hand it to -- free it rather than leak. */
extern "C" EMSCRIPTEN_KEEPALIVE   /* called by NAME from the JS worker glue -> must not mangle */
void w3_worker_mesh(int z, uint32_t x, uint32_t y, float *verts, int nverts, float err, float yoff,
                    double *origin) {
  for (int i = 0; i < w3_cache_n; i++) {
    w3_cent *c = &w3_cache[i];
    if (c->state == W3_SLOT_WAIT && c->z == z && c->x == x && c->y == y) {
      c->mverts = verts;
      c->mnverts = nverts;
      c->merr = err;
      c->state = W3_SLOT_MESH;
      c->origin[0] = origin[0];
      c->origin[1] = origin[1];
      c->origin[2] = origin[2];
      if (c->want_yoff && !w3_O.yoff_set) {
        w3_O.yoff = yoff;
        w3_O.yoff_set = 1;
      }
      return;
    }
  }
  free(verts); /* no taker: the tile was trimmed while its build was in flight */
}
/* The worker could not build (a real 204 hole, or it gave up after retries). Free the WAIT slot so
 * the tree either re-asks next frame or leaves it absent -- the same "retry, never silently hole"
 * the sync path had. */
extern "C" EMSCRIPTEN_KEEPALIVE
void w3_worker_fail(int z, uint32_t x, uint32_t y) {
  for (int i = 0; i < w3_cache_n; i++) {
    w3_cent *c = &w3_cache[i];
    if (c->state == W3_SLOT_WAIT && c->z == z && c->x == x && c->y == y) {
      c->state = W3_SLOT_FREE;
      return;
    }
  }
}

/* Bake tex[mode][lod] for (z,x,y) at size TS if it is not already resident. Returns:
 *   1  present (already there, or baked just now)
 *   0  the server has nothing here -- a genuine hole (only meaningful for PHOTO -> photo_none)
 *  -1  in flight (or momentarily undecodable): ask again next frame
 * This is the ONE place a tile texture is created, mirroring w3_cache_trim as the one place it is
 * freed. The size is TS; the STORAGE slot is W3_LODIDX(lod) -- the two differ only under the
 * W3_LOD_NOKEY control, where every size collapses onto slot 0 and a size change evicts first,
 * reproducing the old re-bake-every-frame thrash on purpose. */
static int w3_ensure_tex(w3_cent *c, int mode, int z, uint32_t x, uint32_t y, int TS, int lod) {
  int idx = W3_LODIDX(lod);
#if W3_LOD_NOKEY
  if (c->tex[mode][0] && c->texpx[mode] != TS) {
    glDeleteTextures(1, &c->tex[mode][0]);
    c->tex[mode][0] = 0;
  }
#endif
  if (c->tex[mode][idx]) return 1;
  int n = w3_bake_size(mode == W3_GROUND_PHOTO, (int)z, (int)x, (int)y, TS);
  if (n < 0) return -1;
  if (n == 0) return 0;
  if (w3_bake_budget <= 0) return -1; /* data is ready, but this frame's upload budget is spent */
  GLuint t = w3_bake(z, x, y, TS, mode);
  if (!t) return -1; /* undecodable: treat as in flight, keep asking */
  w3_bake_budget--;
  c->tex[mode][idx] = t;
#if W3_LOD_NOKEY
  c->texpx[mode] = TS;
#endif
  w3_frame.cache_bakes++;
  return 1;
}

/* The GL texture to DRAW for this tile at the wanted step: the wanted size if resident, else the
 * nearest resident FINER size (never blurrier than asked), else the nearest coarser, else 0. A
 * tile can be READY with only one size baked -- the others land on later frames -- so the draw must
 * always have something to bind rather than a hole. */
static GLuint w3_pick_lod(const w3_cent *c, int mode, int lod) {
  for (int l = lod; l < W3_NLOD; l++)
    if (c->tex[mode][l]) return c->tex[mode][l];
  for (int l = lod - 1; l >= 0; l--)
    if (c->tex[mode][l]) return c->tex[mode][l];
  return 0;
}

/* Progressive refinement: climb the ramp one step at a time toward `target`. The floor (256) is
 * already resident when this runs (the MESH gate ensures it), so the tile is drawable the whole
 * time -- each call requests only the NEXT step above the highest one present. That lets the cheap
 * sizes show first and the expensive ones (a fresh 2048 is 7-20 s) land in the background without
 * ever blocking the image. Sequential, not all-at-once: a 2048 bake is never kicked off before the
 * 512 it supersedes is even shown, and the in-flight set stays small (the JS side caps at 256).
 * Returns 1 when OSM has reached `target`, 0 while still climbing -- the caller counts a 0 as
 * w3_frame.sharpen (drawable, not yet sharp), NEVER as w3_frame.pending, so climbing does not hold
 * the gate. */
static int w3_climb(w3_cent *c, int z, uint32_t x, uint32_t y, int target) {
  int hi = -1;
  for (int l = 0; l <= target; l++)
    if (c->tex[W3_GROUND_OSM][l]) hi = l;
  if (hi >= target) {
    /* OSM at target: let PHOTO catch up to the same step (opportunistic, never blocks the gate). */
    if (!c->photo_none && !c->tex[W3_GROUND_PHOTO][target]) {
      int p = w3_ensure_tex(c, W3_GROUND_PHOTO, z, x, y, w3_lod_px[target], target);
      if (p == 0)
        c->photo_none = 1;
      else if (p < 0)
        return 0; /* top photo step still fetching */
    }
    return 1;
  }
  int next = hi + 1, TS = w3_lod_px[next];
  w3_ensure_tex(c, W3_GROUND_OSM, z, x, y, TS, next); /* -1 pending is the normal case here */
  if (!c->photo_none) {
    int p = w3_ensure_tex(c, W3_GROUND_PHOTO, z, x, y, TS, next);
    if (p == 0) c->photo_none = 1;
  }
  return 0;
}

/* Return a READY (drawable) slot for (z,x,y) at texture step `lod`, or -1 if not ready yet. NEVER
 * blocks.
 *
 * The expensive work -- fetch DEM+MVT, decode, build the mesh (measured: 76 % of the per-tile
 * cost, ~23 ms) -- runs OFF the main thread now (tileworker.c). So this no longer DOES that work;
 * it drives the per-slot state machine and posts to the worker:
 *   not resident -> claim a slot, post to the worker, go WAIT, return -1
 *   WAIT         -> worker still building; return -1 WITHOUT re-posting (WAIT is the dedup)
 *   MESH         -> verts delivered; when the FLOOR albedo (256) is here too, upload the VBO, go
 *                   READY at the floor and start climbing toward `lod` -- return the slot
 *   READY        -> drawable; climb one ramp step toward `lod` (progressive), return the slot
 *
 * Progressive by design: a tile becomes drawable at the cheap 256 floor and SHARPENS up the ramp to
 * its SSE target `lod` over later frames (w3_climb). So the readiness gate is the FLOOR, not the
 * target -- a fresh 2048 (7-20 s) never blocks the first image. `lod` is part of what identifies a
 * texture (tex[mode][lod]), never a size passed beside an unkeyed one, so the several steps a chunk
 * climbs through sit side by side, not re-baked over each other. want_lod_max records the target so
 * trim can release steps above it once the chunk recedes. */
static int w3_cache_get(int z, uint32_t x, uint32_t y, int lod, int is_centre) {
  int slot = -1;
  for (int i = 0; i < w3_cache_n; i++)
    if (w3_cache[i].state != W3_SLOT_FREE && w3_cache[i].z == z && w3_cache[i].x == x &&
        w3_cache[i].y == y) {
      slot = i;
      break;
    }

  if (slot < 0) {
    /* Not resident: claim a FREE slot (grace-expired slots recycle each pass) or grow the cache. */
    for (int i = 0; i < w3_cache_n; i++)
      if (w3_cache[i].state == W3_SLOT_FREE) {
        slot = i;
        break;
      }
    if (slot < 0) {
      /* No FREE slot: steal the stalest READY chunk the walk is not using this pass (stale>0).
       * This is the LRU bound that keeps a long straight flight from pinning the cache full of
       * grace-period chunks behind the aircraft. Never steal WAIT (worker in flight) or MESH
       * (delivery in hand) -- both are transient and about to matter. */
      slot = w3_cache_grow();   /* demand-scaled: no free slot means the working set grew */
    }
    if (slot < 0) {
      w3_frame.pending++;
      return -1;
    }
    w3_cent *c = &w3_cache[slot];
    c->z = z;
    c->x = x;
    c->y = y;
    c->state = W3_SLOT_WAIT;
    c->touch = ++w3_touch;
    c->stale = 0;
    c->want_lod_max = lod;
    c->photo_none = 0;
    memset(c->tex, 0, sizeof c->tex);
    c->mverts = 0;
    c->vbo = 0;
#if W3_LOD_NOKEY
    c->texpx[0] = c->texpx[1] = 0;
#endif
    /* Preserve the original yoff condition EXACTLY (is_centre && z==W3_MAXZ && !set): the worker
     * computes the origin ground elevation only when asked, and w3_worker_mesh applies it. */
    c->want_yoff = (is_centre && z == W3_MAXZ && !w3_O.yoff_set);
    w3_worker_post(z, (int)x, (int)y, W3_TERR, c->want_yoff);
    w3_frame.pending++;
    return -1;
  }

  w3_cent *c = &w3_cache[slot];
  int fresh = (c->touch < w3_frame.pass_mark); /* first touch this pass? */
  c->touch = ++w3_touch;
  if (fresh || lod > c->want_lod_max)
    c->want_lod_max = lod; /* per-pass MAX of the requested target */

  if (c->state == W3_SLOT_WAIT) {
    w3_frame.pending++;
    return -1;
  } /* worker still building */

  if (c->state == W3_SLOT_MESH) {
    /* Mesh in hand. Gate drawability on the FLOOR albedo (256) -- cheap and fast, so the tile
     * shows almost at once and sharpens later. Upload the VBO only once that texture is in hand, so
     * a slot never holds a VBO with no texture (a placeholder square over Grohnde is the bug). */
    int r = w3_ensure_tex(c, W3_GROUND_OSM, z, x, y, w3_lod_px[0], 0);
    if (r <= 0) {
      w3_frame.pending++;
      return -1;
    } /* floor in flight: keep asking */
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)((size_t)c->mnverts * sizeof(w3_vtx)), c->mverts,
                 GL_STATIC_DRAW);
    /* AABB from the mesh: the skirt only lowers bmin -> the box grows, never over-culls. */
    {
      const w3_vtx *v = (const w3_vtx *)c->mverts;
      c->bmin[0] = c->bmax[0] = v[0].pos[0];
      c->bmin[1] = c->bmax[1] = v[0].pos[1];
      c->bmin[2] = c->bmax[2] = v[0].pos[2];
      for (int k = 1; k < c->mnverts; k++)
        for (int a = 0; a < 3; a++) {
          float p = v[k].pos[a];
          if (p < c->bmin[a]) c->bmin[a] = p;
          if (p > c->bmax[a]) c->bmax[a] = p;
        }
    }
    free(c->mverts);
    c->mverts = 0;
    c->vbo = vbo;
    c->nverts = c->mnverts;
    c->err = c->merr;
    {
      int p = w3_ensure_tex(c, W3_GROUND_PHOTO, z, x, y, w3_lod_px[0],
                            0); /* photo floor; OSM covers if -1 */
      if (p == 0) c->photo_none = 1;
    }
    c->state = W3_SLOT_READY;
    if (!w3_climb(c, z, x, y, lod)) w3_frame.sharpen++; /* start the climb toward the target */
    return slot;
  }

  /* READY: drawable. Climb one step toward the target if not there yet -- side by side with the
   * steps already resident, never re-baked over them. A chunk below its target is w3_frame.sharpen,
   * not w3_frame.pending: it is on screen, just not yet at full resolution. */
  w3_frame.cache_hits++;
  if (!w3_climb(c, z, x, y, lod)) w3_frame.sharpen++;
  return slot;
}
#endif
