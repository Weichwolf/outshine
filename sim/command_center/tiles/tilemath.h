/* FlightBox renderer — tile geography: fractional tile coords, metres-per-tile, and a chunk's
 * on-screen size in FBO pixels. Pure arithmetic; the walk and children_ready both project through
 * it. */
#ifndef W3_TILES_TILEMATH_H
#define W3_TILES_TILEMATH_H
/* Fractional tile coords -- the camera sits between tiles, and the tree needs to know where.
 * osmmesh_geo_to_tile only answers in whole tiles. */
static void w3_geo_to_tile_f(double lat, double lon, int z, double *tx, double *ty) {
  double n = ldexp(1.0, z), la = lat * M_PI / 180.0;
  *tx = (lon + 180.0) / 360.0 * n;
  *ty = (1.0 - asinh(tan(la)) / M_PI) / 2.0 * n;
}
/* Metres per tile at this zoom and latitude. */
static double w3_tile_span(int z, double lat) {
  return 40075016.686 * cos(lat * M_PI / 180.0) / ldexp(1.0, z);
}
/* On-screen size of a chunk in FBO pixels: span * K / distance, the same projection the SSE uses
 * (K = viewport_height / (2 tan(fov/2))). This is what the texture step is chosen to match, so a
 * chunk's texel density tracks the pixels it actually covers -- and, being the identical arithmetic
 * for a given (z,x,y,camera), it makes w3_walk and w3_children_ready agree by construction rather
 * than by convention. (tx,ty) is the camera's fractional tile coord AT LEVEL z. */
static double w3_chunk_px(int z, long x, long y, double lat, double alt, double tx, double ty) {
  double span = w3_tile_span(z, lat);
  double dx = fabs(tx - ((double)x + 0.5)) - 0.5, dy = fabs(ty - ((double)y + 0.5)) - 0.5;
  if (dx < 0) dx = 0;
  if (dy < 0) dy = 0;
  double horiz = sqrt(dx * dx + dy * dy) * span;
  double dist = sqrt(horiz * horiz + alt * alt);
  if (dist < 1.0) dist = 1.0;
  return span * (double)W3_SSE_K / dist;
}
#endif
