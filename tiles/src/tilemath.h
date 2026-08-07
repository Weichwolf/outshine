#ifndef FB_TILEMATH_H
#define FB_TILEMATH_H
#include <math.h>
#include <stdint.h>
#include <stddef.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define FB_MERC_LAT_MAX 85.0511287798

static inline double fb_clampd(double v, double lo, double hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline void fb_geo_to_tile(double lat, double lon, int z, double *tx, double *ty) {
    double n = ldexp(1.0, z);
    lat = fb_clampd(lat, -FB_MERC_LAT_MAX, FB_MERC_LAT_MAX);
    lon = fb_clampd(lon, -180.0, 180.0);
    double lr = lat * M_PI / 180.0;
    *tx = (lon + 180.0) / 360.0 * n;
    *ty = (1.0 - log(tan(lr) + 1.0 / cos(lr)) / M_PI) / 2.0 * n;
}

static inline void fb_tile_to_geo(double tx, double ty, int z, double *lat, double *lon) {
    double n = ldexp(1.0, z);
    *lon = tx / n * 360.0 - 180.0;
    double m = M_PI * (1.0 - 2.0 * ty / n);
    *lat = atan(sinh(m)) * 180.0 / M_PI;
}

static inline double fb_tile_res_m(double lat, int z) {
    return 40075016.686 * cos(lat * M_PI / 180.0) / (ldexp(1.0, z) * 256.0);
}

static inline int fb_tile_wrap(int z, long *x, long *y) {
    long n = (long)ldexp(1.0, z);
    *x = ((*x % n) + n) % n;
    if (*y < 0 || *y >= n) return 0;
    return 1;
}

/* THE REGISTRATION, and every DEM sampler in this repo goes through this one line. A raster texel is
 * an AREA, not a lattice point: tilezen/joerd writes a Terrarium tile through a GDAL geotransform of
 * the tile's own bbox with res = span/N (joerd/mercator.py), so texel i covers [i/N,(i+1)/N] and its
 * sample sits at (i+0.5)/N. A query at tile fraction f therefore lands at grid index f*N - 0.5 --
 * half a texel west/north of the naive f*N, and one full texel away from f*(N-1) at the far edge. */
static inline double fb_texel_index(double frac, uint32_t n) { return frac * (double)n - 0.5; }

static inline float fb_bilinear(const float *g, uint32_t cols, uint32_t rows, double gx, double gy) {
    if (!g || !cols || !rows) return 0.f;
    gx = fb_clampd(gx, 0.0, (double)cols - 1.0);
    gy = fb_clampd(gy, 0.0, (double)rows - 1.0);
    uint32_t x0 = (uint32_t)gx, y0 = (uint32_t)gy;
    uint32_t x1 = x0 + 1 < cols ? x0 + 1 : x0;
    uint32_t y1 = y0 + 1 < rows ? y0 + 1 : y0;
    double fx = gx - x0, fy = gy - y0;
    double a = g[(size_t)y0 * cols + x0], b = g[(size_t)y0 * cols + x1];
    double c = g[(size_t)y1 * cols + x0], d = g[(size_t)y1 * cols + x1];
    return (float)((a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy);
}

/* ONE texel of tile (x,y); 0 = not resident. Per texel and not per grid, because a grid pointer
 * handed back to the caller dangles the instant another thread evicts that slot (tiles/src/elev.c). */
typedef int (*fb_texel_fn)(void *user, long x, long y, uint32_t col, uint32_t row, float *out);

/* The point query at texel-centre registration. Half a texel from a tile border the four corners lie
 * in up to four different tiles, so the gather crosses them; a neighbour that is not resident falls
 * back to the query tile's own edge texel, which is exactly the old in-tile clamp -- never worse than
 * before, exact as soon as the neighbour lands. */
static inline int fb_dem_bilinear(int z, double tx, double ty, uint32_t n,
                                  fb_texel_fn fetch, void *user, double *out) {
    if (!fetch || !n) return 0;
    long hx = (long)tx, hy = (long)ty;
    if (!fb_tile_wrap(z, &hx, &hy)) return 0;
    const long edge = (long)ldexp(1.0, z) * (long)n;         /* texels along the whole world edge */
    const double gx = fb_texel_index(tx - (double)(long)tx, n) + (double)(hx * (long)n);
    const double gy = fb_texel_index(ty - (double)(long)ty, n) + (double)(hy * (long)n);
    const long ix = (long)floor(gx), iy = (long)floor(gy);
    const double fx = gx - (double)ix, fy = gy - (double)iy;
    double v[4];
    for (int k = 0; k < 4; k++) {
        long px = ix + (k & 1), py = iy + (k >> 1);
        px = ((px % edge) + edge) % edge;                    /* dateline wraps, the poles clamp */
        if (py < 0) py = 0; else if (py >= edge) py = edge - 1;
        long bx = px / (long)n, by = py / (long)n;
        float h = 0.f;
        if (!fetch(user, bx, by, (uint32_t)(px - bx * (long)n), (uint32_t)(py - by * (long)n), &h)) {
            long cx = px, cy = py;
            if (cx < hx * (long)n) cx = hx * (long)n;
            else if (cx >= (hx + 1) * (long)n) cx = (hx + 1) * (long)n - 1;
            if (cy < hy * (long)n) cy = hy * (long)n;
            else if (cy >= (hy + 1) * (long)n) cy = (hy + 1) * (long)n - 1;
            if (!fetch(user, hx, hy, (uint32_t)(cx - hx * (long)n), (uint32_t)(cy - hy * (long)n), &h))
                return 0;
        }
        v[k] = (double)h;
    }
    *out = (v[0] * (1 - fx) + v[1] * fx) * (1 - fy) + (v[2] * (1 - fx) + v[3] * fx) * fy;
    return 1;
}

#endif
