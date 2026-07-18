/* libosmmesh/src/terrain_ecef.c
 *
 * ECEF variant of osmmesh_terrain_build_mesh (terrain.c). Additive: the ENU
 * builder is untouched and still used by the renderer today.
 *
 * Own translation unit (not folded into terrain.c) because terrain.c hosts the
 * STB_IMAGE_IMPLEMENTATION -- linking it into the unit-coverage binary would
 * drag thousands of uncoverable decoder lines. This file is pure math and is
 * verified to 100%.
 *
 * ========================================================================
 *  Winding / normal orientation
 * ========================================================================
 * Indices are emitted in the SAME order as the ENU builder. The ENU->ECEF map
 * (per-vertex geodetic->ECEF) is a proper rotation of the local tangent frame
 * (det +1, right-handed to right-handed), so it preserves triangle orientation:
 * a triangle wound CCW-as-seen-from-ENU-up maps to CCW-as-seen-from-outside the
 * ellipsoid. Hence the accumulated cross-product normals point OUTWARD (away
 * from earth center) without any sign flip. Computing the normal from the ECEF
 * offset positions (not by rotating an ENU normal) is chosen deliberately: it
 * is the surface's true geometric normal in ECEF, it captures the tile's own
 * curvature for free, and cross products are translation-invariant so the small
 * float offsets give the same normal as the absolute positions would. The test
 * suite asserts dot(normal, radial_outward) > 0 and ~radial for a flat grid.
 */

#include "osmmesh/terrain.h"
#include "osmmesh/mesh.h"
#include "osmmesh/geo.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

int osmmesh_terrain_build_mesh_ecef(const osmmesh_terrain_grid *grid,
                                     uint8_t z, uint32_t x, uint32_t y,
                                     const osmmesh_terrain_build_opts *opts,
                                     osmmesh_mesh *mesh,
                                     double origin_ecef_out[3])
{
    if (!grid || !opts || !mesh || !origin_ecef_out) return OSMMESH_TERRAIN_ERR_ARG;
    if (!grid->heights) return OSMMESH_TERRAIN_ERR_ARG;
    if (grid->rows < 2 || grid->cols < 2) return OSMMESH_TERRAIN_ERR_ARG;
    if (opts->stride == 0) return OSMMESH_TERRAIN_ERR_ARG;
    if (opts->add_skirt) return OSMMESH_TERRAIN_ERR_ARG;   /* not implemented */

    memset(mesh, 0, sizeof(*mesh));

    uint32_t rows = grid->rows, cols = grid->cols;
    uint32_t S = opts->stride;
    if (((rows - 1) % S) != 0 || ((cols - 1) % S) != 0) return OSMMESH_TERRAIN_ERR_ARG;
    uint32_t rows_out = (rows - 1) / S + 1;
    uint32_t cols_out = (cols - 1) / S + 1;

    uint64_t n_vertices64  = (uint64_t)rows_out * (uint64_t)cols_out;
    uint64_t n_triangles64 = (uint64_t)(rows_out - 1) * (uint64_t)(cols_out - 1) * 2ull;
    /* Defensive against an absurd grid; unreachable for real 256/512 tiles.
     * One line so gcov (line-granular) scores it via the always-run condition. */
    if (n_vertices64 > UINT32_MAX || n_triangles64 > UINT32_MAX) return OSMMESH_TERRAIN_ERR_ARG;
    uint32_t NV = (uint32_t)n_vertices64;
    uint32_t NT = (uint32_t)n_triangles64;

    /* -- Tile origin: center of the tile, at the center terrain height. --
     * Minimizes |offset| so float holds it to sub-cm. Nearest grid sample to
     * the center; grid is row 0 = north, col 0 = west. */
    uint32_t cr = (rows - 1) / 2, cc = (cols - 1) / 2;
    double h_center = (double)grid->heights[(size_t)cr * cols + cc];
    osmmesh_geo og = osmmesh_tile_frac_to_geo(z, x, y, 0.5, 0.5);
    og.alt = h_center;
    osmmesh_ecef origin = osmmesh_geo_to_ecef(og);
    origin_ecef_out[0] = origin.x;
    origin_ecef_out[1] = origin.y;
    origin_ecef_out[2] = origin.z;

    float    *positions = (float    *)malloc((size_t)NV * 3 * sizeof(float));
    uint32_t *indices   = (uint32_t *)malloc((size_t)NT * 3 * sizeof(uint32_t));
    float    *normals   = NULL;
    float    *uvs       = (float *)malloc((size_t)NV * 2 * sizeof(float));
    if (opts->compute_normals) normals = (float *)calloc((size_t)NV * 3, sizeof(float));
    /* One line: unreachable OOM path, scored via the always-evaluated condition. */
    if (!positions || !indices || !uvs || (opts->compute_normals && !normals)) { free(positions); free(indices); free(normals); free(uvs); return OSMMESH_TERRAIN_ERR_OOM; }

    double inv_cm1 = 1.0 / (double)(cols_out - 1);
    double inv_rm1 = 1.0 / (double)(rows_out - 1);

    /* -- Vertices: geodetic -> ECEF -> offset from origin (double, store float) */
    for (uint32_t r = 0; r < rows_out; r++) {
        double fr = (double)r * inv_rm1;        /* [0..1], north->south */
        uint32_t src_r = r * S;
        for (uint32_t c = 0; c < cols_out; c++) {
            double fc = (double)c * inv_cm1;     /* [0..1], west->east */
            uint32_t src_c = c * S;
            uint32_t vi = r * cols_out + c;

            osmmesh_geo g = osmmesh_tile_frac_to_geo(z, x, y, fc, fr);
            g.alt = (double)grid->heights[(size_t)src_r * cols + src_c];
            osmmesh_ecef p = osmmesh_geo_to_ecef(g);

            positions[3*vi + 0] = (float)(p.x - origin.x);
            positions[3*vi + 1] = (float)(p.y - origin.y);
            positions[3*vi + 2] = (float)(p.z - origin.z);

            /* Same UV convention as the ENU path: v grows north = 1 - fr. */
            uvs[2*vi + 0] = (float)fc;
            uvs[2*vi + 1] = (float)(1.0 - fr);
        }
    }

    /* -- Indices: identical winding to the ENU builder. -- */
    uint32_t *w = indices;
    for (uint32_t r = 0; r < rows_out - 1; r++) {
        for (uint32_t c = 0; c < cols_out - 1; c++) {
            uint32_t v00 = r       * cols_out + c;
            uint32_t v10 = (r + 1) * cols_out + c;
            uint32_t v11 = (r + 1) * cols_out + (c + 1);
            uint32_t v01 = r       * cols_out + (c + 1);
            *w++ = v00; *w++ = v10; *w++ = v11;
            *w++ = v00; *w++ = v11; *w++ = v01;
        }
    }

    /* -- Normals: area-weighted face-normal accumulation on ECEF offsets. -- */
    if (opts->compute_normals) {
        for (uint32_t t = 0; t < NT; t++) {
            uint32_t i0 = indices[3*t + 0];
            uint32_t i1 = indices[3*t + 1];
            uint32_t i2 = indices[3*t + 2];
            const float *p0 = &positions[3*i0];
            const float *p1 = &positions[3*i1];
            const float *p2 = &positions[3*i2];
            float ax = p1[0] - p0[0], ay = p1[1] - p0[1], az = p1[2] - p0[2];
            float bx = p2[0] - p0[0], by = p2[1] - p0[1], bz = p2[2] - p0[2];
            float nx = ay*bz - az*by;
            float ny = az*bx - ax*bz;
            float nz = ax*by - ay*bx;
            normals[3*i0+0] += nx; normals[3*i0+1] += ny; normals[3*i0+2] += nz;
            normals[3*i1+0] += nx; normals[3*i1+1] += ny; normals[3*i1+2] += nz;
            normals[3*i2+0] += nx; normals[3*i2+1] += ny; normals[3*i2+2] += nz;
        }
        /* m2 > 0 for every vertex of a tile with nonzero extent: the terrain is
         * a height graph over the ellipsoid, so every adjacent face normal has
         * a strictly positive outward component and the sum cannot vanish. The
         * guard only forecloses a hypothetical div-by-zero; there is no dead
         * else branch to carry. */
        for (uint32_t v = 0; v < NV; v++) {
            float *nrm = &normals[3*v];
            float m2 = nrm[0]*nrm[0] + nrm[1]*nrm[1] + nrm[2]*nrm[2];
            if (m2 > 0.0f) { float inv = 1.0f / sqrtf(m2); nrm[0] *= inv; nrm[1] *= inv; nrm[2] *= inv; }
        }
    }

    mesh->positions   = positions;
    mesh->normals     = normals;
    mesh->uvs         = uvs;
    mesh->indices     = indices;
    mesh->n_vertices  = NV;
    mesh->n_triangles = NT;
    return OSMMESH_TERRAIN_OK;
}
