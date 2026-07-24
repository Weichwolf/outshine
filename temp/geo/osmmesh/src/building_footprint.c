/* libosmmesh/src/building_footprint.c
 *
 * Footprint analysis: MVT outer ring -> ENU polygon + OBB + hull ratio.
 *
 * The analysis has three consumers:
 *   - Heuristic roof choice (hull_ratio, is_rectangular, is_regular).
 *   - Geometry (OBB axis/extent for the gabled/hipped ridge direction).
 *   - Debug info (footprint_area_m2, obb_long/short) for tests & telemetry.
 *
 * Outer-ring detection (MVP): the first ring of the feature is taken as the
 * outer; remaining rings are logged and skipped. A proper implementation
 * would sign-check every ring (T7+). Inner rings ≠ supported in the MVP.
 */

#include "building_internal.h"

#include "osmmesh/geo.h"
#include "osmmesh/mvt.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Signed area
 * ------------------------------------------------------------------------- */

float osmmesh_building_polygon_signed_area(const float *xy, uint32_t n)
{
    if (!xy || n < 3) return 0.0f;
    double a = 0.0;
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t j = (i + 1) % n;
        a += (double)xy[i * 2 + 0] * (double)xy[j * 2 + 1]
           - (double)xy[j * 2 + 0] * (double)xy[i * 2 + 1];
    }
    return (float)(a * 0.5);
}

/* -------------------------------------------------------------------------
 * Convex hull (Andrew's monotone chain)
 *
 * Sort points by (x, y) then build lower + upper hulls. Duplicate first/last
 * point is removed before returning. Output indices reference the original
 * `xy` array. O(n log n).
 * ------------------------------------------------------------------------- */

typedef struct { float x, y; uint32_t idx; } hull_pt;

static int hull_cmp(const void *a, const void *b)
{
    const hull_pt *pa = (const hull_pt *)a;
    const hull_pt *pb = (const hull_pt *)b;
    if (pa->x < pb->x) return -1;
    if (pa->x > pb->x) return  1;
    if (pa->y < pb->y) return -1;
    if (pa->y > pb->y) return  1;
    return 0;
}

static float hull_cross(hull_pt o, hull_pt a, hull_pt b)
{
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

uint32_t osmmesh_building_convex_hull(const float *xy, uint32_t n,
                                        uint32_t *out_indices)
{
    if (n < 3) {
        for (uint32_t i = 0; i < n; ++i) out_indices[i] = i;
        return n;
    }

    /* Capacity = OSMMESH_BUILDING_MAX_VERTS (caller guards n <= this). */
    hull_pt buf[OSMMESH_BUILDING_MAX_VERTS];
    hull_pt hull[2 * OSMMESH_BUILDING_MAX_VERTS + 4];
    if (n > OSMMESH_BUILDING_MAX_VERTS) n = OSMMESH_BUILDING_MAX_VERTS;

    for (uint32_t i = 0; i < n; ++i) {
        buf[i].x = xy[i * 2 + 0];
        buf[i].y = xy[i * 2 + 1];
        buf[i].idx = i;
    }
    qsort(buf, n, sizeof(hull_pt), hull_cmp);

    uint32_t k = 0;
    for (uint32_t i = 0; i < n; ++i) {
        while (k >= 2 && hull_cross(hull[k - 2], hull[k - 1], buf[i]) <= 0) --k;
        hull[k++] = buf[i];
    }
    uint32_t lower = k + 1;
    for (int32_t i = (int32_t)n - 2; i >= 0; --i) {
        while (k >= lower && hull_cross(hull[k - 2], hull[k - 1], buf[i]) <= 0) --k;
        hull[k++] = buf[(uint32_t)i];
    }
    /* Last point == first point; drop it. */
    uint32_t m = k - 1;
    for (uint32_t i = 0; i < m; ++i) out_indices[i] = hull[i].idx;
    return m;
}

/* -------------------------------------------------------------------------
 * Oriented bounding box via rotating calipers on the hull.
 *
 * For each hull edge, rotate the polygon so the edge is axis-aligned, take
 * the AABB, keep the minimum-area one. `out_axis` is the unit vector along
 * the LONG side; corners are CCW around the OBB with corner[0] anchored at
 * the (u_min, v_min) corner of the projection.
 * ------------------------------------------------------------------------- */

void osmmesh_building_obb(const float *xy, uint32_t n,
                            float out_corners[4][2],
                            float *out_long, float *out_short,
                            float out_axis[2], float out_center[2])
{
    /* Degenerate fallback: AABB. */
    if (n < 3) {
        float mn_x = FLT_MAX, mn_y = FLT_MAX, mx_x = -FLT_MAX, mx_y = -FLT_MAX;
        for (uint32_t i = 0; i < n; ++i) {
            float x = xy[i * 2], y = xy[i * 2 + 1];
            if (x < mn_x) mn_x = x;
            if (x > mx_x) mx_x = x;
            if (y < mn_y) mn_y = y;
            if (y > mx_y) mx_y = y;
        }
        out_corners[0][0] = mn_x; out_corners[0][1] = mn_y;
        out_corners[1][0] = mx_x; out_corners[1][1] = mn_y;
        out_corners[2][0] = mx_x; out_corners[2][1] = mx_y;
        out_corners[3][0] = mn_x; out_corners[3][1] = mx_y;
        float dx = mx_x - mn_x, dy = mx_y - mn_y;
        *out_long  = dx > dy ? dx : dy;
        *out_short = dx > dy ? dy : dx;
        out_axis[0] = dx >= dy ? 1.0f : 0.0f;
        out_axis[1] = dx >= dy ? 0.0f : 1.0f;
        out_center[0] = 0.5f * (mn_x + mx_x);
        out_center[1] = 0.5f * (mn_y + mx_y);
        return;
    }

    uint32_t hull_idx[OSMMESH_BUILDING_MAX_VERTS];
    uint32_t m = osmmesh_building_convex_hull(xy, n, hull_idx);
    if (m < 3) {
        /* All points collinear: bail with AABB. */
        osmmesh_building_obb(xy, 0, out_corners, out_long, out_short,
                             out_axis, out_center);
        /* AABB from the actual points. */
        float mn_x = FLT_MAX, mn_y = FLT_MAX, mx_x = -FLT_MAX, mx_y = -FLT_MAX;
        for (uint32_t i = 0; i < n; ++i) {
            float x = xy[i * 2], y = xy[i * 2 + 1];
            if (x < mn_x) mn_x = x;
            if (x > mx_x) mx_x = x;
            if (y < mn_y) mn_y = y;
            if (y > mx_y) mx_y = y;
        }
        out_corners[0][0] = mn_x; out_corners[0][1] = mn_y;
        out_corners[1][0] = mx_x; out_corners[1][1] = mn_y;
        out_corners[2][0] = mx_x; out_corners[2][1] = mx_y;
        out_corners[3][0] = mn_x; out_corners[3][1] = mx_y;
        float dx = mx_x - mn_x, dy = mx_y - mn_y;
        *out_long  = dx > dy ? dx : dy;
        *out_short = dx > dy ? dy : dx;
        out_axis[0] = dx >= dy ? 1.0f : 0.0f;
        out_axis[1] = dx >= dy ? 0.0f : 1.0f;
        out_center[0] = 0.5f * (mn_x + mx_x);
        out_center[1] = 0.5f * (mn_y + mx_y);
        return;
    }

    double best_area = DBL_MAX;
    double best_long = 0.0, best_short = 0.0;
    float  best_axis[2] = {1.0f, 0.0f};
    float  best_corners[4][2] = {{0}};
    float  best_center[2] = {0.0f, 0.0f};

    for (uint32_t i = 0; i < m; ++i) {
        uint32_t j = (i + 1) % m;
        float ax = xy[hull_idx[i] * 2 + 0];
        float ay = xy[hull_idx[i] * 2 + 1];
        float bx = xy[hull_idx[j] * 2 + 0];
        float by = xy[hull_idx[j] * 2 + 1];
        double ex = bx - ax, ey = by - ay;
        double len = sqrt(ex * ex + ey * ey);
        if (len < 1e-6) continue;
        double ux = ex / len, uy = ey / len;
        double vx = -uy,      vy = ux;

        double u_min =  DBL_MAX, u_max = -DBL_MAX;
        double v_min =  DBL_MAX, v_max = -DBL_MAX;
        for (uint32_t k = 0; k < m; ++k) {
            double px = (double)xy[hull_idx[k] * 2 + 0] - ax;
            double py = (double)xy[hull_idx[k] * 2 + 1] - ay;
            double pu = px * ux + py * uy;
            double pv = px * vx + py * vy;
            if (pu < u_min) u_min = pu;
            if (pu > u_max) u_max = pu;
            if (pv < v_min) v_min = pv;
            if (pv > v_max) v_max = pv;
        }
        double du = u_max - u_min;
        double dv = v_max - v_min;
        double area = du * dv;
        if (area < best_area) {
            best_area = area;
            /* Corners CCW: (u_min,v_min) -> (u_max,v_min) -> (u_max,v_max) -> (u_min,v_max) */
            float c[4][2];
            c[0][0] = (float)(ax + ux * u_min + vx * v_min);
            c[0][1] = (float)(ay + uy * u_min + vy * v_min);
            c[1][0] = (float)(ax + ux * u_max + vx * v_min);
            c[1][1] = (float)(ay + uy * u_max + vy * v_min);
            c[2][0] = (float)(ax + ux * u_max + vx * v_max);
            c[2][1] = (float)(ay + uy * u_max + vy * v_max);
            c[3][0] = (float)(ax + ux * u_min + vx * v_max);
            c[3][1] = (float)(ay + uy * u_min + vy * v_max);
            memcpy(best_corners, c, sizeof c);

            if (du >= dv) {
                best_long = du; best_short = dv;
                best_axis[0] = (float)ux; best_axis[1] = (float)uy;
            } else {
                best_long = dv; best_short = du;
                best_axis[0] = (float)vx; best_axis[1] = (float)vy;
            }
            best_center[0] = 0.5f * (c[0][0] + c[2][0]);
            best_center[1] = 0.5f * (c[0][1] + c[2][1]);
        }
    }

    memcpy(out_corners, best_corners, sizeof best_corners);
    *out_long  = (float)best_long;
    *out_short = (float)best_short;
    out_axis[0] = best_axis[0];
    out_axis[1] = best_axis[1];
    out_center[0] = best_center[0];
    out_center[1] = best_center[1];
}

/* -------------------------------------------------------------------------
 * Entry point
 * ------------------------------------------------------------------------- */

int osmmesh_building_footprint_build(const osmmesh_mvt_feature  *feature,
                                      const osmmesh_tile_enu_map *map,
                                      osmmesh_building_footprint *out)
{
    if (!feature || !map || !out) return OSMMESH_BUILDING_ERR_ARG;
    if (feature->geom_type != OSMMESH_MVT_GEOM_POLYGON) return OSMMESH_BUILDING_ERR_ARG;
    if (feature->n_rings < 1 || !feature->ring_offsets) return OSMMESH_BUILDING_ERR_SKIP;

    memset(out, 0, sizeof *out);

    /* First ring = outer (MVP). Warn on extras. */
    if (feature->n_rings > 1) {
        fprintf(stderr,
                "osmmesh_building: feature id=%llu has %zu rings; "
                "inner rings ignored in MVP\n",
                (unsigned long long)feature->id,
                (size_t)feature->n_rings);
    }

    uint32_t start = feature->ring_offsets[0];
    uint32_t end   = feature->ring_offsets[1];
    if (end <= start) return OSMMESH_BUILDING_ERR_SKIP;
    uint32_t raw_n = end - start;
    if (raw_n < 3) return OSMMESH_BUILDING_ERR_SKIP;
    if (raw_n > OSMMESH_BUILDING_MAX_VERTS) return OSMMESH_BUILDING_ERR_SKIP;

    /* Project into ENU, strip a trailing duplicate if present. */
    uint32_t n = 0;
    for (uint32_t i = 0; i < raw_n; ++i) {
        const osmmesh_mvt_coord *c = &feature->coords[start + i];
        osmmesh_enu e = osmmesh_tile_enu_map_apply(map, c->x, c->y);
        out->xy[n * 2 + 0] = (float)e.e;
        out->xy[n * 2 + 1] = (float)e.n;
        ++n;
    }
    if (n >= 2) {
        float dx = out->xy[(n - 1) * 2 + 0] - out->xy[0];
        float dy = out->xy[(n - 1) * 2 + 1] - out->xy[1];
        if (dx * dx + dy * dy < 1e-6f) { /* < 1 mm */
            --n;
        }
    }
    if (n < 3) return OSMMESH_BUILDING_ERR_SKIP;

    out->n = n;
    out->signed_area = osmmesh_building_polygon_signed_area(out->xy, n);
    out->area_abs    = fabsf(out->signed_area);
    if (out->area_abs < 1e-4f) return OSMMESH_BUILDING_ERR_SKIP;

    /* OBB + hull. */
    osmmesh_building_obb(out->xy, n,
                          out->obb_corners, &out->obb_long, &out->obb_short,
                          out->obb_axis, out->obb_center);

    uint32_t hull[OSMMESH_BUILDING_MAX_VERTS];
    uint32_t hn = osmmesh_building_convex_hull(out->xy, n, hull);
    float hull_area = 0.0f;
    if (hn >= 3) {
        float hxy[OSMMESH_BUILDING_MAX_VERTS * 2];
        for (uint32_t i = 0; i < hn; ++i) {
            hxy[i * 2 + 0] = out->xy[hull[i] * 2 + 0];
            hxy[i * 2 + 1] = out->xy[hull[i] * 2 + 1];
        }
        hull_area = fabsf(osmmesh_building_polygon_signed_area(hxy, hn));
    }
    out->hull_ratio = hull_area > 1e-6f ? (out->area_abs / hull_area) : 1.0f;
    if (out->hull_ratio > 1.0f) out->hull_ratio = 1.0f; /* numeric guard */

    out->is_rectangular = (n == 4 && out->hull_ratio > 0.95f) ? 1 : 0;
    out->is_regular     = (out->hull_ratio > 0.85f) ? 1 : 0;

    return OSMMESH_BUILDING_OK;
}
