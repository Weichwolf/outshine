/* FlightBox FDM — solar and lunar position.
 *
 * Pure astronomy: no state, no I/O. Given a time and a place, where are the sun and moon?
 * The aircraft computes this and sends it down as telemetry so the renderer can light the world
 * and place the real stars — the renderer never needs a clock or an almanac of its own.
 *
 * Low-precision formulae (good to ~a degree), which is far beyond what a flight simulator needs:
 * being half a degree off makes no visible difference to a sunset.
 */
#ifndef FB_EPHEMERIS_H
#define FB_EPHEMERIS_H
#include <time.h>

/* Julian day from a POSIX timestamp. */
double fb_julian_day(time_t t);

/* Sun position for an observer at lat/lon (degrees).
 * el = elevation above the horizon (deg, negative = below), az = azimuth (deg, 0=N, 90=E). */
void   fb_sun_pos(double jd, double lat, double lon, double *el, double *az);

/* Moon position, plus the illuminated fraction (0 = new, 1 = full).
 * Simplified series — visually accurate placement, not an ephemeris table. */
void   fb_moon_pos(double jd, double lat, double lon, double *el, double *az, double *illum);

#endif /* FB_EPHEMERIS_H */
