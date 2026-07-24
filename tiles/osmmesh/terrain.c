/* tiles/osmmesh/terrain.c -- server subset (T8 split, 2026-07-24).
 *
 * Terrarium-RGB PNG -> float heightfield. See terrain.h (this directory) for the API contract and
 * osmmesh.h for why this is a deliberately smaller copy of the client's full terrain.c
 * (temp/geo/osmmesh/src/terrain.c, which also builds ENU/ECEF meshes -- a rendering concern this
 * server binary has no use for; elev.c only ever samples single points from the decoded grid).
 */

#include "osmmesh/terrain.h"

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
/* The one genuinely third-party piece here (see README.md, this directory); osmmesh itself is
 * FlightBox code. JPEG stays enabled (upstream disabled it): bake.c/raster.c decode JPEG aerial
 * imagery and this is the only image decoder this binary ships, so this is the ONE
 * implementation for the whole server link (bake.c's own stb_image.h include has none). */
#define STBI_NO_BMP
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_GIF
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_NO_STDIO       /* no file IO -- we only decode from memory */
#define STBI_ASSERT(x) ((void)0)

#include "../vendor/stb_image.h"

#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif

/* ========================================================================
 *  Terrarium PNG decode
 * ====================================================================== */

/* kept in sync with temp/geo/osmmesh/src/terrain.c */

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
