/* FlightBox — JSBSim adapter: the ONE-TU seam between our C++ and the vendored JSBSim engine
 * (vendor/jsbsim, read-only per CLAUDE.md Prinzip 1). jsbsim_adapter.cpp is the ONLY translation unit
 * that includes a JSBSim header — every caller here (App/systems, all our own C++) sees only this flat
 * POD state + the inventoried property-access functions below, never FGFDMExec directly. That seam,
 * not a C ABI, is the point: this used to be `extern "C"` for a since-deleted xp_bridge.c (pre-pivot
 * architecture); nothing calls it from C or from JS today (WASM exports only fb_toggle_ground/
 * fb_set_ground, see FBAppWasm.cpp), so it is plain C++ in `namespace FlightBox` like everything else.
 *
 * fb_fdm_state units: angles deg, rates deg/s, lat/lon deg GEODETIC, elev m ASL, speeds m/s, vx/vy/vz
 * X-Plane-local (E,+up,S) m/s (a legacy convention from the same pre-pivot bridge, kept because the
 * renderer/HUD math already consumes it), nz g. */
#ifndef FB_JSBSIM_ADAPTER_H
#define FB_JSBSIM_ADAPTER_H

namespace FlightBox {

struct fb_fdm_state {
  double roll, pitch, yaw;   /* deg (phi/theta/psi) */
  double p, q, r;            /* deg/s body rates */
  double lat, lon, elev;     /* deg geodetic, deg, m ASL */
  double speed, gs, cas;     /* true airspeed, groundspeed, calibrated airspeed (density-corrected), m/s */
  double mach;               /* Mach number (dimensionless) */
  double vx, vy, vz;         /* X-Plane local: +x east, +y up, +z south, m/s */
  double nx, ny, nz;         /* body load factors, g (long/lat/normal) — for --useimu accel feed */
  double alphaDeg;           /* angle of attack, deg (aero/alpha-deg) — mission telemetry's stall check */
};

/* Load models_root/<ac>/<ac>.xml (+ its engine/ and Systems/), set a geodetic IC at
 * (lat,lon, elev+hoff), speed_ms level on heading yaw_deg, TRIM, and (if fbw) engage
 * fcs/fbw-override so FlightBox's own FCS — not the aircraft's own FLCS — is the controller (F-16).
 * Returns 0 on success. */
int  fb_jsbsim_init(const char *models_root, const char *ac,
                    double lat, double lon, double elev_m, double hoff_m,
                    double speed_ms, double yaw_deg, int fbw_override);

/* FBFlightControl's stick/throttle output, normalized: roll/pitch/yaw in [-1,1], thr in [0,1]. */
void fb_jsbsim_set_controls(double in_roll, double in_pitch, double in_yaw, double in_thr);

/* Auxiliary actuators (gear/flap/speedbrake), each in [0,1]; pass a negative value to leave that
 * control at the FDM default (control not mapped by the model). */
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

/* ---- Airframe-controls surface (FBJsbsimAirframeControls) — generic FGFCS/FGGroundReactions/
 * FGPropulsion-tied properties, verified against vendor/jsbsim (src/models/FGFCS.cpp,
 * FGGroundReactions.cpp, FGPropulsion.cpp) and aircraft/f16/f16.xml's own use of them; none of this is
 * F-16-specific plumbing, only the f16.xml gear-kinematic/steer-gain SCHEDULES that consume it are. ---- */

/* Gear command [0,1] (1=down) and its lagged position readback [0,1] (gear/gear-cmd-norm,
 * gear/gear-pos-norm — the F-16's flight_control channel drives a 5 s kinematic transit through this
 * SAME property; fb_jsbsim_set_aux's `gear` argument writes the identical property). */
void   fb_jsbsim_set_gear(double cmd);
double fb_jsbsim_get_gear_pos(void);

/* Speedbrake command [0,1] (fcs/speedbrake-cmd-norm — same property fb_jsbsim_set_aux's `speedbrake`
 * argument writes; a dedicated single-control setter for FBJsbsimAirframeControls). */
void fb_jsbsim_set_speedbrake(double cmd);

/* Independent per-side wheel brakes [0,1] (fcs/left-brake-cmd-norm / fcs/right-brake-cmd-norm) — unlike
 * fb_jsbsim_set_brake (both mains + centre together, for a straight rollout stop), these differ for
 * differential braking/steering assist. */
void fb_jsbsim_set_wheel_brakes(double left, double right);

/* Nosewheel steering command [-1,1] (fcs/steer-cmd-norm, tied generically in FGGroundReactions; the
 * F-16's own scheduled-gain block maps it to a speed-dependent deflection — 80 deg at taxi speed down
 * to 2 deg at 150 fps, aircraft/f16/f16.xml). The adapter only asserts the pilot's command. */
void fb_jsbsim_set_nosewheel_steer(double cmd);

/* Weight-on-wheels. unit 0/1/2 = nose/left-main/right-main (aircraft/f16/f16.xml contact declaration
 * order) reads gear/unit[N]/WOW; unit<0 reads the model-wide gear/wow (true if ANY bogey is
 * compressed, FGGroundReactions::GetWOW). Returns 0/1. */
int fb_jsbsim_get_wow(int unit);

/* Engine starter/cutoff — propulsion-wide starter_cmd/cutoff_cmd properties (FGPropulsion::SetStarter/
 * SetCutoff default to ALL engines); running-state readback per engine index
 * (propulsion/engine[N]/set-running). Returns 0/1 for the readback. */
void fb_jsbsim_engine_start(void);
void fb_jsbsim_engine_cutoff(void);
int  fb_jsbsim_engine_running(int engine_index);

/* Throttle actually applied (fcs/throttle-cmd-norm, post slew — see fb_jsbsim_set_controls) and the
 * speedbrake's lagged position (fcs/speedbrake-pos-norm, both generic FGFCS ties) — mission telemetry's
 * throttleNorm/speedbrake columns, readbacks rather than the last-commanded value. */
double fb_jsbsim_get_throttle(void);
double fb_jsbsim_get_speedbrake_pos(void);

/* Current gross weight, lb (inertia/weight-lbs, generic FGMassBalance tie) — FBPilot's rotation-speed
 * table is indexed by weight, not a fixed number. */
double fb_jsbsim_get_weight_lbs(void);

} // namespace FlightBox
#endif
