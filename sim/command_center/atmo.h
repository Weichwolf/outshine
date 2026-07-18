/* The atmosphere of ONE frame, as pure arithmetic. No GL, no globals, no side effects.
 *
 * These nine lines used to sit inside world3d_render_scene as locals, under a comment that read
 * like a sky prologue -- and that is how they were understood: as the sky's business. They are
 * not. `haze`, `light` and `sun` are read by the TERRAIN pass (uHaze/uLight/uSun) and `haze` again
 * by the building/wire pass. Three passes share them, and it worked only because they happened to
 * be in scope: per-frame shared state with no name and therefore no owner. That is the same shape
 * as the cache thrash (a size that was a parameter beside the key, stored nowhere), one level
 * subtler -- this state did not even have a name to argue about.
 *
 * Now it has one. Whoever needs the atmosphere takes a `const w3_atmo*`; nobody re-derives it, so
 * `0.20 + 0.80*day` exists once. A second copy would make the agreement a convention instead of a
 * type, which is exactly what we refuse everywhere else here.
 *
 * And the reason it is a separate header rather than a tidier block: it is pure, so it is
 * TESTABLE. Every pixel of the image hangs off these numbers -- the day/night ramp, the horizon
 * colour, the light level -- and until now nothing could check them, because they were welded to
 * glUniform calls. Same argument as chunkmesh.h, one storey up. See test/unit/test_atmo.c.
 *
 * ENU render space throughout: E=+X, up=+Y, N=-Z. Angles in degrees, as they arrive on the wire.
 */
#ifndef W3_ATMO_H
#define W3_ATMO_H

#include "protocol.h"
#include <math.h>

typedef struct {
  float sun[3];   /* unit direction TO the sun */
  float moon[3];  /* unit direction TO the moon */
  float haze[3];  /* horizon/fog colour right now (sun elevation + cloud) */
  float day;      /* 0 = night (sun <= -6 deg) .. 1 = day (sun >= +6 deg) */
  float light;    /* scene light level: 0.20 at night, 1.00 by day */
  float cloud;    /* total cover 0..1, clamped */
  float moon_ph;  /* illuminated fraction, 0 new .. 1 full */
  float sun_disc; /* 1 = draw the sun disc + glow (EVS/real sky); 0 = plain map-blue (SVS) */
} w3_atmo;

/* ONE daylight factor from sun elevation, shared by sky, ground AND star visibility so they never
 * disagree. Replaces two different `day` curves that were up to 13 points apart through twilight (a
 * CPU one linear in degrees, a shader one on sin(el)); the sky now takes THIS value as a uniform.
 * Physically it tracks twilight: full day just above the horizon, fading through civil twilight,
 * fully dark by nautical twilight (~ -9 deg) so deep night is genuinely dark. smoothstep = smooth
 * monotone S-curve. EVS only -- SVS pins day=1 (constant synthetic daylight). */
static float w3_daylight(float sun_el_deg) {
  float t = (sun_el_deg + 9.f) / 12.f;
  if (t < 0.f) t = 0.f;
  if (t > 1.f) t = 1.f;
  return t * t * (3.f - 2.f * t);
}

/* `have` = 0 means no telemetry yet; the defaults are the ones the renderer has always used for
 * that case (afternoon sun, some cloud) so a fresh page is not black. Keep them: they are what a
 * browser shows in the second before the first packet arrives. */
static w3_atmo w3_atmo_from(const telem_packet_t *t, int have) {
  const float RAD = (float)M_PI / 180.f;
  w3_atmo a;

  float sun_el = have ? t->sun_el : 35.f, sun_az = have ? t->sun_az : 150.f;
  float moon_el = have ? t->moon_el : 25.f, moon_az = have ? t->moon_az : 300.f;
  a.moon_ph = have ? t->moon_phase : 0.5f;

  a.cloud = have ? t->cloud : 0.3f;
  if (a.cloud < 0) a.cloud = 0;
  if (a.cloud > 1) a.cloud = 1;

  float ce = cosf(sun_el * RAD);
  a.sun[0] = ce * sinf(sun_az * RAD);
  a.sun[1] = sinf(sun_el * RAD);
  a.sun[2] = -ce * cosf(sun_az * RAD);

  float me = cosf(moon_el * RAD);
  a.moon[0] = me * sinf(moon_az * RAD);
  a.moon[1] = sinf(moon_el * RAD);
  a.moon[2] = -me * cosf(moon_az * RAD);

  /* Daylight -- ONE curve now (w3_daylight, above). This used to disagree with the sky shader: a
   * CPU value linear in degrees here vs a shader value on sin(el), up to 13 points apart through
   * twilight. Both are gone -- sky, ground and star visibility all read this, and the sky takes it
   * as a uniform (uDayF) instead of recomputing. */
  a.day = w3_daylight(sun_el);

  a.haze[0] = 0.05f + 0.67f * a.day;
  a.haze[1] = 0.06f + 0.76f * a.day;
  a.haze[2] = 0.13f + 0.79f * a.day;
  /* Cloud pulls the horizon toward a flat grey -- bright by day, dim by night -- but only 45 %
   * of the way even at full cover, or an overcast noon would go pure white. */
  for (int i = 0; i < 3; i++) {
    float tgt = 0.80f * a.day + 0.06f * (1.f - a.day);
    a.haze[i] += (tgt - a.haze[i]) * a.cloud * 0.45f;
  }

  a.light = 0.08f + 0.92f * a.day; /* low deep-night floor so EVS night is genuinely dark */
  a.sun_disc = 1.f;                /* the real sky shows the sun */
  return a;
}

/* SVS (synthetic vision) atmosphere: a CONSTANT, time-independent daylight, per RTCA DO-315B
 * (SVS/EVS MASPS) and the Garmin SVT / Honeywell PFD convention -- blue-over-brown, a map-like sky,
 * not a photo of the real one. So: no day/night, no stars, no clouds, and NO sun disc (sun_disc=0)
 * -- just a blue gradient. The sun vector is kept only as a FIXED directional light for terrain
 * relief shading (one light, no wandering, no moving shadows), high (~55 deg) for even relief. Same
 * at any SIM_UTC.
 *
 * Extension point, deliberately NOT built (that is more than asked): TAWS terrain-awareness
 * colouring
 * -- tinting the terrain green/amber/red by height relative to the aircraft's altitude. The
 * standard next step for a real SVS; it would key off telemetry alt in the terrain pass, not here.
 */
static w3_atmo w3_atmo_synthetic(void) {
  const float RAD = (float)M_PI / 180.f;
  const float sun_el = 55.f, sun_az = 160.f;
  w3_atmo a;
  float ce = cosf(sun_el * RAD);
  a.sun[0] = ce * sinf(sun_az * RAD);
  a.sun[1] = sinf(sun_el * RAD);
  a.sun[2] = -ce * cosf(sun_az * RAD);
  a.moon[0] = 0.f;
  a.moon[1] = -1.f;
  a.moon[2] = 0.f; /* below horizon: no moon disc */
  a.moon_ph = 0.f;
  a.cloud = 0.f;
  a.day = 1.f;
  a.light = 1.f;
  a.sun_disc = 0.f; /* map-like: plain blue, no sun disc */
  a.haze[0] = 0.72f;
  a.haze[1] = 0.82f;
  a.haze[2] = 0.92f; /* clear-day blue horizon */
  return a;
}

#endif /* W3_ATMO_H */
