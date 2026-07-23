/* FlightBox — the local flight simulator INSIDE the command center. Replaces net.h: instead of
 * telemetry arriving over a WebSocket from a separate world process, JSBSim + the FB autopilot/FBW
 * run in THIS process and fill `telem` each frame. One process, no wire (CLAUDE.md pivot).
 *
 * The F-16 is flown through its own real FLCS, commanded by FBFlightControl::F16() (fb/). The
 * renderer/HUD (present.h) still consume `telem`, unchanged — only its source moved from the
 * network to a direct JSBSim read. flightctl.h's fc_ctl/fc_update are RETIRED here — this is the
 * fb/ port, numerics equivalence-tested against them (see FBAutopilot.h/FBFlightControl.h). */
#ifndef FB_FLIGHTSIM_H
#define FB_FLIGHTSIM_H
#include <cmath>
#include "jsbsim_adapter.h"
#include "fb/FBState.h"
#include "fb/FBAutopilot.h"
#include "fb/FBFlightControl.h"
#include "fb/FBEphemeris.h"

/* telemetry the renderer reads (was net.h's globals) */
static FlightBox::FBState telem;
static int have_telem = 0;

static FlightBox::FBAutopilot fs_ap;
static FlightBox::FBFlightControl fs_fc = FlightBox::FBFlightControl::F16();
static fb_fdm_state fs_st;
static double fs_olat, fs_olon;   /* ENU origin = loiter centre = spawn reference */
static double fs_acc = 0.0;
static const double FS_MPD = 111320.0;   /* metres per degree latitude (spherical approx) */

/* point `dist` m from (lat0,lon0) on bearing `brg` (deg, 0=N,90=E) — spawn placement only */
static void fs_ne_dest(double lat0, double lon0, double dist, double brg, double *lat, double *lon) {
  double b = brg * M_PI / 180.0;
  *lat = lat0 + (dist * cos(b)) / FS_MPD;
  *lon = lon0 + (dist * sin(b)) / (FS_MPD * cos(lat0 * M_PI / 180.0));
}

/* Spawn the F-16 loitering around (lat,lon) at alt_asl on a ring of `radius` m and start the FLCS.
 * ground_asl: origin ground (m ASL, -1e8 = unknown/cold /elev) -- seeds JSBSim's collision floor
 * before the first step; frame()'s periodic fb_jsbsim_set_ground (cc.cpp) keeps it current as the
 * aircraft moves. Without this JSBSim's ground reactions collide against 0 m ASL (Jura ~1000-1300 m)
 * -- gear/crash physics would be blind until the first live DEM sample landed. */
static void flightsim_init(double lat, double lon, double alt_asl, double radius, double ground_asl) {
  fs_olat = lat; fs_olon = lon;
  double slat, slon; fs_ne_dest(lat, lon, radius, 0.0, &slat, &slon);   /* start due N on the ring */
  fb_jsbsim_init("/jsbsim/aircraft", "f16", slat, slon, alt_asl, 0.0, 220.0, 90.0, 0);
  if (ground_asl > -1e8) fb_jsbsim_set_ground(ground_asl);
  /* w3_cam_clear (world3d.h): the camera-floor clamp. Old architecture fed it over IPC from the
   * xpjsb bridge (/tmp/fb_clearance -> FB_GROUND_CLEAR); that channel is retired and always reads 0
   * now. The model's own geometry lives in THIS process (fb_jsbsim_ground_clearance, query-after-init)
   * -- belly/gear-up, matching the clamp's "where the fuselage rests" semantics. Only fill it if
   * cfg_from_js didn't already set an explicit test override (still at its 0 default). */
  if (w3_cam_clear <= 0) w3_cam_clear = (float)fb_jsbsim_ground_clearance(0);
  printf("[cc] camera floor clearance %.3f m (model geometry)\n", w3_cam_clear);
  fs_ap.SetLoiter(lat, lon, alt_asl, radius, 1, 220.0);
  fb_jsbsim_step(&fs_st);          /* one step so fs_st is valid before the first Run() */
  have_telem = 1;
}

/* Advance ~`dt` seconds of flight (fixed 100 Hz substeps), then publish the pose into `telem`. */
static void flightsim_step(double dt) {
  fs_acc += dt;
  for (int k = 0; fs_acc >= 0.01 && k < 8; k++) {     /* cap 8 substeps/frame (spiral-of-death guard) */
    FlightBox::FBGuidance g = fs_ap.Run(fs_st);
    FlightBox::FBControls o = fs_fc.Run(g, fs_st);
    fb_jsbsim_set_controls(o.Roll, o.Pitch, o.Yaw, o.Thr);
    fb_jsbsim_step(&fs_st);
    fs_acc -= 0.01;
  }
  telem.roll = (float)fs_st.roll; telem.pitch = (float)fs_st.pitch; telem.yaw = (float)fs_st.yaw;
  telem.alt = (float)fs_st.elev;
  telem.y = (float)((fs_st.lat - fs_olat) * FS_MPD);                               /* north offset (m) */
  double fs_dlon = fs_st.lon - fs_olon;   /* Wrap180 (antimeridian): else a ~360° delta blows up the HUD home */
  while (fs_dlon > 180.0) fs_dlon -= 360.0;
  while (fs_dlon < -180.0) fs_dlon += 360.0;
  telem.x = (float)(fs_dlon * FS_MPD * cos(fs_olat * M_PI / 180.0)); /* east offset (m) */
  telem.gs = (float)fs_st.gs; telem.airspeed = (float)fs_st.speed; telem.vs = (float)fs_st.vy;
  /* home = the ENU origin (loiter centre): distance + bearing RELATIVE TO NOSE — the HUD home
   * pointer and WP box read these live (sim-critic: they were never filled -> dead at 0) */
  telem.home_dist = (float)std::hypot((double)telem.x, (double)telem.y);
  {
    double absBrg = std::atan2(-(double)telem.x, -(double)telem.y) * 180.0 / M_PI;  /* to origin, 0=N 90=E */
    double rel = absBrg - fs_st.yaw;
    while (rel > 180) rel -= 360;
    while (rel < -180) rel += 360;
    telem.home_bearing = (float)rel;
  }
  telem.batt = 0; telem.rssi = 100;
  telem.state = fs_ap.GetMode();   /* annunciator = REAL mode (MIL-STD-1787: no optimistic labels) */
  telem.glideslope_err = 99.0f;   /* no approach mode yet -> sentinel (FBState.h), HUD hides the GP cue;
                                   * a future ILS/approach mode feeds real deg vs TARGET_GLIDE here */
  /* Real ephemeris (fb/FBEphemeris.h), aircraft position + SIM_UTC (w3_sim_utc, world3d.h; 0/unset =
   * wall clock) -- the in-process pivot never got this (flightsim.h's own history: hardcoded "always
   * noon, no moon"), so EVS was time-independent regardless of FB_SIM_UTC. SVS ignores telem's
   * sun/moon entirely (w3_atmo_synthetic, atmo.h) -- this only ever reaches the real (EVS) sky. */
  {
    time_t utc = w3_sim_utc > 0 ? (time_t)w3_sim_utc : time(nullptr);
    FlightBox::SunPos(fs_st.lat, fs_st.lon, utc, &telem.sun_el, &telem.sun_az);
    FlightBox::MoonPos(fs_st.lat, fs_st.lon, utc, &telem.moon_el, &telem.moon_az, &telem.moon_phase);
  }
  telem.cloud = 0; telem.vis = 40000;   /* no live weather in scope (README A7) -- constants, not ephemeris */
  have_telem = 1;
}
#endif /* FB_FLIGHTSIM_H */
