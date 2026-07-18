/* libosmmesh/src/terrain.c
 *
 * Terrarium-RGB PNG -> float heightfield -> triangulated mesh.
 * See include/osmmesh/terrain.h for the API contract.
 *
 * ========================================================================
 *  Winding / orientation proof (do not "just flip it" if a test fails --
 *  verify the math first).
 * ========================================================================
 *
 * The input grid is row-major with row 0 = NORTH edge, col 0 = WEST edge.
 * We place vertex (r, c) at:
 *
 *     local_x = c * (extent / (cols_out - 1))
 *     local_y = r * (extent / (rows_out - 1))
 *
 * The tile_enu_map apply function turns (lx, ly) into
 *
 *     e = origin_e + lx * scale_e      (scale_e > 0)
 *     n = origin_n + ly * scale_n      (scale_n < 0: ENU n grows north,
 *                                        r/local_y grow south)
 *
 * Up (z) is the height in meters from the grid.
 *
 * With that mapping:
 *   - P(r+1, c)   has n smaller than P(r, c)    (south)
 *   - P(r,   c+1) has e larger  than P(r, c)    (east)
 *
 * For triangle (r,c), (r+1,c), (r+1,c+1):
 *   edge A = P(r+1,c)   - P(r,c)   = (0, scale_n*dy, 0) with scale_n<0,dy>0
 *           = (0, -, 0)   -- pointing south
 *   edge B = P(r+1,c+1) - P(r,c)   = (+, -, 0) -- pointing south-east
 *   A x B = (edge_A.y * edge_B.z - edge_A.z * edge_B.y,
 *            edge_A.z * edge_B.x - edge_A.x * edge_B.z,
 *            edge_A.x * edge_B.y - edge_A.y * edge_B.x)
 *         = (0, 0, 0 - (-)*(+))
 *         = (0, 0, positive)  -- points UP, good.
 *
 * For triangle (r,c), (r+1,c+1), (r,c+1):
 *   edge A = P(r+1,c+1) - P(r,c)   = (+, -, 0)
 *   edge B = P(r,  c+1) - P(r,c)   = (+, 0,  0)
 *   A x B = (0, 0, (+)*(0) - (-)*(+))
 *         = (0, 0, positive)  -- points UP, good.
 *
 * Both tris have CCW winding when viewed from +z (up), so this ordering
 * is consistent with right-handed normals pointing skyward. This was
 * verified algebraically above, not by running tests and flipping until
 * green. The test suite cross-checks by asserting normal.z ~ 1 for a
 * flat grid, which triggers if the math above is wrong.
 *
 * ========================================================================
 *  Normals: per-face, area-weighted, accumulated per vertex, normalized
 *  at the end. Area-weighting falls out of NOT normalizing the face
 *  normal before accumulation (the un-normalized cross product has
 *  magnitude = 2 * face area). For a flat grid all faces have the same
 *  normal (0,0,1) so the result is exactly (0,0,1) after re-normalize.
 * ========================================================================
 */

#include "osmmesh/terrain.h"
#include "osmmesh/mesh.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Silence stb_image warnings locally. stb_image has a handful of cast-qual
 * and unused-but-set-variable hits under our -Wall -Wextra -Wpedantic
 * -Werror flags. Keep the relaxation scoped to this include. */
#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wcast-qual"
#  pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#  pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#  pragma GCC diagnostic ignored "-Wdouble-promotion"
#  pragma GCC diagnostic ignored "-Wtype-limits"
#  pragma GCC diagnostic ignored "-Wsign-compare"
#  pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif

#define STB_IMAGE_IMPLEMENTATION
/* JPEG stays ENABLED (upstream disabled it): the command center decodes Esri aerial imagery,
 * which is JPEG, and this is the only image decoder we ship. Two stb_image implementations in
 * one link would collide, so the renderer shares this one. stb_image is the one genuinely
 * third-party piece here (see src/3rdparty/README.md); osmmesh itself is FlightBox code. */
#define STBI_NO_BMP
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_GIF
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_NO_STDIO       /* no file IO -- we only decode from memory */
#define STBI_ASSERT(x) ((void)0)

#include "../src/3rdparty/stb_image.h"

#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif

/* ========================================================================
 *  Shared mesh container
 * ====================================================================== */

void osmmesh_mesh_free(osmmesh_mesh *m)
{
    if (!m) return;
    free(m->positions); m->positions = NULL;
    free(m->normals);   m->normals   = NULL;
    free(m->uvs);       m->uvs       = NULL;
    free(m->indices);   m->indices   = NULL;
    m->n_vertices  = 0;
    m->n_triangles = 0;
}

/* ========================================================================
 *  Terrarium PNG decode
 * ====================================================================== */

void osmmesh_terrain_grid_free(osmmesh_terrain_grid *grid)
{
    if (!grid) return;
    free(grid->heights);
    grid->heights = NULL;
    grid->rows = 0;
    grid->cols = 0;
}

int osmmesh_terrain_decode_png(const uint8_t *png_data, size_t png_len,
                                osmmesh_terrain_grid *out)
{
    if (!out) return OSMMESH_TERRAIN_ERR_ARG;
    memset(out, 0, sizeof(*out));
    if (!png_data || png_len == 0) return OSMMESH_TERRAIN_ERR_ARG;
    /* stb_image takes int for length; guard against >2GiB tiles (never
     * happens in practice but silence the conversion). */
    if (png_len > (size_t)INT32_MAX) return OSMMESH_TERRAIN_ERR_ARG;

    int w = 0, h = 0, channels = 0;
    /* Force RGB (3 channels); stb_image drops alpha for us. */
    stbi_uc *px = stbi_load_from_memory(png_data, (int)png_len,
                                         &w, &h, &channels, STBI_rgb);
    if (!px) {
        fprintf(stderr, "osmmesh_terrain_decode_png: %s\n", stbi_failure_reason());
        return OSMMESH_TERRAIN_ERR_DECODE;
    }
    if (w <= 0 || h <= 0) {
        stbi_image_free(px);
        return OSMMESH_TERRAIN_ERR_DECODE;
    }

    size_t n = (size_t)w * (size_t)h;
    float *heights = (float *)malloc(n * sizeof(float));
    if (!heights) {
        stbi_image_free(px);
        return OSMMESH_TERRAIN_ERR_OOM;
    }

    /* Terrarium: h = R*256 + G + B/256 - 32768. All arithmetic in float32
     * is lossless at this magnitude (1 ULP ~ 1e-3 m for |h| < 32768). */
    for (size_t i = 0; i < n; i++) {
        const stbi_uc *p = px + i * 3;
        float H = (float)p[0] * 256.0f
                + (float)p[1]
                + (float)p[2] * (1.0f / 256.0f)
                - 32768.0f;
        heights[i] = H;
    }
    stbi_image_free(px);

    out->heights = heights;
    out->rows = (uint32_t)h;
    out->cols = (uint32_t)w;
    return OSMMESH_TERRAIN_OK;
}

/* ========================================================================
 *  Mesh build
 * ====================================================================== */

int osmmesh_terrain_build_mesh(const osmmesh_terrain_grid *grid,
                                const osmmesh_tile_enu_map *map,
                                const osmmesh_terrain_build_opts *opts,
                                osmmesh_mesh *mesh)
{
    if (!grid || !map || !opts || !mesh) return OSMMESH_TERRAIN_ERR_ARG;
    if (!grid->heights) return OSMMESH_TERRAIN_ERR_ARG;
    if (grid->rows < 2 || grid->cols < 2) return OSMMESH_TERRAIN_ERR_ARG;
    if (opts->stride == 0) return OSMMESH_TERRAIN_ERR_ARG;
    if (opts->add_skirt) return OSMMESH_TERRAIN_ERR_ARG;  /* not implemented */
    if (map->extent == 0) return OSMMESH_TERRAIN_ERR_ARG;

    memset(mesh, 0, sizeof(*mesh));

    uint32_t rows = grid->rows, cols = grid->cols;
    uint32_t S = opts->stride;
    /* Stride must divide (rows-1) and (cols-1) exactly so the sampled grid
     * covers the full tile. */
    if (((rows - 1) % S) != 0 || ((cols - 1) % S) != 0) {
        return OSMMESH_TERRAIN_ERR_ARG;
    }
    uint32_t rows_out = (rows - 1) / S + 1;
    uint32_t cols_out = (cols - 1) / S + 1;

    uint64_t n_vertices64  = (uint64_t)rows_out * (uint64_t)cols_out;
    uint64_t n_triangles64 = (uint64_t)(rows_out - 1) * (uint64_t)(cols_out - 1) * 2ull;
    if (n_vertices64 > UINT32_MAX || n_triangles64 > UINT32_MAX) {
        return OSMMESH_TERRAIN_ERR_ARG;
    }
    uint32_t NV = (uint32_t)n_vertices64;
    uint32_t NT = (uint32_t)n_triangles64;

    float    *positions = (float    *)malloc((size_t)NV * 3 * sizeof(float));
    uint32_t *indices   = (uint32_t *)malloc((size_t)NT * 3 * sizeof(uint32_t));
    float    *normals   = NULL;
    float    *uvs       = NULL;
    if (opts->compute_normals) {
        normals = (float *)calloc((size_t)NV * 3, sizeof(float));
    }
    /* Always emit UVs: cheap, one float2 per vertex, useful downstream. */
    uvs = (float *)malloc((size_t)NV * 2 * sizeof(float));

    if (!positions || !indices || !uvs ||
        (opts->compute_normals && !normals)) {
        free(positions); free(indices); free(normals); free(uvs);
        return OSMMESH_TERRAIN_ERR_OOM;
    }

    /* Per-grid-step local-coord delta. Using double for the accumulation
     * to avoid step drift across the 256-pixel span; cast to float at
     * store. */
    double inv_cm1 = 1.0 / (double)(cols_out - 1);
    double inv_rm1 = 1.0 / (double)(rows_out - 1);
    double tile_w_e = map->scale_e * (double)map->extent;   /* meters */
    double tile_h_n = map->scale_n * (double)map->extent;   /* meters, NEG */

    /* -- Vertices ----------------------------------------------------- */
    for (uint32_t r = 0; r < rows_out; r++) {
        double fr = (double)r * inv_rm1;           /* [0..1], north->south */
        uint32_t src_r = r * S;                     /* row in input grid */
        for (uint32_t c = 0; c < cols_out; c++) {
            double fc = (double)c * inv_cm1;        /* [0..1], west->east */
            uint32_t src_c = c * S;
            uint32_t vi = r * cols_out + c;

            double e = map->origin_e + fc * tile_w_e;
            double n = map->origin_n + fr * tile_h_n;  /* tile_h_n is <0 */
            float  u = grid->heights[(size_t)src_r * cols + src_c];

            positions[3*vi + 0] = (float)e;
            positions[3*vi + 1] = (float)n;
            positions[3*vi + 2] = u;

            /* UV origin bottom-left, standard GL convention: v grows with n
             * (northward), so v = 1 - fr. */
            uvs[2*vi + 0] = (float)fc;
            uvs[2*vi + 1] = (float)(1.0 - fr);
        }
    }

    /* -- Indices ------------------------------------------------------ */
    uint32_t *w = indices;
    for (uint32_t r = 0; r < rows_out - 1; r++) {
        for (uint32_t c = 0; c < cols_out - 1; c++) {
            uint32_t v00 = r       * cols_out + c;
            uint32_t v10 = (r + 1) * cols_out + c;
            uint32_t v11 = (r + 1) * cols_out + (c + 1);
            uint32_t v01 = r       * cols_out + (c + 1);
            /* See top-of-file winding proof. CCW from +z (up). */
            *w++ = v00; *w++ = v10; *w++ = v11;
            *w++ = v00; *w++ = v11; *w++ = v01;
        }
    }

    /* -- Normals (area-weighted face-normal accumulation) ------------- */
    if (opts->compute_normals) {
        for (uint32_t t = 0; t < NT; t++) {
            uint32_t i0 = indices[3*t + 0];
            uint32_t i1 = indices[3*t + 1];
            uint32_t i2 = indices[3*t + 2];
            const float *p0 = &positions[3*i0];
            const float *p1 = &positions[3*i1];
            const float *p2 = &positions[3*i2];
            float ax = p1[0] - p0[0];
            float ay = p1[1] - p0[1];
            float az = p1[2] - p0[2];
            float bx = p2[0] - p0[0];
            float by = p2[1] - p0[1];
            float bz = p2[2] - p0[2];
            /* cross = a x b (un-normalized, magnitude = 2*area). */
            float nx = ay*bz - az*by;
            float ny = az*bx - ax*bz;
            float nz = ax*by - ay*bx;
            normals[3*i0+0] += nx; normals[3*i0+1] += ny; normals[3*i0+2] += nz;
            normals[3*i1+0] += nx; normals[3*i1+1] += ny; normals[3*i1+2] += nz;
            normals[3*i2+0] += nx; normals[3*i2+1] += ny; normals[3*i2+2] += nz;
        }
        /* Normalize per vertex. */
        for (uint32_t v = 0; v < NV; v++) {
            float *n = &normals[3*v];
            float m2 = n[0]*n[0] + n[1]*n[1] + n[2]*n[2];
            if (m2 > 0.0f) {
                float inv = 1.0f / sqrtf(m2);
                n[0] *= inv; n[1] *= inv; n[2] *= inv;
            } else {
                /* Degenerate (should not happen for a well-formed grid).
                 * Default to up. */
                n[0] = 0.0f; n[1] = 0.0f; n[2] = 1.0f;
            }
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
