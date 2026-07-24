/* FlightBox renderer — PRIORITY refinement (chunked-LOD, Cesium-style): each chunk draws itself or
 * its four children, never both; the split budget is spent on the highest FRUSTUM-WEIGHTED
 * screen-space error first. Replaces the recursive Ulrich walk, which spent the budget in root-ring
 * ITERATION order (north-west first, no view awareness) — looking south at the Alps, the visible
 * high-error chunks systematically lost the budget race to invisible northern chunks ("heading 122,
 * alles blau"). The comment promised worst-error-first; now the code does it.
 *
 * The frontier, the draw list and the tile cache are all DYNAMIC (realloc, demand-scaled): view
 * distance is a runtime setting (w3_view_m), not a compile-time cap. A safety ceiling bounds
 * pathological runaway, far above any real demand. */
#ifndef W3_TILES_WALK_H
#define W3_TILES_WALK_H
#ifndef W3_TRACE
#define W3_TRACE 0 /* 1 = log every stream pass */
#endif

#define W3_LEAF_CEIL 4096 /* runaway backstop on leaves per pass, not a tuning knob */

/* Last frame's frustum + camera, stored by w3_draw_terrain: refinement prioritises what the camera
 * actually sees (1-frame stale is fine). Out-of-frustum chunks refine at 5% priority — never
 * starved, so turning the head does not start from scratch. */
static w3_frustum w3_cull_fr;
static double w3_cull_cam[3];
static int w3_cull_valid = 0;

/* Can this chunk be replaced by its four children right now? Asks for them either way -- that ask
 * is what starts the fetch, in refinement-priority order. */
static int w3_children_ready(int z, uint32_t x, uint32_t y, int ci[4], double lat, double alt,
                             double ctx, double cty) {
  int ok = 1;
  for (int q = 0; q < 4; q++) {
    long cx = x * 2 + (q & 1), cy = y * 2 + (q >> 1);
    int lod = w3_lod_for_px(w3_chunk_px(z + 1, cx, cy, lat, alt, ctx, cty));
    if (W3_LOD_INJECT) lod = (lod + 1 < w3_lod_cap) ? lod + 1 : lod - 1;
    if (lod < 0) lod = 0;
    ci[q] = w3_cache_get(z + 1, (uint32_t)cx, (uint32_t)cy, lod, 0);
    if (ci[q] < 0) ok = 0; /* still in flight: keep drawing the parent */
  }
  return ok;
}

static void w3_emit(int ci, int lod) {
  if (ci < 0) return;
  if (w3_frame.nD >= w3_D_cap) { /* draw list scales on demand */
    int ncap = w3_D_cap ? w3_D_cap * 2 : 256;
    w3_tileGL *nd = (w3_tileGL *)realloc(w3_D, (size_t)ncap * sizeof(w3_tileGL));
    if (!nd) { w3_frame.over++; return; }
    w3_D = nd;
    w3_D_cap = ncap;
  }
  {
    int L = w3_cache[ci].z - W3_ROOTZ;
    if (L >= 0 && L < 8) w3_frame.lvl[L]++;
  }
  w3_D[w3_frame.nD].vbo = w3_cache[ci].vbo;
  w3_D[w3_frame.nD].nverts = w3_cache[ci].nverts;
  w3_D[w3_frame.nD].tex[0] = w3_pick_lod(&w3_cache[ci], W3_GROUND_OSM, lod);
  w3_D[w3_frame.nD].tex[1] = w3_pick_lod(&w3_cache[ci], W3_GROUND_PHOTO, lod);
  for (int a = 0; a < 3; a++) {
    w3_D[w3_frame.nD].bmin[a] = w3_cache[ci].bmin[a];
    w3_D[w3_frame.nD].bmax[a] = w3_cache[ci].bmax[a];
    w3_D[w3_frame.nD].origin[a] = w3_cache[ci].origin[a];
  }
  w3_frame.nD++;
}

/* ---- the refinement frontier -------------------------------------------------------------- */
typedef struct {
  int z;
  long x, y;
  double tx, ty; /* camera's fractional tile coord at THIS level */
  int ci, lod;
  float eff;          /* frustum-weighted SSE — the refinement priority */
  unsigned char want; /* wants to split (sse > eps, z < max) */
  unsigned char tried; /* children requested this pass but not ready — skip, don't re-pick */
} w3_leaf_t;
static w3_leaf_t *w3_fr = 0;
static int w3_fr_n = 0, w3_fr_cap = 0;

/* Eligibility only: map bounds + view distance, no cache access, no side effects. Used to decide
 * whether a SPLIT is allowed at all -- view distance may only PREVENT a split (parent keeps drawing
 * as the leaf), it must never be the reason a quadrant loses coverage. */
static int w3_leaf_viable(int z, long x, long y, double tx, double ty, double lat) {
  long n = 1L << z;
  if (x < 0 || y < 0 || x >= n || y >= n) return 0; /* off the map: a real hole, not a gap */
  double span = w3_tile_span(z, lat);
  double dx = fabs(tx - ((double)x + 0.5)) - 0.5, dy = fabs(ty - ((double)y + 0.5)) - 0.5;
  if (dx < 0) dx = 0;
  if (dy < 0) dy = 0;
  double horiz = sqrt(dx * dx + dy * dy) * span;
  if (horiz - span * 0.71 > w3_view_m) return 0; /* past the configured view distance */
  return 1;
}

/* Evaluate a chunk into a leaf entry: distance, texture step, residency (the get also REQUESTS a
 * missing tile), SSE, and the frustum weight. Returns 0 if off-map or past the view distance. */
static int w3_leaf_eval(w3_leaf_t *f, int z, long x, long y, double tx, double ty, double lat,
                        double alt) {
  if (!w3_leaf_viable(z, x, y, tx, ty, lat)) return 0;
  double span = w3_tile_span(z, lat);
  double dx = fabs(tx - ((double)x + 0.5)) - 0.5, dy = fabs(ty - ((double)y + 0.5)) - 0.5;
  if (dx < 0) dx = 0;
  if (dy < 0) dy = 0;
  double horiz = sqrt(dx * dx + dy * dy) * span;
  double dist = sqrt(horiz * horiz + alt * alt);
  if (dist < 1.0) dist = 1.0;
  f->z = z;
  f->x = x;
  f->y = y;
  f->tx = tx;
  f->ty = ty;
  f->lod = w3_lod_for_px(span * (double)W3_SSE_K / dist);
  f->ci = w3_cache_get(z, (uint32_t)x, (uint32_t)y, f->lod, (z == W3_ROOTZ));
  f->tried = 0;
  f->want = 0;
  f->eff = 0;
  if (f->ci < 0) return 1; /* requested; drawable/splittable on a later pass */
  float sse = w3_cache[f->ci].err * W3_SSE_K / (float)dist;
  f->want = (z < W3_MAXZ && sse > W3_EPS);
  if (f->want) w3_frame.split_want++;
  /* frustum weight: what the camera sees refines first; the rest trickles at 5% priority */
  float w = 1.0f;
  if (w3_cull_valid) {
    float bmn[3], bmx[3];
    for (int a = 0; a < 3; a++) {
      float t = (float)(w3_cache[f->ci].origin[a] - w3_cull_cam[a]);
      bmn[a] = w3_cache[f->ci].bmin[a] + t;
      bmx[a] = w3_cache[f->ci].bmax[a] + t;
    }
    if (!w3_aabb_visible(&w3_cull_fr, bmn, bmx)) w = 0.05f;
  }
  f->eff = sse * w;
  return 1;
}

static void w3_leaf_push(int z, long x, long y, double tx, double ty, double lat, double alt) {
  if (w3_fr_n >= w3_fr_cap) {
    int ncap = w3_fr_cap ? w3_fr_cap * 2 : 512;
    w3_leaf_t *nf = (w3_leaf_t *)realloc(w3_fr, (size_t)ncap * sizeof(w3_leaf_t));
    if (!nf) return;
    w3_fr = nf;
    w3_fr_cap = ncap;
  }
  if (w3_leaf_eval(&w3_fr[w3_fr_n], z, x, y, tx, ty, lat, alt)) w3_fr_n++;
}

/* Refine the frontier: repeatedly split the highest-priority splittable leaf until nothing wants to
 * split (SSE <= eps everywhere), children are pending, or the runaway ceiling. Then emit leaves. */
static void w3_refine(double lat, double alt) {
  for (;;) {
    int best = -1;
    float be = 0;
    for (int i = 0; i < w3_fr_n; i++)
      if (w3_fr[i].want && !w3_fr[i].tried && w3_fr[i].eff > be) {
        be = w3_fr[i].eff;
        best = i;
      }
    if (best < 0) break;
    if (w3_fr_n + 3 > W3_LEAF_CEIL) {
      w3_frame.over++;
      break;
    }
    w3_leaf_t f = w3_fr[best];
    double ctx = f.tx * 2.0, cty = f.ty * 2.0;
    /* view distance sheds DETAIL, never coverage: if any child would fall off-map or past the view
     * distance, the parent must keep drawing as the leaf -- do not split, do not touch the slot. */
    int all_viable = 1;
    for (int q = 0; q < 4; q++) {
      long cx = f.x * 2 + (q & 1), cy = f.y * 2 + (q >> 1);
      if (!w3_leaf_viable(f.z + 1, cx, cy, ctx, cty, lat)) { all_viable = 0; break; }
    }
    if (!all_viable) {
      w3_fr[best].tried = 1;
      continue;
    }
    int cc[4];
    if (!w3_children_ready(f.z, (uint32_t)f.x, (uint32_t)f.y, cc, lat, alt, ctx, cty)) {
      w3_fr[best].tried = 1;
      w3_frame.split_wait++;
      w3_ground.dirty = 1; /* keep the streamer awake for them */
      continue;
    }
    /* replace the parent leaf with its four children (overwrite + append three) */
    int slot = best;
    for (int q = 0; q < 4; q++) {
      long cx = f.x * 2 + (q & 1), cy = f.y * 2 + (q >> 1);
      if (q == 0) {
        if (!w3_leaf_eval(&w3_fr[slot], f.z + 1, cx, cy, ctx, cty, lat, alt)) {
          /* unreachable in practice: all_viable already confirmed bounds + view distance above.
           * Defensive backstop only, kept inert rather than asserting on a production render path. */
          w3_fr[slot].want = 0;
          w3_fr[slot].tried = 1;
          w3_fr[slot].ci = -1;
        }
      } else
        w3_leaf_push(f.z + 1, cx, cy, ctx, cty, lat, alt);
    }
  }
  for (int i = 0; i < w3_fr_n; i++)
    if (w3_fr[i].ci >= 0) w3_emit(w3_fr[i].ci, w3_fr[i].lod);
}
#endif
