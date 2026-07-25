/* FlightBox — JSBSim <-> bridge adapter (Migrationspaket D). C-callable so xp_bridge.c (C) drives
 * the C++ JSBSim engine. Fills a flat state in the SAME units as state_t S; xp_bridge assigns it.
 * Units: angles deg, rates deg/s, lat/lon deg GEODETIC, elev m ASL, speeds m/s, vx/vy/vz X-Plane
 * local (E,+up,S) m/s, nz g. All frame/unit conversions verified via jsb_probe (see D2 commit). */
#ifndef FB_JSBSIM_ADAPTER_H
#define FB_JSBSIM_ADAPTER_H
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  double roll, pitch, yaw;   /* deg (phi/theta/psi) */
  double p, q, r;            /* deg/s body rates */
  double lat, lon, elev;     /* deg geodetic, deg, m ASL */
  double speed, gs, cas;     /* true airspeed, groundspeed, calibrated airspeed (density-corrected), m/s */
  double mach;               /* Mach number (dimensionless) */
  double vx, vy, vz;         /* X-Plane local: +x east, +y up, +z south, m/s */
  double nx, ny, nz;         /* body load factors, g (long/lat/normal) — for --useimu accel feed */
} fb_fdm_state;

/* Load models_root/<ac>/<ac>.xml (+ its engine/ and Systems/), set a geodetic IC at
 * (lat,lon, elev+hoff), speed_ms level on heading yaw_deg, TRIM, and (if fbw) engage
 * fcs/fbw-override so iNav — not the aircraft's own FLCS — is the controller (F-16).
 * Returns 0 on success. */
int  fb_jsbsim_init(const char *models_root, const char *ac,
                    double lat, double lon, double elev_m, double hoff_m,
                    double speed_ms, double yaw_deg, int fbw_override);

/* iNav mixer outputs, normalized: roll/pitch/yaw in [-1,1], thr in [0,1]. */
void fb_jsbsim_set_controls(double in_roll, double in_pitch, double in_yaw, double in_thr);

/* Auxiliary actuators driven by iNav servos read over MSP (gear/flap/speedbrake), each in [0,1];
 * pass a negative value to leave that control at the FDM default (control not mapped by the model). */
void fb_jsbsim_set_aux(double gear, double flap, double speedbrake);

/* Wheel brakes [0,1] on both main-gear brake groups — for the landing rollout to a stop. */
void fb_jsbsim_set_brake(double b);

/* Steady wind (m/s, north/east). */
void fb_jsbsim_set_wind(double wind_n, double wind_e);

/* Real ground elevation under the aircraft (m ASL) so gear/contact use fb-tiles terrain. */
void fb_jsbsim_set_ground(double ground_m);

/* Read back the ground elevation (m ASL) the FDM is CURRENTLY colliding against -- lets a caller
 * prove the DEM value actually reached JSBSim, not just that the setter was called. */
double fb_jsbsim_get_ground(void);

/* Advance one fixed 100 Hz step, then read the resulting state. */
void fb_jsbsim_step(fb_fdm_state *out);

/* Model ground clearance (m): CG height above ground when the lowest active contact touches, level.
 * gear_down=1 -> wheels on struts; gear_down=0 -> only fixed structure (belly). Query after init. */
double fb_jsbsim_ground_clearance(int gear_down);

#ifdef __cplusplus
}
#endif
#endif
