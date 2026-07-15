/* libosmmesh/src/building_geom.c
 *
 * Ports streetwalk/tools/building/builders/roofs/_geom.py (the subset
 * the roof builders actually need at runtime): OBB via rotating calipers,
 * signed-area / CCW test, rectangle test, polygon-split, pitch helper.
 *
 * Triangulation + polylabel are NOT ported — libosmmesh already has its
 * own ear-clipper (building_triangulate.c) and roof builders here won't
 * need polylabel.
 */

#include "building_geom.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Not in strict C99 without extensions — mirror terrain_planish.c. */
#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

/* -------------------------------------------------------------------------
 * Signed area + CCW
 * ------------------------------------------------------------------------- */

float osmmesh_building_geom_signed_area(const float *xy, uint32_t n)
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

int osmmesh_building_geom_is_ccw(const float *xy, uint32_t n)
{
    return osmmesh_building_geom_signed_area(xy, n) > 0.0f ? 1 : 0;
}

/* -------------------------------------------------------------------------
 * Rectangle test
 *
 * A polygon is "axis-aligned" relative to a direction axis iff every edge
 * vector projects to either zero along-axis or zero across-axis. We also
 * require exactly 4 vertices — anything else can't be a rectangle.
 *
 * Tolerance: 1e-3 m on the smaller projection. At OSM noise levels (1-5 cm)
 * this prevents false negatives on slightly wonky quads.
 * ------------------------------------------------------------------------- */

int osmmesh_building_geom_is_axis_aligned_rect(const float *xy, uint32_t n,
                                                 const float axis_dir[2])
{
    if (!xy || !axis_dir || n != 4) return 0;
    double ax = axis_dir[0], ay = axis_dir[1];
    double len = sqrt(ax * ax + ay * ay);
    if (len < 1e-9) return 0;
    ax /= len; ay /= len;
    double px = -ay, py = ax;  /* perpendicular */

    for (uint32_t i = 0; i < 4; ++i) {
        uint32_t j = (i + 1) % 4;
        double dx = (double)xy[j * 2 + 0] - (double)xy[i * 2 + 0];
        double dy = (double)xy[j * 2 + 1] - (double)xy[i * 2 + 1];
        double along  = dx * ax + dy * ay;
        double across = dx * px + dy * py;
        /* Accept if at least one component is near zero. */
        if (fabs(along) > 1e-3 && fabs(across) > 1e-3) return 0;
    }
    return 1;
}

/* -------------------------------------------------------------------------
 * Convex hull (Andrew's monotone chain)
 *
 * Local copy keeps this file self-contained so we don't expose
 * building_internal.h's variant. Takes interleaved xy, returns hull point
 * count `m` and writes hull coordinates into `hx`/`hy` parallel arrays.
 * ------------------------------------------------------------------------- */

typedef struct { double x, y; } geom_pt;

static int geom_hull_cmp(const void *a, const void *b)
{
    const geom_pt *pa = (const geom_pt *)a;
    const geom_pt *pb = (const geom_pt *)b;
    if (pa->x < pb->x) return -1;
    if (pa->x > pb->x) return  1;
    if (pa->y < pb->y) return -1;
    if (pa->y > pb->y) return  1;
    return 0;
}

static double geom_cross(geom_pt o, geom_pt a, geom_pt b)
{
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

static uint32_t geom_convex_hull(const float *xy, uint32_t n,
                                  geom_pt *out_hull /* cap >= 2n+2 */)
{
    if (n < 2) {
        for (uint32_t i = 0; i < n; ++i) {
            out_hull[i].x = xy[i * 2 + 0];
            out_hull[i].y = xy[i * 2 + 1];
        }
        return n;
    }

    geom_pt *buf = (geom_pt *)malloc(sizeof(geom_pt) * n);
    if (!buf) return 0;
    for (uint32_t i = 0; i < n; ++i) {
        buf[i].x = xy[i * 2 + 0];
        buf[i].y = xy[i * 2 + 1];
    }
    qsort(buf, n, sizeof(geom_pt), geom_hull_cmp);

    /* Deduplicate sorted points. */
    uint32_t nu = 0;
    for (uint32_t i = 0; i < n; ++i) {
        if (nu > 0 && buf[i].x == buf[nu - 1].x && buf[i].y == buf[nu - 1].y) continue;
        buf[nu++] = buf[i];
    }
    if (nu < 2) {
        for (uint32_t i = 0; i < nu; ++i) out_hull[i] = buf[i];
        free(buf);
        return nu;
    }

    uint32_t k = 0;
    /* Lower hull. */
    for (uint32_t i = 0; i < nu; ++i) {
        while (k >= 2 && geom_cross(out_hull[k - 2], out_hull[k - 1], buf[i]) <= 0.0) --k;
        out_hull[k++] = buf[i];
    }
    uint32_t lower = k + 1;
    /* Upper hull. */
    for (int32_t i = (int32_t)nu - 2; i >= 0; --i) {
        while (k >= lower && geom_cross(out_hull[k - 2], out_hull[k - 1], buf[(uint32_t)i]) <= 0.0) --k;
        out_hull[k++] = buf[(uint32_t)i];
    }
    free(buf);
    return k - 1;  /* drop repeated start point */
}

/* -------------------------------------------------------------------------
 * Oriented bounding box via rotating calipers
 * ------------------------------------------------------------------------- */

int osmmesh_building_geom_obb(const float *xy, uint32_t n,
                               osmmesh_building_obb_t *out)
{
    if (!xy || !out || n < 3) return -1;

    /* Hull. Cap generously so we can hold both hull chains. */
    uint32_t hull_cap = 2 * n + 2;
    geom_pt *hull = (geom_pt *)malloc(sizeof(geom_pt) * hull_cap);
    if (!hull) return -1;

    uint32_t m = geom_convex_hull(xy, n, hull);
    if (m < 3) { free(hull); return -1; }

    double best_area = DBL_MAX;
    double best_ux = 1.0, best_uy = 0.0;
    double best_umin = 0.0, best_umax = 0.0;
    double best_vmin = 0.0, best_vmax = 0.0;

    for (uint32_t i = 0; i < m; ++i) {
        uint32_t j = (i + 1) % m;
        double dx = hull[j].x - hull[i].x;
        double dy = hull[j].y - hull[i].y;
        double len = sqrt(dx * dx + dy * dy);
        if (len < 1e-9) continue;
        double ux = dx / len, uy = dy / len;
        double vx = -uy,      vy = ux;

        double u0 = hull[i].x * ux + hull[i].y * uy;
        double v0 = hull[i].x * vx + hull[i].y * vy;
        double umin = u0, umax = u0, vmin = v0, vmax = v0;
        for (uint32_t k = 0; k < m; ++k) {
            double u = hull[k].x * ux + hull[k].y * uy;
            double v = hull[k].x * vx + hull[k].y * vy;
            if (u < umin) umin = u; else if (u > umax) umax = u;
            if (v < vmin) vmin = v; else if (v > vmax) vmax = v;
        }
        double a = (umax - umin) * (vmax - vmin);
        if (a < best_area) {
            best_area = a;
            best_ux = ux; best_uy = uy;
            best_umin = umin; best_umax = umax;
            best_vmin = vmin; best_vmax = vmax;
        }
    }
    free(hull);
    if (best_area == DBL_MAX) return -1;

    double du = best_umax - best_umin;
    double dv = best_vmax - best_vmin;
    double cu = 0.5 * (best_umin + best_umax);
    double cv = 0.5 * (best_vmin + best_vmax);

    double ux = best_ux, uy = best_uy;
    double vx = -uy,     vy = ux;

    /* axis_x must be the longer side. */
    if (du >= dv) {
        out->axis_x[0] = (float)ux;
        out->axis_x[1] = (float)uy;
        out->axis_y[0] = (float)vx;
        out->axis_y[1] = (float)vy;
        out->extent[0] = (float)du;
        out->extent[1] = (float)dv;
    } else {
        out->axis_x[0] = (float)vx;
        out->axis_x[1] = (float)vy;
        out->axis_y[0] = (float)ux;
        out->axis_y[1] = (float)uy;
        out->extent[0] = (float)dv;
        out->extent[1] = (float)du;
    }
    out->center[0] = (float)(cu * ux + cv * vx);
    out->center[1] = (float)(cu * uy + cv * vy);
    return 0;
}

/* -------------------------------------------------------------------------
 * Polygon split by infinite line
 *
 * Walk edges, track which side of the cut line each vertex sits on. Emit
 * the vertex into the "positive" sub-polygon when side >= 0, into the
 * "negative" sub-polygon when side <= 0 (vertices exactly on the line
 * go into both). On a sign flip across an edge, compute the intersection
 * and insert it into both sub-polygons so they share the cut edge.
 *
 * We require exactly two sign flips for a valid cut. Anything else means
 * the line either grazes the polygon or cuts it into more than two pieces
 * (non-convex); return -1.
 * ------------------------------------------------------------------------- */

int osmmesh_building_geom_polygon_split(const float *xy, uint32_t n,
                                          const float origin[2],
                                          const float dir[2],
                                          float **out_a, uint32_t *out_an,
                                          float **out_b, uint32_t *out_bn)
{
    if (out_a) *out_a = NULL;
    if (out_b) *out_b = NULL;
    if (out_an) *out_an = 0;
    if (out_bn) *out_bn = 0;
    if (!xy || !origin || !dir || !out_a || !out_b || !out_an || !out_bn) return -1;
    if (n < 3) return -1;

    double dlen = sqrt((double)dir[0] * dir[0] + (double)dir[1] * dir[1]);
    if (dlen < 1e-9) return -1;

    /* Line normal (rotate dir 90 deg). */
    double nx = -(double)dir[1] / dlen;
    double ny =  (double)dir[0] / dlen;
    double ox = origin[0], oy = origin[1];

    /* Worst case both sub-polys hold n + n intersections. Allocate n+2 each. */
    uint32_t cap = n + 2;
    float *A = (float *)malloc(sizeof(float) * 2 * cap);
    float *B = (float *)malloc(sizeof(float) * 2 * cap);
    if (!A || !B) { free(A); free(B); return -1; }
    uint32_t na = 0, nb = 0;
    int cuts = 0;

    for (uint32_t i = 0; i < n; ++i) {
        uint32_t j = (i + 1) % n;
        double ax = xy[i * 2 + 0], ay = xy[i * 2 + 1];
        double bx = xy[j * 2 + 0], by = xy[j * 2 + 1];
        double sa = (ax - ox) * nx + (ay - oy) * ny;
        double sb = (bx - ox) * nx + (by - oy) * ny;

        if (sa >= 0.0) { A[na * 2 + 0] = (float)ax; A[na * 2 + 1] = (float)ay; ++na; }
        if (sa <= 0.0) { B[nb * 2 + 0] = (float)ax; B[nb * 2 + 1] = (float)ay; ++nb; }

        if ((sa > 0.0 && sb < 0.0) || (sa < 0.0 && sb > 0.0)) {
            double t = sa / (sa - sb);
            /* Reject near-vertex grazing cuts: at t very close to 0 or
             * 1 the intersection coordinate lies within a cm of an
             * original polygon vertex, yet doesn't BYTE-equal it,
             * which desyncs the two subpoly copies vs the source
             * wall-corner and produces sub-cm open edges at the
             * quantisation grid. Fail the split so the caller falls
             * back to a non-split (hipped) topology. */
            if (t < 1e-4 || t > 1.0 - 1e-4) {
                free(A); free(B);
                return -1;
            }
            double ix = ax + t * (bx - ax);
            double iy = ay + t * (by - ay);
            A[na * 2 + 0] = (float)ix; A[na * 2 + 1] = (float)iy; ++na;
            B[nb * 2 + 0] = (float)ix; B[nb * 2 + 1] = (float)iy; ++nb;
            ++cuts;
        }
    }

    if (cuts != 2 || na < 3 || nb < 3) {
        free(A); free(B);
        return -1;
    }

    *out_a = A; *out_an = na;
    *out_b = B; *out_bn = nb;
    return 0;
}

/* -------------------------------------------------------------------------
 * Ridge-height helper
 * ------------------------------------------------------------------------- */

float osmmesh_building_geom_ridge_height_for_pitch(float run_m, float pitch_deg)
{
    double p = (double)pitch_deg * (M_PI / 180.0);
    return (float)((double)run_m * tan(p));
}
