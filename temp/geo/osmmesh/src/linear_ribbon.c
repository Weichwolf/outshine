/* libosmmesh/src/linear_ribbon.c
 *
 * Polyline-to-ribbon sweep with mitre joins. The math is documented in
 * full in osmmesh/linear.h and cross-referenced here with the derivation
 * of the corner-vertex positions.
 *
 * Algorithm per sub-string:
 *   1. Walk edges, compute normalized direction d_i and left perpendicular
 *      n_i = (-d_i.y, d_i.x). Endpoints reuse the single available edge's
 *      direction.
 *   2. At every polyline vertex p_i, compute a left/right vertex pair
 *      offset by +/- w/2 along the local n_i (endpoints) or along a mitre
 *      vector (interior vertices).
 *   3. Mitre vector: b = n_{i-1} + n_i, scale = 1 / (1 + n_{i-1} . n_i).
 *      Resulting offset point = p_i + (w/2) * b * scale. By construction
 *      this point lies on the extended left offset lines of both adjacent
 *      segments.
 *      Identity: 1 + n_{i-1}.n_i = 1 + cos(alpha) = 2 cos^2(alpha/2),
 *      so the distance from the spine is (w/2) / cos(alpha/2). The ratio
 *      to w/2 is 1/cos(alpha/2); when that exceeds mitre_limit we fall
 *      back to a bevel join.
 *   4. Bevel: emit the spine-center vertex p_i plus two vertex pairs
 *      (one for end-of-segment-a, one for start-of-segment-b, each offset
 *      along its own n, without mitre extension). Close the corner with
 *      two cap triangles fanning from p_i whose winding depends on the
 *      turn direction so face normals stay +Z.
 *   5. Triangulation per segment: two tris, (left_out_i, right_out_i,
 *      right_in_{i+1}) and (left_out_i, right_in_{i+1}, left_in_{i+1}).
 *      The winding algebra (mirrored between the two tris) gives +Z face
 *      normal under the ENU basis.
 *
 * The sub-string writer is additive: callers set v_count / t_count on the
 * emit struct to append multiple ribbons into one mesh. Vertex normals are
 * always (0,0,1); UVs optional (u = cumulative meters along centerline,
 * v = 0 / 1 for right / left).
 */

#include "osmmesh/geo.h"
#include "osmmesh/linear.h"
#include "osmmesh/mesh.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "linear_internal.h"

/* Upper bound for vertex / triangle counts of one sub-string.
 *
 * Straight vertex: 2 verts emitted.
 * Mitre vertex:    2 verts emitted.
 * Bevel vertex:    5 verts emitted (center + two pairs) + 2 cap triangles.
 *
 * Per segment: 2 triangles.
 * Per bevel vertex: +2 triangles.
 *
 * So for n_spine vertices, worst case (all interior bevels):
 *   verts <= 2 * 2 (endpoints) + (n_spine - 2) * 5 = 5*n_spine - 6
 *   tris  <= (n_spine - 1) * 2 + (n_spine - 2) * 2 = 4*n_spine - 6
 *
 * Size conservatively to 5*n_spine and 4*n_spine. */
uint32_t osmmesh_linear_ribbon_vcap(uint32_t n_spine)
{
    return n_spine < 2 ? 0 : 5u * n_spine;
}

uint32_t osmmesh_linear_ribbon_tcap(uint32_t n_spine)
{
    return n_spine < 2 ? 0 : 4u * n_spine;
}

/* Internal helpers ------------------------------------------------------- */

static uint32_t emit_v(osmmesh_linear_emit *e,
                        float x, float y, float z,
                        float nx, float ny, float nz,
                        float u, float v)
{
    uint32_t i = e->v_count++;
    e->pos[i * 3 + 0] = x;  e->pos[i * 3 + 1] = y;  e->pos[i * 3 + 2] = z;
    e->nrm[i * 3 + 0] = nx; e->nrm[i * 3 + 1] = ny; e->nrm[i * 3 + 2] = nz;
    if (e->uvs) {
        e->uvs[i * 2 + 0] = u;
        e->uvs[i * 2 + 1] = v;
    }
    return i;
}

static void emit_t(osmmesh_linear_emit *e, uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t i = e->t_count++;
    e->idx[i * 3 + 0] = a;
    e->idx[i * 3 + 1] = b;
    e->idx[i * 3 + 2] = c;
}

/* One "ring" stores left/right index pairs at a polyline vertex.
 * Straight/mitre => single pair (in == out). Bevel => two pairs.
 * The "out" pair is used as the starting pair of the next segment; the
 * "in" pair is used as the ending pair of the previous segment. */
typedef struct {
    uint32_t left_in,  right_in;
    uint32_t left_out, right_out;
} corner_ring;

float osmmesh_linear_ribbon_emit(osmmesh_linear_emit *e,
                                  const float *xy, uint32_t n_spine,
                                  float width_m, float mitre_limit,
                                  int emit_uvs)
{
    if (n_spine < 2) return 0.0f;

    /* Pre-compute per-edge unit direction and per-vertex cumulative length. */
    uint32_t n_edge = n_spine - 1;
    float *dx  = (float *)malloc((size_t)n_edge  * sizeof(float));
    float *dy  = (float *)malloc((size_t)n_edge  * sizeof(float));
    float *cum = (float *)malloc((size_t)n_spine * sizeof(float));
    if (!dx || !dy || !cum) { free(dx); free(dy); free(cum); return 0.0f; }

    float total_len = 0.0f;
    cum[0] = 0.0f;
    for (uint32_t i = 0; i < n_edge; ++i) {
        float ex = xy[(i + 1) * 2 + 0] - xy[i * 2 + 0];
        float ey = xy[(i + 1) * 2 + 1] - xy[i * 2 + 1];
        float L  = sqrtf(ex * ex + ey * ey);
        if (L < 1e-9f) { dx[i] = 0.0f; dy[i] = 0.0f; }
        else           { dx[i] = ex / L; dy[i] = ey / L; }
        total_len += L;
        cum[i + 1] = total_len;
    }

    if (total_len < 1e-6f) {
        free(dx); free(dy); free(cum);
        return 0.0f;
    }

    float half_w = 0.5f * width_m;

    corner_ring *rings = (corner_ring *)malloc((size_t)n_spine * sizeof(corner_ring));
    if (!rings) { free(dx); free(dy); free(cum); return 0.0f; }

    for (uint32_t i = 0; i < n_spine; ++i) {
        float px = xy[i * 2 + 0];
        float py = xy[i * 2 + 1];
        float u  = cum[i];

        float dax = (i > 0)      ? dx[i - 1] : dx[i];
        float day = (i > 0)      ? dy[i - 1] : dy[i];
        float dbx = (i < n_edge) ? dx[i]     : dx[i - 1];
        float dby = (i < n_edge) ? dy[i]     : dy[i - 1];

        /* Left perpendiculars (+90 deg CCW around +Z: (x,y) -> (-y,x)). */
        float nax = -day, nay = dax;
        float nbx = -dby, nby = dbx;

        int is_endpoint = (i == 0) || (i == n_spine - 1);
        if (is_endpoint) {
            float nx = (i == 0) ? nbx : nax;
            float ny = (i == 0) ? nby : nay;
            uint32_t vl = emit_v(e, px + nx * half_w, py + ny * half_w, 0.0f,
                                    0.0f, 0.0f, 1.0f,
                                    emit_uvs ? u : 0.0f,
                                    emit_uvs ? 1.0f : 0.0f);
            uint32_t vr = emit_v(e, px - nx * half_w, py - ny * half_w, 0.0f,
                                    0.0f, 0.0f, 1.0f,
                                    emit_uvs ? u : 0.0f,
                                    emit_uvs ? 0.0f : 0.0f);
            rings[i].left_in  = rings[i].left_out  = vl;
            rings[i].right_in = rings[i].right_out = vr;
            continue;
        }

        /* Interior vertex. Test mitre-stretch ratio against the limit.
         * Half-angle identity: 1 + n_a.n_b = 2 cos^2(alpha/2), so the
         * stretch 1/cos(alpha/2) squared equals 2/(1+dot). */
        float dot   = nax * nbx + nay * nby;
        float denom = 1.0f + dot;
        int   do_bevel = 0;
        if (denom < 1e-4f) {
            do_bevel = 1;                                   /* near hairpin */
        } else {
            float stretch_sq = 2.0f / denom;
            if (stretch_sq > mitre_limit * mitre_limit) do_bevel = 1;
        }

        if (!do_bevel) {
            float bx = (nax + nbx) / denom;
            float by = (nay + nby) / denom;
            uint32_t vl = emit_v(e, px + bx * half_w, py + by * half_w, 0.0f,
                                    0.0f, 0.0f, 1.0f,
                                    emit_uvs ? u : 0.0f,
                                    emit_uvs ? 1.0f : 0.0f);
            uint32_t vr = emit_v(e, px - bx * half_w, py - by * half_w, 0.0f,
                                    0.0f, 0.0f, 1.0f,
                                    emit_uvs ? u : 0.0f,
                                    emit_uvs ? 0.0f : 0.0f);
            rings[i].left_in  = rings[i].left_out  = vl;
            rings[i].right_in = rings[i].right_out = vr;
            continue;
        }

        /* Bevel: spine-center hub + two offset pairs. Cap fills the gap
         * on the outer side of the turn; the inner-side cap triangle
         * overlaps the incoming quad slightly — acceptable for MVP. */
        uint32_t vc = emit_v(e, px, py, 0.0f, 0.0f, 0.0f, 1.0f,
                              emit_uvs ? u : 0.0f,
                              emit_uvs ? 0.5f : 0.0f);
        uint32_t vl_in  = emit_v(e,
            px + nax * half_w, py + nay * half_w, 0.0f,
            0.0f, 0.0f, 1.0f,
            emit_uvs ? u : 0.0f, emit_uvs ? 1.0f : 0.0f);
        uint32_t vr_in  = emit_v(e,
            px - nax * half_w, py - nay * half_w, 0.0f,
            0.0f, 0.0f, 1.0f,
            emit_uvs ? u : 0.0f, emit_uvs ? 0.0f : 0.0f);
        uint32_t vl_out = emit_v(e,
            px + nbx * half_w, py + nby * half_w, 0.0f,
            0.0f, 0.0f, 1.0f,
            emit_uvs ? u : 0.0f, emit_uvs ? 1.0f : 0.0f);
        uint32_t vr_out = emit_v(e,
            px - nbx * half_w, py - nby * half_w, 0.0f,
            0.0f, 0.0f, 1.0f,
            emit_uvs ? u : 0.0f, emit_uvs ? 0.0f : 0.0f);
        rings[i].left_in   = vl_in;
        rings[i].right_in  = vr_in;
        rings[i].left_out  = vl_out;
        rings[i].right_out = vr_out;

        /* Cap triangulation. Turn direction decides winding so both caps
         * have +Z face normals. cross(d_a, d_b).z = dax*dby - day*dbx.
         *   > 0: left turn  => left-cap winding  (vl_out, vc, vl_in)
         *                      right-cap winding (vr_out, vc, vr_in)
         *   < 0: right turn => mirrored
         *   Full derivation (case analysis on the +90-deg-rotate n-vectors)
         *   is in the module notes. */
        float cross_z = dax * dby - day * dbx;
        if (cross_z >= 0.0f) {
            emit_t(e, vl_out, vc, vl_in);
            emit_t(e, vr_out, vc, vr_in);
        } else {
            emit_t(e, vl_in, vc, vl_out);
            emit_t(e, vr_in, vc, vr_out);
        }
    }

    /* Segment triangulation. */
    for (uint32_t i = 0; i < n_edge; ++i) {
        uint32_t lA = rings[i].left_out;
        uint32_t rA = rings[i].right_out;
        uint32_t lB = rings[i + 1].left_in;
        uint32_t rB = rings[i + 1].right_in;
        emit_t(e, lA, rA, rB);
        emit_t(e, lA, rB, lB);
    }

    free(dx); free(dy); free(cum); free(rings);
    return total_len;
}
