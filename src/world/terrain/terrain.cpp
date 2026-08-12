/* Terrarium-RGB PNG -> float heightfield -> node grid; API contract in terrain.h. */

#include "terrain.h"
#include "mesh.h"
#include "tilemath.h"

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
    m->n_vertices  = 0;
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
    if (map->extent == 0) return OSMMESH_TERRAIN_ERR_ARG;

    memset(mesh, 0, sizeof(*mesh));

    uint32_t rows = grid->rows, cols = grid->cols;
    uint32_t S = opts->stride;
    /* Exact division, so the sampled grid covers the full tile. */
    if (((rows - 1) % S) != 0 || ((cols - 1) % S) != 0) {
        return OSMMESH_TERRAIN_ERR_ARG;
    }
    uint32_t rows_out = osmmesh_terrain_postings(rows, S);
    uint32_t cols_out = osmmesh_terrain_postings(cols, S);

    uint64_t n_vertices64  = (uint64_t)rows_out * (uint64_t)cols_out;
    if (n_vertices64 > UINT32_MAX) {
        return OSMMESH_TERRAIN_ERR_ARG;
    }
    uint32_t NV = (uint32_t)n_vertices64;

    float *positions = (float *)malloc((size_t)NV * 3 * sizeof(float));
    if (!positions) return OSMMESH_TERRAIN_ERR_OOM;

    double tile_w_e = map->scale_e * (double)map->extent;   /* meters */
    double tile_h_n = map->scale_n * (double)map->extent;   /* meters, NEG */

    for (uint32_t r = 0; r < rows_out; r++) {
        double fr = osmmesh_terrain_posting_frac(r, rows_out);   /* [0..1], north->south */
        for (uint32_t c = 0; c < cols_out; c++) {
            double fc = osmmesh_terrain_posting_frac(c, cols_out);   /* [0..1], west->east */
            uint32_t vi = r * cols_out + c;

            double e = map->origin_e + fc * tile_w_e;
            double n = map->origin_n + fr * tile_h_n;  /* tile_h_n is <0 */
            /* The clamp at fr/fc = 0 and 1 is exact rather than approximate: the stitch has already
             * replaced the edge row/column with the mean of the two texels straddling the border,
             * which IS the field's value on the border. */
            float  u = osmmesh_terrain_posting_height(grid, fc, fr);

            positions[3*vi + 0] = (float)e;
            positions[3*vi + 1] = (float)n;
            positions[3*vi + 2] = u;
        }
    }

    mesh->positions   = positions;
    mesh->n_vertices  = NV;
    return OSMMESH_TERRAIN_OK;
}
