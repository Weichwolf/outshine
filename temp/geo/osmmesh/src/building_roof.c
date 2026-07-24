/* libosmmesh/src/building_roof.c
 *
 * Integrated wall+roof builder (T15.2 rewrite). Direct ports of
 * streetwalk/tools/building/builders/roofs/{flat,gabled,hipped}.py plus
 * the surrounding dispatch that used to live across building.c's walls
 * and the per-shape roof builders.
 *
 * Key change vs T14: walls and the bordering roof face share their
 * eave-edge vertices by CONSTRUCTION — wall tops and roof eaves both
 * sit on the flat `y_eaves` plane at the same fp.xy[i] coordinates, so
 * the closed-mesh invariant (test_087) holds without a stitch pass.
 *
 * Coordinate convention: ENU meters, xy horizontal, z vertical. Input
 * `fp->xy` is the outer-ring footprint (may be CW or CCW — we read
 * fp->signed_area's sign to decide).
 */

#include "building_internal.h"
#include "building_emit.h"
#include "building_geom.h"
#include "building_skeleton.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ==========================================================================
 *  flat
 *
 *  Ear-clipped cap at y_eaves + walls below.
 * ========================================================================== */

static int build_flat(osmmesh_emit_buf *b,
                       const float *xy, uint32_t n,
                       const float *z_bot_per_v,
                       int ccw, float y_eaves)
{
    if (osmmesh_emit_walls_below_eaves(b, xy, n, z_bot_per_v, y_eaves, ccw) != 0)
        return -1;

    float z_cap[OSMMESH_BUILDING_MAX_VERTS];
    for (uint32_t i = 0; i < n; ++i) z_cap[i] = y_eaves;
    if (osmmesh_emit_lifted_ring(b, xy, n, z_cap, ccw) != 0) return -1;
    return 0;
}

/* ==========================================================================
 *  hipped — straight skeleton + walls below.
 * ========================================================================== */

/* The skeleton algorithm expects a CCW polygon. Flip to a CCW copy if
 * needed. Returns a malloc'd CCW copy; caller frees. */
static float *copy_ccw(const float *xy, uint32_t n, int src_ccw)
{
    float *copy = (float *)malloc((size_t)n * 2 * sizeof(float));
    if (!copy) return NULL;
    if (src_ccw) {
        memcpy(copy, xy, (size_t)n * 2 * sizeof(float));
    } else {
        for (uint32_t i = 0; i < n; ++i) {
            copy[i * 2 + 0] = xy[(n - 1 - i) * 2 + 0];
            copy[i * 2 + 1] = xy[(n - 1 - i) * 2 + 1];
        }
    }
    return copy;
}

/* Returns 0 on success, -1 on OOM, 1 on "skeleton failed — caller
 * should treat as flat". */
static int build_hipped(osmmesh_emit_buf *b,
                         const float *xy, uint32_t n,
                         const float *z_bot_per_v,
                         int ccw, float y_eaves, float roof_h)
{
    float *ccw_xy = copy_ccw(xy, n, ccw);
    if (!ccw_xy) return -1;

    osmmesh_skeleton *sk = osmmesh_skeleton_compute(ccw_xy, n);
    free(ccw_xy);
    if (!sk || sk->t_max < 1e-6f) {
        osmmesh_skeleton_free(sk);
        return 1;
    }

    /* Walls (below eaves). */
    if (osmmesh_emit_walls_below_eaves(b, xy, n, z_bot_per_v, y_eaves, ccw) != 0) {
        osmmesh_skeleton_free(sk);
        return -1;
    }
    /* Skeleton faces (roof slope). Pass the ORIGINAL footprint so t=0
     * skeleton verts are snapped back to exact polygon coords — the
     * skeleton's internal centre-then-translate pipeline otherwise
     * drifts polygon-corner positions by ~1 cm. */
    float y_ridge = y_eaves + roof_h;
    if (osmmesh_emit_skeleton_faces(b, sk, xy, n, y_eaves, y_ridge) != 0) {
        osmmesh_skeleton_free(sk);
        return -1;
    }
    osmmesh_skeleton_free(sk);
    return 0;
}

/* ==========================================================================
 *  gabled
 *
 *  OBB-split + two lifted sub-polygons + wall_fill with cut at the ridge.
 *  Non-rectangular fallback (split fails): caller tries hipped.
 * ========================================================================== */

/* Returns 0 on success, -1 on OOM, 1 on "gabled path not applicable —
 * try hipped fallback". */
static int build_gabled(osmmesh_emit_buf *b,
                         const osmmesh_building_footprint *fp,
                         const float *z_bot_per_v,
                         int ccw, float y_eaves, float roof_h)
{
    uint32_t n = fp->n;
    const float *xy = fp->xy;

    osmmesh_building_obb_t box;
    if (osmmesh_building_geom_obb(xy, n, &box) != 0) return 1;
    float lx = box.extent[0];
    float ly = box.extent[1];
    if (lx < 1e-3f || ly < 1e-3f) return 1;

    /* Ridge runs along long axis (axis_x); cut perpendicular to it. */
    float ridge_dir[2] = { box.axis_x[0], box.axis_x[1] };
    float half_short   = ly * 0.5f;

    /* Only rectangular footprints get the OBB-split path. */
    if (!osmmesh_building_geom_is_axis_aligned_rect(xy, n, ridge_dir))
        return 1;

    float cx = box.center[0], cy = box.center[1];
    float y_ridge = y_eaves + roof_h;

    /* Ridge-perpendicular unit vector (points across the ridge). */
    float nx = -ridge_dir[1], ny = ridge_dir[0];

    float *sub_a = NULL; uint32_t sub_an = 0;
    float *sub_b = NULL; uint32_t sub_bn = 0;
    float origin[2] = { cx, cy };
    if (osmmesh_building_geom_polygon_split(xy, n, origin, ridge_dir,
                                              &sub_a, &sub_an,
                                              &sub_b, &sub_bn) != 0) {
        return 1;  /* Fall back to hipped. */
    }

    /* Walls (below eaves). */
    if (osmmesh_emit_walls_below_eaves(b, xy, n, z_bot_per_v, y_eaves, ccw) != 0) {
        free(sub_a); free(sub_b); return -1;
    }

    /* lift_subpoly: z[i] = y_eaves + (y_ridge - y_eaves) * (1 - t)
     * where t = min(1, v / half_short), v = |(p-c) . nx,ny|. */
    float z_sub_a[OSMMESH_BUILDING_MAX_VERTS];
    float z_sub_b[OSMMESH_BUILDING_MAX_VERTS];
    if (sub_an > OSMMESH_BUILDING_MAX_VERTS ||
        sub_bn > OSMMESH_BUILDING_MAX_VERTS) {
        free(sub_a); free(sub_b);
        return 1;
    }

    for (uint32_t i = 0; i < sub_an; ++i) {
        float px = sub_a[i * 2 + 0], py = sub_a[i * 2 + 1];
        float v  = fabsf((px - cx) * nx + (py - cy) * ny);
        float t  = (half_short > 1e-5f) ? (v / half_short) : 1.0f;
        if (t > 1.0f) t = 1.0f;
        z_sub_a[i] = y_eaves + (y_ridge - y_eaves) * (1.0f - t);
    }
    for (uint32_t i = 0; i < sub_bn; ++i) {
        float px = sub_b[i * 2 + 0], py = sub_b[i * 2 + 1];
        float v  = fabsf((px - cx) * nx + (py - cy) * ny);
        float t  = (half_short > 1e-5f) ? (v / half_short) : 1.0f;
        if (t > 1.0f) t = 1.0f;
        z_sub_b[i] = y_eaves + (y_ridge - y_eaves) * (1.0f - t);
    }

    /* Both sub-polygons inherit the parent ring's orientation. */
    if (osmmesh_emit_lifted_ring(b, sub_a, sub_an, z_sub_a, ccw) != 0) {
        free(sub_a); free(sub_b); return -1;
    }
    if (osmmesh_emit_lifted_ring(b, sub_b, sub_bn, z_sub_b, ccw) != 0) {
        free(sub_a); free(sub_b); return -1;
    }
    free(sub_a); free(sub_b);

    /* Gable-end triangles: emit DIRECTLY from polygon_split's vertex
     * data so the intersection points match byte-for-byte with the
     * subpoly ridge endpoints. Each sub-polygon has exactly two "ridge
     * endpoints" — vertices whose z == y_ridge — sitting on the cut
     * line. For each such pair we need one gable-end triangle whose
     * base spans the two footprint corners adjacent to the cut and
     * whose peak is the ridge endpoint.
     *
     * More precisely: sub_a and sub_b share the same two intersection
     * points. Walking sub_a, an intersection vertex is flanked by a
     * footprint corner on one side (the eave-level predecessor or
     * successor). That corner + the OTHER footprint corner flanking
     * the SAME intersection in sub_b define the gable-end triangle.
     *
     * Simpler approach: scan sub_a for each "ridge vertex" (z at
     * y_ridge) and for each ring-edge that traverses it, identify the
     * footprint corner (z at y_eaves) adjacent to it. Do the same for
     * sub_b. Combine to get the gable triangle for each ridge endpoint. */
    /* Implementation: walk the ORIGINAL polygon again, compute the
     * same signed-distance sa (in DOUBLE precision, matching
     * polygon_split exactly), and emit a gable_tri whose intersection
     * coordinates are looked up from sub_a / sub_b. */
    double ox_d = cx, oy_d = cy;
    double dirx = ridge_dir[0], diry = ridge_dir[1];
    double dlen = sqrt(dirx * dirx + diry * diry);
    double nxd = -diry / dlen, nyd = dirx / dlen;
    float ndir = ccw ? 1.0f : -1.0f;

    /* Locate the two intersection points in sub_a/sub_b by scanning
     * for z=y_ridge (or equivalently t=0 in lift). They are shared. */
    float ridge_pts[2][2];
    uint32_t n_ridge = 0;
    for (uint32_t i = 0; i < sub_an && n_ridge < 2; ++i) {
        if (fabsf(z_sub_a[i] - y_ridge) < 1e-4f) {
            ridge_pts[n_ridge][0] = sub_a[i * 2 + 0];
            ridge_pts[n_ridge][1] = sub_a[i * 2 + 1];
            ++n_ridge;
        }
    }

    /* For each polygon edge that straddles the cut, find its
     * intersection point (the ridge_pt with minimum distance) and
     * emit a gable triangle. The ridge_pt is guaranteed to equal the
     * polygon_split computation because we walked the same math. */
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t j = (i + 1) % n;
        double ax = xy[i * 2 + 0], ay = xy[i * 2 + 1];
        double bx = xy[j * 2 + 0], by = xy[j * 2 + 1];
        double sa = (ax - ox_d) * nxd + (ay - oy_d) * nyd;
        double sb = (bx - ox_d) * nxd + (by - oy_d) * nyd;
        if (!((sa > 0.0 && sb < 0.0) || (sa < 0.0 && sb > 0.0))) continue;

        /* Which ridge_pt corresponds to this edge? Pick the one closest
         * to the parametric mid-intersection. (For well-formed
         * rectangles with 2 intersections total, this uniquely matches.) */
        double t = sa / (sa - sb);
        double mid_x = ax + t * (bx - ax);
        double mid_y = ay + t * (by - ay);
        float best_ix = 0.0f, best_iy = 0.0f;
        double best_d2 = 1e30;
        for (uint32_t k = 0; k < n_ridge; ++k) {
            double dxk = ridge_pts[k][0] - mid_x;
            double dyk = ridge_pts[k][1] - mid_y;
            double d2 = dxk * dxk + dyk * dyk;
            if (d2 < best_d2) {
                best_d2 = d2;
                best_ix = ridge_pts[k][0];
                best_iy = ridge_pts[k][1];
            }
        }

        /* Outward normal of this edge (matches emit_wall_fill's
         * convention). */
        double edx = bx - ax, edy = by - ay;
        double elen = sqrt(edx * edx + edy * edy);
        if (elen < 1e-9) continue;
        float onx = (float)(ndir * edy / elen);
        float ony = (float)(-ndir * edx / elen);

        /* Emit one triangle: low-A (eave) = (ax, ay, y_eaves),
         *                    low-B (eave) = (bx, by, y_eaves),
         *                    peak          = (best_ix, best_iy, y_ridge).
         * Winding picks a face normal matching (onx, ony, 0). */
        float p0x = (float)ax, p0y = (float)ay, p0z = y_eaves;
        float p1x = (float)bx, p1y = (float)by, p1z = y_eaves;
        float p2x = best_ix,   p2y = best_iy,   p2z = y_ridge;

        float n3[3];
        float e1x = p1x - p0x, e1y = p1y - p0y, e1z = p1z - p0z;
        float e2x = p2x - p0x, e2y = p2y - p0y, e2z = p2z - p0z;
        n3[0] = e1y * e2z - e1z * e2y;
        n3[1] = e1z * e2x - e1x * e2z;
        n3[2] = e1x * e2y - e1y * e2x;
        float ln = sqrtf(n3[0]*n3[0] + n3[1]*n3[1] + n3[2]*n3[2]);
        if (ln < 1e-9f) continue;
        n3[0] /= ln; n3[1] /= ln; n3[2] /= ln;
        if (n3[0] * onx + n3[1] * ony < 0.0f) {
            /* Swap p1 and p2. */
            float tx = p1x, ty = p1y, tz = p1z;
            p1x = p2x; p1y = p2y; p1z = p2z;
            p2x = tx;  p2y = ty;  p2z = tz;
            n3[0] = -n3[0]; n3[1] = -n3[1]; n3[2] = -n3[2];
        }
        if (osmmesh_emit_buf_reserve(b, 3, 1) != 0) return -1;
        /* Hand-roll: emit 3 verts + 1 tri. Stored normal is the
         * outward (onx, ony, 0) so UVs come out consistently with
         * the wall convention. */
        uint32_t v0 = b->nv++;
        uint32_t v1 = b->nv++;
        uint32_t v2 = b->nv++;
        b->positions[v0*3+0]=p0x; b->positions[v0*3+1]=p0y; b->positions[v0*3+2]=p0z;
        b->positions[v1*3+0]=p1x; b->positions[v1*3+1]=p1y; b->positions[v1*3+2]=p1z;
        b->positions[v2*3+0]=p2x; b->positions[v2*3+1]=p2y; b->positions[v2*3+2]=p2z;
        b->normals  [v0*3+0]=onx; b->normals  [v0*3+1]=ony; b->normals  [v0*3+2]=0.0f;
        b->normals  [v1*3+0]=onx; b->normals  [v1*3+1]=ony; b->normals  [v1*3+2]=0.0f;
        b->normals  [v2*3+0]=onx; b->normals  [v2*3+1]=ony; b->normals  [v2*3+2]=0.0f;
        b->indices[b->nt*3+0]=v0;
        b->indices[b->nt*3+1]=v1;
        b->indices[b->nt*3+2]=v2;
        ++b->nt;
    }

    return 0;
}

/* ==========================================================================
 *  Dispatch — the single entry point used by building.c
 * ========================================================================== */

int osmmesh_building_roof_build_dispatch(
    osmmesh_emit_buf *b,
    const osmmesh_building_footprint *fp,
    osmmesh_roof_shape shape,
    float base_z,
    float wall_h,
    float roof_h,
    const float *vertex_bot_z);

int osmmesh_building_roof_build_dispatch(
    osmmesh_emit_buf *b,
    const osmmesh_building_footprint *fp,
    osmmesh_roof_shape shape,
    float base_z,
    float wall_h,
    float roof_h,
    const float *vertex_bot_z)
{
    if (!b || !fp || fp->n < 3) return -1;

    uint32_t n = fp->n;
    int ccw = (fp->signed_area > 0.0f) ? 1 : 0;
    float y_eaves = base_z + wall_h;

    /* Per-vertex wall-bottom z: terrain drape if provided, otherwise
     * flat at base_z. */
    float z_bot[OSMMESH_BUILDING_MAX_VERTS];
    for (uint32_t i = 0; i < n; ++i) {
        z_bot[i] = vertex_bot_z ? vertex_bot_z[i] : base_z;
    }

    int rc;
    switch (shape) {
    case OSMMESH_ROOF_FLAT:
    default:
        return build_flat(b, fp->xy, n, z_bot, ccw, y_eaves);

    case OSMMESH_ROOF_HIPPED:
        /* Degenerate pitch -> flat: without this the skeleton's ridge
         * compresses into the eave plane and boundary/ridge edges
         * collapse, leaving open edges where walls meet roof at the
         * degenerate ridge. */
        if (roof_h < 1e-3f)
            return build_flat(b, fp->xy, n, z_bot, ccw, y_eaves);
        rc = build_hipped(b, fp->xy, n, z_bot, ccw, y_eaves, roof_h);
        if (rc == 1) {
            b->nv = 0; b->nt = 0;
            return build_flat(b, fp->xy, n, z_bot, ccw, y_eaves);
        }
        return rc;

    case OSMMESH_ROOF_GABLED:
        if (roof_h < 1e-3f)
            return build_flat(b, fp->xy, n, z_bot, ccw, y_eaves);
        rc = build_gabled(b, fp, z_bot, ccw, y_eaves, roof_h);
        if (rc == 1) {
            b->nv = 0; b->nt = 0;
            rc = build_hipped(b, fp->xy, n, z_bot, ccw, y_eaves, roof_h);
            if (rc == 1) {
                b->nv = 0; b->nt = 0;
                return build_flat(b, fp->xy, n, z_bot, ccw, y_eaves);
            }
        }
        return rc;
    }
}
