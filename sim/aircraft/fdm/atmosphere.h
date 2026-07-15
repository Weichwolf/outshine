/* FlightBox FDM — the air the aircraft flies through.
 *
 * Owns everything about the moving atmosphere and nothing about the aircraft: steady wind,
 * horizontal gust wander, Dryden turbulence, and the thermal field. It answers one question for
 * the FDM — "at this point in space, which way and how fast is the air moving?" — and one for the
 * link layer: "what buffet rates should iNav's gyro see?".
 *
 * All state lives in fb_atmo, passed explicitly. No globals: that is what makes the maths here
 * unit-testable, which the file-scope statics in the old god-file were not. The RNG lives here
 * too, so a run stays bit-for-bit reproducible as long as the call order is unchanged.
 *
 * Two-layer wind, on purpose:
 *   - the *target* fields (_t) are what the 15-minute live-weather refresh writes
 *   - the current fields are slewed toward them with a long time constant
 * Real weather changes gradually; assigning it directly would step the aircraft's relative
 * airflow instantly — a genuine jolt every refresh.
 */
#ifndef FB_ATMOSPHERE_H
#define FB_ATMOSPHERE_H
#include <stdint.h>

/* Slew time constant from a weather refresh to the air the aircraft actually feels, seconds. */
#define FB_ATMO_TAU 60.0

typedef struct {
    /* --- what the FDM feels right now --- */
    double windN, windE;      /* steady wind vector, m/s (ground/NED frame) */
    double gustN, gustE;      /* slowly-varying horizontal gust offset, m/s */
    double turb;              /* turbulence intensity (0=calm, 1=moderate, 2=rough) */
    double sigma;             /* Dryden vertical-gust std, m/s */
    double bl_height;         /* boundary-layer / thermal-top height, m AGL */
    double thermal_W;         /* peak thermal updraft, m/s (0 = off) */

    /* --- targets the live-weather refresh writes; the above slew toward these --- */
    double windN_t, windE_t, turb_t, sigma_t, bl_t, thermal_t;
    int    first;             /* 1 = next slew snaps (no minute-long ramp from zero at start-up) */

    /* --- gust rates handed to iNav's gyro (deg/s) --- */
    double gustP, gustQ;

    /* --- internal Dryden filter state --- */
    double wg, pg, qg;
    uint32_t rng;
} fb_atmo;

/* Calm-day defaults + deterministic RNG seed. */
void   fb_atmo_init(fb_atmo *a);

/* xorshift uniform [0,1) and ~gaussian (mean 0, UNIT variance). Deterministic per run.
 * Exposed because the telemetry layer borrows the same stream for link-quality noise. */
double fb_atmo_urand(fb_atmo *a);
double fb_atmo_nrand(fb_atmo *a);

/* Vertical air velocity from the thermal field at a point, m/s (+ = up).
 * Pure: depends only on (thermal_W, bl_height) and the position. */
double fb_atmo_thermal_w(const fb_atmo *a, double x, double y, double z);

/* Point the atmosphere at new weather. Does NOT take effect instantly — see FB_ATMO_TAU.
 * Call fb_atmo_snap() once at start-up so the first real observation applies immediately. */
void   fb_atmo_set_target(fb_atmo *a, double windN, double windE,
                          double turb, double sigma, double bl_height, double thermal_W);
void   fb_atmo_snap(fb_atmo *a);      /* current := target, right now */

/* Ease the current values toward the targets by dt. Exponential, time constant FB_ATMO_TAU.
 * Honours `first` by snapping instead of ramping. */
void   fb_atmo_slew(fb_atmo *a, double dt);

/* Advance the Dryden turbulence filters and the horizontal gust wander by dt.
 * agl in m, speed = representative airspeed in m/s. Updates gustN/E and gustP/Q. */
void   fb_atmo_step_gusts(fb_atmo *a, double dt, double agl, double speed);

#endif /* FB_ATMOSPHERE_H */
