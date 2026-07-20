/* FlightBox — shared flight-state globals owned by the bridge orchestrator.
 * The plant state S, atmosphere ATM, home origin and load factor are written by physics_step
 * and read across every bridge module; one owning TU (xp_bridge.c) DEFINES them, the rest
 * reference them through this header. */
#ifndef FB_SIM_STATE_H
#define FB_SIM_STATE_H
#include "fdm/atmosphere.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define DEG (180.0/M_PI)
#define RAD (M_PI/180.0)
#define M_PER_DEG 111320.0   /* metres per degree of latitude (equirectangular local-tangent scale) */

/* --- physics state (this IS the flight dynamics model) --- */
typedef struct {
    double roll, pitch, yaw;      /* deg, X-Plane phi/theta/psi */
    double p, q, r;               /* deg/s body rates */
    double lat, lon, elev, agl;   /* deg, deg, m, m */
    double speed;                 /* airspeed, m/s (aero uses this) */
    double gs;                    /* groundspeed, m/s (airspeed + wind; GPS reports this) */
    double vx, vy, vz;            /* X-Plane local ground velocity: +x east, +y up, +z south */
    /* control inputs from iNav (its mixer outputs) */
    double in_roll, in_pitch, in_yaw, in_thr;
} state_t;

extern state_t S;
extern double  HOME_LAT, HOME_LON, HOME_ELEV;
extern fb_atmo ATM;
extern float   g_nx, g_ny, g_nz;
extern int     g_inject;   /* XP_INJECT: force a fixed attitude (main sets, physics_step reads) */

/* live sun/moon ephemeris (orchestrator writes ~1 Hz, telemetry reads) */
extern float g_sun_el, g_sun_az, g_moon_el, g_moon_az, g_moon_ph;

/* Link-quality noise borrows the atmosphere's RNG stream (one deterministic sequence per run). */
#define nrand() fb_atmo_nrand(&ATM)

#endif /* FB_SIM_STATE_H */
