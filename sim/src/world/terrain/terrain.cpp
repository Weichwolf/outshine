/* Terrarium-RGB PNG -> float heightfield -> triangulated mesh; API contract in terrain.h.
 * Der WINDING-BEWEIS (algebraisch, nicht ergruent — bei einem Testfehler erst die Mathematik
 * pruefen) und die Flaechengewichtung der Normalen: doc/world/terrain.md, Abschnitt 5.1. */

#include "terrain.h"
#include "mesh.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* stb_image has cast-qual and unused-but-set hits under -Wall -Wextra -Wpedantic -Werror; the
 * relaxation stays scoped to this include. */
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
/* JPEG stays ENABLED (upstream disables it): Esri aerial imagery is JPEG, and two stb_image
 * implementations in one link would collide — so the renderer shares this one. */
#define STBI_NO_BMP
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_GIF
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_NO_STDIO       /* no file IO -- we only decode from memory */
#define STBI_ASSERT(x) ((void)0)

#include "stb_image.h"

#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif

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

/* kept in sync with tiles/osmmesh/terrain.c */

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
    /* stb_image takes int for the length. */
    if (png_len > (size_t)INT32_MAX) return OSMMESH_TERRAIN_ERR_ARG;

    int w = 0, h = 0, channels = 0;
    /* Force RGB; stb_image drops alpha. */
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

    /* h = R*256 + G + B/256 - 32768; float32 is lossless here (1 ULP ~ 1e-3 m for |h| < 32768). */
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
    /* Exact division, so the sampled grid covers the full tile. */
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
    uvs = (float *)malloc((size_t)NV * 2 * sizeof(float));

    if (!positions || !indices || !uvs ||
        (opts->compute_normals && !normals)) {
        free(positions); free(indices); free(normals); free(uvs);
        return OSMMESH_TERRAIN_ERR_OOM;
    }

    /* double accumulation against step drift over the 256-pixel span; float only at store. */
    double inv_cm1 = 1.0 / (double)(cols_out - 1);
    double inv_rm1 = 1.0 / (double)(rows_out - 1);
    double tile_w_e = map->scale_e * (double)map->extent;   /* meters */
    double tile_h_n = map->scale_n * (double)map->extent;   /* meters, NEG */

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

            /* UV origin bottom-left (GL): v grows northward, so v = 1 - fr. */
            uvs[2*vi + 0] = (float)fc;
            uvs[2*vi + 1] = (float)(1.0 - fr);
        }
    }

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
            float nx = ay*bz - az*by;
            float ny = az*bx - ax*bz;
            float nz = ax*by - ay*bx;
            normals[3*i0+0] += nx; normals[3*i0+1] += ny; normals[3*i0+2] += nz;
            normals[3*i1+0] += nx; normals[3*i1+1] += ny; normals[3*i1+2] += nz;
            normals[3*i2+0] += nx; normals[3*i2+1] += ny; normals[3*i2+2] += nz;
        }
        for (uint32_t v = 0; v < NV; v++) {
            float *n = &normals[3*v];
            float m2 = n[0]*n[0] + n[1]*n[1] + n[2]*n[2];
            if (m2 > 0.0f) {
                float inv = 1.0f / sqrtf(m2);
                n[0] *= inv; n[1] *= inv; n[2] *= inv;
            } else {
                /* Degenerate: default to up. */
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
