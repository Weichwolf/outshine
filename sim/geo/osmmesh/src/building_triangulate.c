/* libosmmesh/src/building_triangulate.c
 *
 * Ear-clipping triangulation for simple 2-D polygons without holes. Standard
 * O(n^2) algorithm — ~100 lines, no allocations, fine for the per-building
 * vertex counts we see in MVT data (typical n < 32, hard cap 512).
 *
 * The algorithm:
 *   1. Maintain a doubly-linked list of remaining vertex indices.
 *   2. Scan for an "ear": a vertex v where (prev, v, next) is a convex
 *      corner on the polygon's interior side, and no other vertex lies
 *      inside that triangle.
 *   3. Emit the triangle, splice v out, repeat until 3 vertices remain
 *      (the final triangle).
 *
 * The `ccw` parameter selects the interior side — on a CCW polygon an ear's
 * cross product is positive; on CW it's negative. The tests in T6 feed CW
 * polygons (MVT outer rings after ENU y-flip), so the common path is
 * `ccw == 0`.
 */

#include "building_internal.h"

#include <stdint.h>

static int point_in_tri(float px, float py,
                         float ax, float ay,
                         float bx, float by,
                         float cx, float cy)
{
    /* Barycentric via signed cross-products. A point is inside iff all three
     * sub-triangle areas share the sign of the parent area. We accept
     * edge-hits as "outside" (strict inequality) so vertices legally sitting
     * on a triangle edge don't block ear-clipping of near-collinear input. */
    float d1 = (px - bx) * (ay - by) - (ax - bx) * (py - by);
    float d2 = (px - cx) * (by - cy) - (bx - cx) * (py - cy);
    float d3 = (px - ax) * (cy - ay) - (cx - ax) * (py - ay);
    int has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    int has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(has_neg && has_pos);
}

uint32_t osmmesh_building_ear_clip(const float *xy, uint32_t n,
                                     int ccw, uint32_t *out_tris)
{
    if (n < 3 || !xy || !out_tris) return 0;

    /* Index ring. Using static storage keeps us allocation-free; we guard n
     * at the caller (OSMMESH_BUILDING_MAX_VERTS = 512). */
    uint32_t idx[OSMMESH_BUILDING_MAX_VERTS];
    if (n > OSMMESH_BUILDING_MAX_VERTS) return 0;
    for (uint32_t i = 0; i < n; ++i) idx[i] = i;

    uint32_t remaining = n;
    uint32_t written   = 0;
    /* Safety-net: a well-formed simple polygon clips in <= n iterations of
     * the outer loop; allow n*n to cover degenerate corners we need to skip. */
    uint32_t safety = n * n + 4;

    while (remaining > 3 && safety--) {
        int found_ear = 0;
        for (uint32_t i = 0; i < remaining; ++i) {
            uint32_t pi = (i + remaining - 1) % remaining;
            uint32_t ni = (i + 1) % remaining;
            uint32_t ai = idx[pi], bi = idx[i], ci = idx[ni];
            float ax = xy[ai * 2 + 0], ay = xy[ai * 2 + 1];
            float bx = xy[bi * 2 + 0], by = xy[bi * 2 + 1];
            float cx = xy[ci * 2 + 0], cy = xy[ci * 2 + 1];

            float cross = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
            if (ccw) { if (cross <= 0) continue; }
            else     { if (cross >= 0) continue; }

            /* Ear candidate. Reject if any other vertex is inside (a,b,c). */
            int contains = 0;
            for (uint32_t k = 0; k < remaining; ++k) {
                if (k == pi || k == i || k == ni) continue;
                uint32_t ki = idx[k];
                float px = xy[ki * 2 + 0], py = xy[ki * 2 + 1];
                if (point_in_tri(px, py, ax, ay, bx, by, cx, cy)) {
                    contains = 1;
                    break;
                }
            }
            if (contains) continue;

            out_tris[written * 3 + 0] = ai;
            out_tris[written * 3 + 1] = bi;
            out_tris[written * 3 + 2] = ci;
            ++written;

            /* Splice vertex i out of the ring. */
            for (uint32_t k = i; k + 1 < remaining; ++k) idx[k] = idx[k + 1];
            --remaining;
            found_ear = 1;
            break;
        }
        if (!found_ear) return 0; /* non-simple or wrong winding */
    }
    if (remaining == 3) {
        out_tris[written * 3 + 0] = idx[0];
        out_tris[written * 3 + 1] = idx[1];
        out_tris[written * 3 + 2] = idx[2];
        ++written;
    }
    return written;
}
