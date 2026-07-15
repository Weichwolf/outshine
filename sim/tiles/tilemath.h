/* FlightBox tiles — Web-Mercator tile math.
 *
 * Pure functions, no I/O, no state: this header is the unit-testable core of the world-data
 * service. Everything that converts between geography and tile addresses lives HERE and only
 * here — a stray lat/lon conversion elsewhere is how a missing cos(lat) once put the whole
 * world 1.6x too far east.
 *
 * Convention: XYZ ("Google") scheme — y counts from NORTH. Tilezen/AWS terrarium and Esri
 * imagery both use it. TMS flips y; we never use TMS.
 */
#ifndef FB_TILEMATH_H
#define FB_TILEMATH_H
#include <math.h>
#include <stdint.h>
#include <stddef.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Web-Mercator latitude limit (the projection is undefined at the poles). */
#define FB_MERC_LAT_MAX 85.0511287798

static inline double fb_clampd(double v, double lo, double hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/* Geographic -> continuous tile coordinates at zoom z. The integer part is the tile index,
 * the fractional part is the position WITHIN that tile (0..1, y downward from the north edge). */
static inline void fb_geo_to_tile(double lat, double lon, int z, double *tx, double *ty) {
    double n = ldexp(1.0, z);                       /* 2^z */
    lat = fb_clampd(lat, -FB_MERC_LAT_MAX, FB_MERC_LAT_MAX);
    lon = fb_clampd(lon, -180.0, 180.0);
    double lr = lat * M_PI / 180.0;
    *tx = (lon + 180.0) / 360.0 * n;
    *ty = (1.0 - log(tan(lr) + 1.0 / cos(lr)) / M_PI) / 2.0 * n;
}

/* Tile corner (north-west) back to geographic — inverse of the above. */
static inline void fb_tile_to_geo(double tx, double ty, int z, double *lat, double *lon) {
    double n = ldexp(1.0, z);
    *lon = tx / n * 360.0 - 180.0;
    double m = M_PI * (1.0 - 2.0 * ty / n);
    *lat = atan(sinh(m)) * 180.0 / M_PI;
}

/* Ground resolution (metres per pixel) for a 256 px tile at this latitude/zoom. */
static inline double fb_tile_res_m(double lat, int z) {
    return 40075016.686 * cos(lat * M_PI / 180.0) / (ldexp(1.0, z) * 256.0);
}

/* Wrap x cyclically and clamp y — tile indices at the antimeridian/poles. Returns 0 if the
 * tile is off the map in y (nothing to fetch), 1 if usable. */
static inline int fb_tile_wrap(int z, long *x, long *y) {
    long n = (long)ldexp(1.0, z);
    *x = ((*x % n) + n) % n;                        /* longitude wraps */
    if (*y < 0 || *y >= n) return 0;                /* latitude does not */
    return 1;
}

/* Bilinear sample of a row-major grid at continuous (gx, gy) in pixel units, clamped at the
 * edges. Used to read a height field at an exact geographic position. */
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

#endif /* FB_TILEMATH_H */
