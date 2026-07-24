#ifndef FB_STARS_H
#define FB_STARS_H
#include <stddef.h>
#include <stdint.h>

#define FB_STAR_BANDS 4

/* Load the baked magnitude bands (band0..3.bin) from dir into memory once. Missing files are not
 * fatal: that band simply serves empty. */
void fb_stars_init(const char *dir);

/* Serve one band's raw 6-B/star blob. Returns 1 for a valid band (n may be 0), 0 out of range. */
int  fb_stars_band(int band, const uint8_t **out, size_t *n);

#endif
