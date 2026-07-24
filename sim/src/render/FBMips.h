/* FlightBox — sRGB mip-pyramid builder, shared by the native terrain path (FBTerrainLoader.cpp) and
 * the off-thread tile worker (FBTileWorkerMain.cpp). Colour is averaged in LINEAR light (decode ->
 * average -> re-encode) so the distance doesn't darken; alpha is already linear, averaged directly.
 * The pyramid is packed CONTIGUOUS: level 0 (ts*ts), level 1 ((ts/2)^2), ... down to 1x1, RGBA8
 * throughout. */
#ifndef FBMIPS_H
#define FBMIPS_H
#include <stdint.h>
#include <string.h>
#include <math.h>

static float fb_srgb_lin_[256];
static int fb_srgb_init_ = 0;
static inline void fb_srgb_lut_(void) {
  if (fb_srgb_init_) return;
  for (int i = 0; i < 256; i++) {
    float s = i / 255.0f;
    fb_srgb_lin_[i] = s <= 0.04045f ? s / 12.92f : powf((s + 0.055f) / 1.055f, 2.4f);
  }
  fb_srgb_init_ = 1;
}
static inline uint8_t fb_lin_srgb_(float l) {
  l = l <= 0.0031308f ? l * 12.92f : 1.055f * powf(l, 1.0f / 2.4f) - 0.055f;
  int v = (int)(l * 255.0f + 0.5f);
  return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
}

/* Mip levels for a ts*ts image (floor(log2 ts)+1). */
static inline int fb_mip_count(int ts) { int n = 1; while (ts > 1) { ts >>= 1; n++; } return n; }

/* Total bytes of the full pyramid for ts (level 0..N packed contiguous). */
static inline int fb_pyramid_bytes(int ts) {
  int b = 0, w = ts;
  for (;;) { b += w * w * 4; if (w == 1) break; w >>= 1; }
  return b;
}

/* Build the whole pyramid from a ts*ts RGBA8-sRGB level-0 image into `dst`
 * (fb_pyramid_bytes(ts) long). */
static inline void fb_build_pyramid(const uint8_t *rgba, int ts, uint8_t *dst) {
  fb_srgb_lut_();
  memcpy(dst, rgba, (size_t)ts * ts * 4);
  const uint8_t *cur = dst;
  uint8_t *next = dst + (size_t)ts * ts * 4;
  int w = ts;
  while (w > 1) {
    int nw = w >> 1;
    for (int y = 0; y < nw; y++)
      for (int x = 0; x < nw; x++) {
        const uint8_t *a = &cur[(((size_t)2 * y) * w + 2 * x) * 4];
        const uint8_t *b = &cur[(((size_t)2 * y) * w + 2 * x + 1) * 4];
        const uint8_t *c = &cur[(((size_t)(2 * y + 1)) * w + 2 * x) * 4];
        const uint8_t *d = &cur[(((size_t)(2 * y + 1)) * w + 2 * x + 1) * 4];
        uint8_t *o = &next[((size_t)y * nw + x) * 4];
        for (int ch = 0; ch < 3; ch++)
          o[ch] = fb_lin_srgb_(0.25f * (fb_srgb_lin_[a[ch]] + fb_srgb_lin_[b[ch]] +
                                        fb_srgb_lin_[c[ch]] + fb_srgb_lin_[d[ch]]));
        o[3] = (uint8_t)(((int)a[3] + b[3] + c[3] + d[3] + 2) >> 2);
      }
    cur = next;
    next += (size_t)nw * nw * 4;
    w = nw;
  }
}

#endif /* FBMIPS_H */
