/* libosmmesh/src/building_emit.c
 *
 * Mesh-emission helpers used by the T15+ roof builders. Direct port of
 * streetwalk/tools/building/builders/{extrude,roofs/_emit}.py.
 *
 * Axis mapping Python -> C:
 *   Python xz plane  <->  C xy plane   (horizontal ENU, interleaved)
 *   Python y axis    <->  C z axis     (vertical, per-vertex)
 *
 * Every face-up check (Python `ny < 0`) becomes `nz < 0` here.
 * Every horizontal normal component (Python `(onx, 0, onz)`) becomes
 * `(onx, ony, 0)`.
 */

#include "building_emit.h"

#include "building_internal.h"       /* osmmesh_building_ear_clip */

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ==========================================================================
 *  Buffer management
 * ========================================================================== */

void osmmesh_emit_buf_init(osmmesh_emit_buf *b)
{
    if (!b) return;
    memset(b, 0, sizeof *b);
}

void osmmesh_emit_buf_free(osmmesh_emit_buf *b)
{
    if (!b) return;
    free(b->positions);
    free(b->normals);
    free(b->indices);
    memset(b, 0, sizeof *b);
}

int osmmesh_emit_buf_reserve(osmmesh_emit_buf *b,
                              uint32_t extra_nv, uint32_t extra_nt)
{
    if (!b) return -1;

    uint32_t need_v = b->nv + extra_nv;
    if (need_v > b->nv_cap) {
        uint32_t cap = b->nv_cap ? b->nv_cap : 16;
        while (cap < need_v) cap *= 2;
        float *pos = (float *)realloc(b->positions, (size_t)cap * 3 * sizeof(float));
        if (!pos) return -1;
        b->positions = pos;
        float *nrm = (float *)realloc(b->normals, (size_t)cap * 3 * sizeof(float));
        if (!nrm) return -1;
        b->normals = nrm;
        b->nv_cap = cap;
    }

    uint32_t need_t = b->nt + extra_nt;
    if (need_t > b->nt_cap) {
        uint32_t cap = b->nt_cap ? b->nt_cap : 16;
        while (cap < need_t) cap *= 2;
        uint32_t *idx = (uint32_t *)realloc(b->indices, (size_t)cap * 3 * sizeof(uint32_t));
        if (!idx) return -1;
        b->indices = idx;
        b->nt_cap = cap;
    }
    return 0;
}

/* Append one vertex; returns its index. Caller guarantees capacity. */
static uint32_t emit_vtx(osmmesh_emit_buf *b,
                          float x, float y, float z,
                          float nx, float ny, float nz)
{
    uint32_t i = b->nv++;
    b->positions[i * 3 + 0] = x;
    b->positions[i * 3 + 1] = y;
    b->positions[i * 3 + 2] = z;
    b->normals  [i * 3 + 0] = nx;
    b->normals  [i * 3 + 1] = ny;
    b->normals  [i * 3 + 2] = nz;
    return i;
}

static void emit_tri(osmmesh_emit_buf *b,
                      uint32_t a, uint32_t bi, uint32_t c)
{
    uint32_t t = b->nt++;
    b->indices[t * 3 + 0] = a;
    b->indices[t * 3 + 1] = bi;
    b->indices[t * 3 + 2] = c;
}

/* ==========================================================================
 *  Vector helpers
 * ========================================================================== */

static void face_normal(float ax, float ay, float az,
                         float bx, float by, float bz,
                         float cx, float cy, float cz,
                         float out[3])
{
    float e1x = bx - ax, e1y = by - ay, e1z = bz - az;
    float e2x = cx - ax, e2y = cy - ay, e2z = cz - az;
    out[0] = e1y * e2z - e1z * e2y;
    out[1] = e1z * e2x - e1x * e2z;
    out[2] = e1x * e2y - e1y * e2x;
    float len = sqrtf(out[0] * out[0] + out[1] * out[1] + out[2] * out[2]);
    if (len < 1e-9f) { out[0] = 0.0f; out[1] = 0.0f; out[2] = 1.0f; }
    else             { out[0] /= len; out[1] /= len; out[2] /= len; }
}

/* Emit a face-up triangle: winding chosen so the stored normal AND the
 * triangle winding agree on "nz >= 0". */
static int push_tri_face_up(osmmesh_emit_buf *b,
                             float p0x, float p0y, float p0z,
                             float p1x, float p1y, float p1z,
                             float p2x, float p2y, float p2z)
{
    float n[3];
    face_normal(p0x, p0y, p0z, p1x, p1y, p1z, p2x, p2y, p2z, n);
    if (n[2] < 0.0f) {
        /* Swap p1 and p2; flip stored normal. */
        float tx = p1x, ty = p1y, tz = p1z;
        p1x = p2x; p1y = p2y; p1z = p2z;
        p2x = tx;  p2y = ty;  p2z = tz;
        n[0] = -n[0]; n[1] = -n[1]; n[2] = -n[2];
    }
    if (osmmesh_emit_buf_reserve(b, 3, 1) != 0) return -1;
    uint32_t i0 = emit_vtx(b, p0x, p0y, p0z, n[0], n[1], n[2]);
    uint32_t i1 = emit_vtx(b, p1x, p1y, p1z, n[0], n[1], n[2]);
    uint32_t i2 = emit_vtx(b, p2x, p2y, p2z, n[0], n[1], n[2]);
    emit_tri(b, i0, i1, i2);
    return 0;
}

/* ==========================================================================
 *  Walls (below eaves)
 *
 *  One quad per footprint edge: bottom at per-vertex z_bot, top at the
 *  flat eaves plane y_eaves. Matches streetwalk extrude.py::_walls
 *  (extended with per-vertex bottoms). The outward normal is picked
 *  from the ring orientation; triangle winding matches so face+vertex
 *  normals agree.
 *
 *  For a CCW ring in the xy plane with z-up, the outward normal of the
 *  edge (a -> b) is rotate-90-RIGHT of the edge direction:
 *     n_out = ( dy, -dx) / |edge|    (CCW source)
 *  For a CW ring, it is rotate-90-LEFT:
 *     n_out = (-dy,  dx) / |edge|    (CW source)
 *  Equivalently: n_out = ndir * ( dy, -dx) / |edge|, ndir = +1 (CCW) / -1 (CW).
 *
 *  Emit vertices BL, BR, TR, TL. Triangle winding picked so the face
 *  normal of (BL, TR, BR) matches n_out for CCW; the mirror for CW.
 * ========================================================================== */

int osmmesh_emit_walls_below_eaves(osmmesh_emit_buf *b,
                                     const float *xy, uint32_t n,
                                     const float *z_bot_per_v,
                                     float        y_eaves,
                                     int ccw)
{
    if (!b || !xy || !z_bot_per_v || n < 3) return 0;
    float ndir = ccw ? 1.0f : -1.0f;

    for (uint32_t i = 0; i < n; ++i) {
        uint32_t j = (i + 1) % n;
        float ax = xy[i * 2 + 0], ay = xy[i * 2 + 1];
        float bx = xy[j * 2 + 0], by = xy[j * 2 + 1];
        float dx = bx - ax, dy = by - ay;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 1e-6f) continue;

        float onx =  ndir * dy / len;
        float ony = -ndir * dx / len;

        float z_a = z_bot_per_v[i];
        float z_b = z_bot_per_v[j];
        /* Clamp below eaves — wall corners must not poke through. */
        if (z_a > y_eaves - 1e-3f) z_a = y_eaves - 1e-3f;
        if (z_b > y_eaves - 1e-3f) z_b = y_eaves - 1e-3f;

        if (osmmesh_emit_buf_reserve(b, 4, 2) != 0) return -1;
        uint32_t bl = emit_vtx(b, ax, ay, z_a,     onx, ony, 0.0f);
        uint32_t br = emit_vtx(b, bx, by, z_b,     onx, ony, 0.0f);
        uint32_t tr = emit_vtx(b, bx, by, y_eaves, onx, ony, 0.0f);
        uint32_t tl = emit_vtx(b, ax, ay, y_eaves, onx, ony, 0.0f);

        /* Winding derivation (detailed derivation in building.c header):
         * for a CCW ring, tri (BL, BR, TR) has face-normal
         * (dy, -dx, 0) * (y_eaves - z_b), matching outward for z_b <
         * y_eaves. Mirror for CW. */
        if (ccw) {
            emit_tri(b, bl, br, tr);
            emit_tri(b, bl, tr, tl);
        } else {
            emit_tri(b, bl, tr, br);
            emit_tri(b, bl, tl, tr);
        }
    }
    return 0;
}

/* ==========================================================================
 *  emit_lifted_ring
 *
 *  Ear-clip the xy ring, lift each vertex by its per-vertex z. Flat
 *  shading: per-triangle normals, separate vertex instances.
 * ========================================================================== */

int osmmesh_emit_lifted_ring(osmmesh_emit_buf *b,
                              const float *xy, uint32_t n,
                              const float *z_per_vertex,
                              int ccw)
{
    if (!b || !xy || !z_per_vertex || n < 3) return 0;
    if (n > OSMMESH_BUILDING_MAX_VERTS) return 0;
    (void)ccw;  /* Winding fixed by push_tri_face_up via nz sign. */

    /* Our ear-clipper emits ears matching the given orientation side.
     * Pick that side from the signed area of the ring itself, so CW /
     * CCW inputs both clip correctly. push_tri_face_up then re-orients
     * the emitted triangle so its face normal points +Z. */
    double sa = 0.0;
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t j = (i + 1) % n;
        sa += (double)xy[i * 2 + 0] * (double)xy[j * 2 + 1]
            - (double)xy[j * 2 + 0] * (double)xy[i * 2 + 1];
    }
    int clip_ccw = (sa > 0.0) ? 1 : 0;

    uint32_t tris[3 * (OSMMESH_BUILDING_MAX_VERTS - 2)];
    uint32_t nt = osmmesh_building_ear_clip(xy, n, clip_ccw, tris);
    if (nt == 0) return 0;

    for (uint32_t k = 0; k < nt; ++k) {
        uint32_t ia = tris[k * 3 + 0];
        uint32_t ib = tris[k * 3 + 1];
        uint32_t ic = tris[k * 3 + 2];
        float p0x = xy[ia * 2 + 0], p0y = xy[ia * 2 + 1], p0z = z_per_vertex[ia];
        float p1x = xy[ib * 2 + 0], p1y = xy[ib * 2 + 1], p1z = z_per_vertex[ib];
        float p2x = xy[ic * 2 + 0], p2y = xy[ic * 2 + 1], p2z = z_per_vertex[ic];
        if (push_tri_face_up(b, p0x, p0y, p0z, p1x, p1y, p1z, p2x, p2y, p2z) != 0)
            return -1;
    }
    return 0;
}

/* ==========================================================================
 *  emit_wall_fill (above eaves)
 *
 *  Per-edge fill ABOVE the eaves plane. For each edge:
 *    - if both endpoints sit at y_eaves and there's no cut: skip.
 *    - if one endpoint is at y_eaves and the other is lifted: single
 *      triangle fill.
 *    - if both endpoints are lifted: trapezoid (two triangles).
 *    - if a cut line is supplied and the edge crosses it: emit one
 *      gable-end triangle (a, b, intersection@y_ridge) instead of
 *      the wall_quad (the two low corners sit at the eaves plane).
 *
 *  Direct port of streetwalk's _emit.py::emit_wall_fill.
 * ========================================================================== */

/* Emit a single vertical gable triangle: two low corners at z=y_eaves,
 * one peak at (ix, iy, y_ridge). Uses horizontal outward normal
 * (onx, ony, 0). Winding picked so the geometric normal agrees with the
 * outward normal. */
static int gable_tri(osmmesh_emit_buf *b,
                      float ax, float ay,
                      float bx, float by,
                      float ix, float iy,
                      float y_eaves, float y_ridge,
                      float onx, float ony,
                      int ccw)
{
    /* Triangle corners in Python order. */
    float p0x = ax, p0y = ay, p0z = y_eaves;
    float p1x = bx, p1y = by, p1z = y_eaves;
    float p2x = ix, p2y = iy, p2z = y_ridge;
    if (!ccw) {
        /* Swap p1 and p2 to pre-flip winding — Python's "else" branch. */
        float tx = p1x, ty = p1y, tz = p1z;
        p1x = p2x; p1y = p2y; p1z = p2z;
        p2x = tx;  p2y = ty;  p2z = tz;
    }
    float n[3];
    face_normal(p0x, p0y, p0z, p1x, p1y, p1z, p2x, p2y, p2z, n);
    if (n[0] * onx + n[1] * ony < 0.0f) {
        /* Swap p1 and p2 again; flip normal. */
        float tx = p1x, ty = p1y, tz = p1z;
        p1x = p2x; p1y = p2y; p1z = p2z;
        p2x = tx;  p2y = ty;  p2z = tz;
        n[0] = -n[0]; n[1] = -n[1]; n[2] = -n[2];
    }
    if (osmmesh_emit_buf_reserve(b, 3, 1) != 0) return -1;
    uint32_t i0 = emit_vtx(b, p0x, p0y, p0z, n[0], n[1], n[2]);
    uint32_t i1 = emit_vtx(b, p1x, p1y, p1z, n[0], n[1], n[2]);
    uint32_t i2 = emit_vtx(b, p2x, p2y, p2z, n[0], n[1], n[2]);
    emit_tri(b, i0, i1, i2);
    return 0;
}

/* Non-cut path: fill between eaves edge (a->b at y_eaves) and lifted
 * tops (z_top_a, z_top_b). Matches Python _emit.py::_wall_quad. */
static int wall_quad_above(osmmesh_emit_buf *b,
                            float ax, float ay,
                            float bx, float by,
                            float z_top_a, float z_top_b,
                            float y_eaves,
                            float onx, float ony,
                            int ccw)
{
    /* Skip if both tops are at or below eaves. */
    if (z_top_a <= y_eaves + 1e-4f && z_top_b <= y_eaves + 1e-4f) return 0;
    /* Skip zero-length edge. */
    float dx = bx - ax, dy = by - ay;
    if (dx * dx + dy * dy < 1e-10f) return 0;

    int low_a = (fabsf(z_top_a - y_eaves) < 1e-4f);
    int low_b = (fabsf(z_top_b - y_eaves) < 1e-4f);

    if (low_a && !low_b) {
        /* Triangle: (ax,y_eaves,ay), (bx,y_eaves,by), (bx,z_top_b,by). */
        float p0x = ax, p0y = ay, p0z = y_eaves;
        float p1x = bx, p1y = by, p1z = y_eaves;
        float p2x = bx, p2y = by, p2z = z_top_b;
        float nn[3];
        face_normal(p0x, p0y, p0z, p1x, p1y, p1z, p2x, p2y, p2z, nn);
        if (nn[0] * onx + nn[1] * ony < 0.0f) {
            float tx = p1x, ty = p1y, tz = p1z;
            p1x = p2x; p1y = p2y; p1z = p2z;
            p2x = tx;  p2y = ty;  p2z = tz;
        }
        (void)ccw;
        if (osmmesh_emit_buf_reserve(b, 3, 1) != 0) return -1;
        uint32_t i0 = emit_vtx(b, p0x, p0y, p0z, onx, ony, 0.0f);
        uint32_t i1 = emit_vtx(b, p1x, p1y, p1z, onx, ony, 0.0f);
        uint32_t i2 = emit_vtx(b, p2x, p2y, p2z, onx, ony, 0.0f);
        emit_tri(b, i0, i1, i2);
        return 0;
    }
    if (low_b && !low_a) {
        float p0x = ax, p0y = ay, p0z = y_eaves;
        float p1x = bx, p1y = by, p1z = y_eaves;
        float p2x = ax, p2y = ay, p2z = z_top_a;
        float nn[3];
        face_normal(p0x, p0y, p0z, p1x, p1y, p1z, p2x, p2y, p2z, nn);
        if (nn[0] * onx + nn[1] * ony < 0.0f) {
            float tx = p1x, ty = p1y, tz = p1z;
            p1x = p2x; p1y = p2y; p1z = p2z;
            p2x = tx;  p2y = ty;  p2z = tz;
        }
        if (osmmesh_emit_buf_reserve(b, 3, 1) != 0) return -1;
        uint32_t i0 = emit_vtx(b, p0x, p0y, p0z, onx, ony, 0.0f);
        uint32_t i1 = emit_vtx(b, p1x, p1y, p1z, onx, ony, 0.0f);
        uint32_t i2 = emit_vtx(b, p2x, p2y, p2z, onx, ony, 0.0f);
        emit_tri(b, i0, i1, i2);
        return 0;
    }

    /* Full trapezoid above eaves: BL=(ax,ay,y_eaves), BR=(bx,by,y_eaves),
     * TR=(bx,by,z_top_b), TL=(ax,ay,z_top_a). Same geometric winding as
     * walls_below_eaves: for a CCW source, cross((BR-BL), (TR-BL)) has
     * direction (dy, -dx, 0) * (z_top_b - y_eaves), matching stored
     * outward (dy, -dx, 0) when z_top_b > y_eaves. */
    if (osmmesh_emit_buf_reserve(b, 4, 2) != 0) return -1;
    uint32_t bl = emit_vtx(b, ax, ay, y_eaves, onx, ony, 0.0f);
    uint32_t br = emit_vtx(b, bx, by, y_eaves, onx, ony, 0.0f);
    uint32_t tr = emit_vtx(b, bx, by, z_top_b, onx, ony, 0.0f);
    uint32_t tl = emit_vtx(b, ax, ay, z_top_a, onx, ony, 0.0f);
    if (ccw) {
        emit_tri(b, bl, br, tr);
        emit_tri(b, bl, tr, tl);
    } else {
        emit_tri(b, bl, tr, br);
        emit_tri(b, bl, tl, tr);
    }
    return 0;
}

int osmmesh_emit_wall_fill(osmmesh_emit_buf *b,
                            const float *xy, uint32_t n,
                            const float *z_top_per_v,
                            int ccw,
                            const float *cut_origin,
                            const float *cut_dir,
                            float        y_eaves,
                            float        y_ridge)
{
    if (!b || !xy || !z_top_per_v || n < 3) return 0;

    float ndir = ccw ? 1.0f : -1.0f;
    int   has_cut = (cut_origin != NULL && cut_dir != NULL);
    /* Cut-line normal and origin in double precision — matches
     * polygon_split exactly so both modules classify each edge
     * identically. Tiny float-vs-double differences at the sa sign
     * threshold otherwise produce half-gable-half-wall edges and open
     * seams at the ridge. */
    double nx_cut = 0.0, ny_cut = 0.0;
    double ox_cut = 0.0, oy_cut = 0.0;
    if (has_cut) {
        double dx = cut_dir[0], dy = cut_dir[1];
        double dlen = sqrt(dx * dx + dy * dy);
        if (dlen < 1e-9) { has_cut = 0; }
        else {
            double dxn = dx / dlen, dyn = dy / dlen;
            nx_cut = -dyn;
            ny_cut =  dxn;
            ox_cut = cut_origin[0];
            oy_cut = cut_origin[1];
        }
    }

    for (uint32_t i = 0; i < n; ++i) {
        uint32_t j = (i + 1) % n;
        float ax = xy[i * 2 + 0], ay = xy[i * 2 + 1];
        float bx = xy[j * 2 + 0], by = xy[j * 2 + 1];
        float za = z_top_per_v[i], zb = z_top_per_v[j];

        float dx = bx - ax, dy = by - ay;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 1e-6f) continue;

        float onx =  ndir * dy / len;
        float ony = -ndir * dx / len;

        int have_cut_pt = 0;
        float ix = 0.0f, iy = 0.0f;
        if (has_cut) {
            double sa = ((double)ax - ox_cut) * nx_cut + ((double)ay - oy_cut) * ny_cut;
            double sb = ((double)bx - ox_cut) * nx_cut + ((double)by - oy_cut) * ny_cut;
            if ((sa > 0.0 && sb < 0.0) || (sa < 0.0 && sb > 0.0)) {
                double t = sa / (sa - sb);
                ix = (float)((double)ax + t * ((double)bx - (double)ax));
                iy = (float)((double)ay + t * ((double)by - (double)ay));
                have_cut_pt = 1;
            }
        }

        if (have_cut_pt) {
            if (gable_tri(b, ax, ay, bx, by, ix, iy,
                            y_eaves, y_ridge, onx, ony, ccw) != 0)
                return -1;
        } else {
            if (wall_quad_above(b, ax, ay, bx, by, za, zb,
                                  y_eaves, onx, ony, ccw) != 0)
                return -1;
        }
    }
    return 0;
}

/* ==========================================================================
 *  emit_skeleton_faces
 *
 *  For each face of the skeleton, build a lifted sub-ring with per-vertex
 *  z = y_eaves + (y_ridge - y_eaves) * (t / t_max) and emit it as a
 *  face-up ring.
 * ========================================================================== */

int osmmesh_emit_skeleton_faces(osmmesh_emit_buf *b,
                                 const osmmesh_skeleton *sk,
                                 const float *footprint_xy,
                                 uint32_t footprint_n,
                                 float y_eaves, float y_ridge)
{
    if (!b || !sk) return 0;
    if (sk->t_max < 1e-6f) return 0;
    float dy = y_ridge - y_eaves;
    float t_max = sk->t_max;

    enum { MAX_FACE = OSMMESH_BUILDING_MAX_VERTS };
    float xy[MAX_FACE * 2];
    float z [MAX_FACE];

    for (uint32_t fi = 0; fi < sk->n_faces; ++fi) {
        const osmmesh_skeleton_face *f = &sk->faces[fi];
        uint32_t m = f->n_indices;
        if (m < 3 || m > MAX_FACE) continue;

        for (uint32_t k = 0; k < m; ++k) {
            uint32_t vi = f->indices[k];
            const osmmesh_skeleton_vertex *v = &sk->vertices[vi];
            float vx = v->xy[0], vy = v->xy[1];
            float t01 = v->t / t_max;
            if (t01 < 0.0f) t01 = 0.0f;
            if (t01 > 1.0f) t01 = 1.0f;
            /* Snap t=0 skeleton vertices to the nearest footprint
             * vertex. The skeleton's centre-then-translate pipeline
             * rounds vertex positions at float precision and drifts
             * them away from the exact footprint coord (can be a cm
             * off), which desyncs the wall-top eave edge from the
             * skeleton-face eave edge and leaves open seams. */
            if (v->t < 1e-4f && footprint_xy && footprint_n > 0) {
                float best_d2 = 1e30f;
                float best_x = vx, best_y = vy;
                for (uint32_t j = 0; j < footprint_n; ++j) {
                    float fx = footprint_xy[j * 2 + 0];
                    float fy = footprint_xy[j * 2 + 1];
                    float dxp = fx - vx, dyp = fy - vy;
                    float d2 = dxp * dxp + dyp * dyp;
                    if (d2 < best_d2) {
                        best_d2 = d2;
                        best_x = fx; best_y = fy;
                    }
                }
                /* Only snap if within ~2 cm — otherwise this is a
                 * genuinely non-footprint skeleton vertex. */
                if (best_d2 < 4e-4f) { vx = best_x; vy = best_y; }
            }
            xy[k * 2 + 0] = vx;
            xy[k * 2 + 1] = vy;
            z[k] = y_eaves + dy * t01;
        }

        /* emit_lifted_ring reads the ring orientation from its own signed
         * area — no need to pass a precomputed ccw flag. */
        if (osmmesh_emit_lifted_ring(b, xy, m, z, 1) != 0) return -1;
    }
    return 0;
}
