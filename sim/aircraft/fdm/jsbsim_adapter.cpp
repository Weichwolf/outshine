/* FlightBox — JSBSim <-> bridge adapter (Migrationspaket D2). See jsbsim_adapter.h.
 * Replaces physics_step when FDM_ENGINE=jsbsim: JSBSim is the plant, state_t S the interface. */
#include "FGFDMExec.h"
#include "initialization/FGInitialCondition.h"
#include "initialization/FGTrim.h"
#include "jsbsim_adapter.h"
#include <cstdio>
#include <string>

using namespace JSBSim;

static const double FT   = 0.3048;              /* ft -> m */
static const double R2D  = 57.29577951308232;   /* rad -> deg */
static const double MS2KT = 1.9438444924406;    /* m/s -> knots */

static FGFDMExec *g_fdm = nullptr;

extern "C" int fb_jsbsim_init(const char *root, const char *ac,
                              double lat, double lon, double elev_m, double hoff_m,
                              double speed_ms, double yaw_deg, int fbw_override) {
  g_fdm = new FGFDMExec();
  g_fdm->SetDebugLevel(0);
  std::string r = root, d = r + "/" + ac;
  if (!g_fdm->LoadModel(SGPath(r), SGPath(d + "/engine"), SGPath(d + "/Systems"), ac)) {
    fprintf(stderr, "[jsbsim] LoadModel(%s) failed\n", ac);
    return 1;
  }
  auto ic = g_fdm->GetIC();
  ic->SetGeodLatitudeDegIC(lat);                /* GEODETIC — matches GPS/HOME_LAT (D2 probe caught this) */
  ic->SetLongitudeDegIC(lon);
  ic->SetAltitudeASLFtIC((elev_m + hoff_m) / FT);
  ic->SetVcalibratedKtsIC(speed_ms * MS2KT);
  ic->SetPsiDegIC(yaw_deg < 0 ? yaw_deg + 360.0 : yaw_deg);
  ic->SetFlightPathAngleDegIC(0.0);             /* level */
  g_fdm->RunIC();
  if (fbw_override) g_fdm->SetPropertyValue("fcs/fbw-override", 1.0);   /* iNav is the FLCS (F-16) */
  g_fdm->Setdt(0.01);                            /* 100 Hz, the bridge loop rate */
  /* Trim to steady level flight so the spawn is an equilibrium, not a big transient. tLongitudinal
   * (pitch/throttle/alpha, wings level) is more robust than tFull for light/low-speed airframes;
   * tFull often fails to converge on the motor-glider. */
  try {
    FGTrim trim(g_fdm, tLongitudinal);
    if (!trim.DoTrim()) fprintf(stderr, "[jsbsim] longitudinal trim did not converge (flying untrimmed)\n");
  } catch (...) { fprintf(stderr, "[jsbsim] trim threw (flying untrimmed)\n"); }
  fprintf(stderr, "[jsbsim] %s loaded + trimmed at %.4f/%.4f %.0f m, %.1f m/s\n",
          ac, lat, lon, elev_m + hoff_m, speed_ms);
  return 0;
}

extern "C" void fb_jsbsim_set_controls(double roll, double pitch, double yaw, double thr) {
  if (!g_fdm) return;
  g_fdm->SetPropertyValue("fcs/aileron-cmd-norm",  roll);
  g_fdm->SetPropertyValue("fcs/elevator-cmd-norm", -pitch);   /* JSBSim +elevator = nose DOWN; iNav +pitch = nose UP (D2 measured) */
  g_fdm->SetPropertyValue("fcs/rudder-cmd-norm",   yaw);
  g_fdm->SetPropertyValue("fcs/throttle-cmd-norm", thr);
}

extern "C" void fb_jsbsim_set_wind(double wind_n, double wind_e) {
  if (!g_fdm) return;
  g_fdm->SetPropertyValue("atmosphere/wind-north-fps", wind_n / FT);
  g_fdm->SetPropertyValue("atmosphere/wind-east-fps",  wind_e / FT);
}

extern "C" void fb_jsbsim_set_ground(double ground_m) {
  if (!g_fdm) return;
  g_fdm->SetPropertyValue("position/terrain-elevation-asl-ft", ground_m / FT);
}

extern "C" void fb_jsbsim_step(fb_fdm_state *o) {
  if (!g_fdm) return;
  g_fdm->Run();
  o->roll  = g_fdm->GetPropertyValue("attitude/phi-deg");
  o->pitch = g_fdm->GetPropertyValue("attitude/theta-deg");
  o->yaw   = g_fdm->GetPropertyValue("attitude/psi-deg");
  o->p     = g_fdm->GetPropertyValue("velocities/p-rad_sec") * R2D;
  o->q     = g_fdm->GetPropertyValue("velocities/q-rad_sec") * R2D;
  o->r     = g_fdm->GetPropertyValue("velocities/r-rad_sec") * R2D;
  o->lat   = g_fdm->GetPropertyValue("position/lat-geod-deg");
  o->lon   = g_fdm->GetPropertyValue("position/long-gc-deg");
  o->elev  = g_fdm->GetPropertyValue("position/h-sl-ft") * FT;
  o->speed = g_fdm->GetPropertyValue("velocities/vt-fps") * FT;
  o->gs    = g_fdm->GetPropertyValue("velocities/vg-fps") * FT;
  o->vx    =  g_fdm->GetPropertyValue("velocities/v-east-fps")  * FT;   /* +x east */
  o->vy    = -g_fdm->GetPropertyValue("velocities/v-down-fps")  * FT;   /* +y up   */
  o->vz    = -g_fdm->GetPropertyValue("velocities/v-north-fps") * FT;   /* +z south */
  o->nz    = g_fdm->GetPropertyValue("accelerations/Nz");
}
