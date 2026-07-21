/* FlightBox — JSBSim <-> bridge adapter (Migrationspaket D2). See jsbsim_adapter.h.
 * Replaces physics_step when FDM_ENGINE=jsbsim: JSBSim is the plant, state_t S the interface. */
#include "FGFDMExec.h"
#include "initialization/FGInitialCondition.h"
#include "initialization/FGTrim.h"
#include "models/FGPropulsion.h"
#include "models/propulsion/FGEngine.h"
#include "models/FGGroundReactions.h"
#include "models/FGLGear.h"
#include "jsbsim_adapter.h"
#include <cstdio>
#include <string>

using namespace JSBSim;

static const double FT   = 0.3048;              /* ft -> m */
static const double R2D  = 57.29577951308232;   /* rad -> deg */
static const double MS2KT = 1.9438444924406;    /* m/s -> knots */
static const double SIM_STEP_S    = 0.01;       /* fb_jsbsim_step advances the FDM this much (100 Hz bridge loop) */
static const double ESC_SPINUP_S  = 0.5;        /* the ESC ramps the motor over this; a throttle STEP blows the prop RPM up */
static const double THROTTLE_SLEW = SIM_STEP_S / ESC_SPINUP_S;

static FGFDMExec *g_fdm = nullptr;
static double g_thr_applied = 0.0;   /* slew-limited throttle actually fed to the engine (see set_controls) */
static double g_elev_trim = 0.0;     /* elevator trim (from DoTrim) biasing iNav's pitch -> neutral = level */

JSBSim::FGFDMExec *fb_dbg_fdm() { return g_fdm; }   /* probe-only accessor (standalone_probe.cpp) */

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
  double prov = (hoff_m > 0.0) ? hoff_m : 3.0;  /* provisional AGL (explicit spawn, or a safe airborne value) */
  ic->SetAltitudeASLFtIC((elev_m + prov) / FT);
  ic->SetVcalibratedKtsIC(speed_ms * MS2KT);
  ic->SetPsiDegIC(yaw_deg < 0 ? yaw_deg + 360.0 : yaw_deg);
  ic->SetFlightPathAngleDegIC(0.0);             /* level */
  g_fdm->RunIC();
  /* hoff_m < 0 = "sit on the gear": now the CG is valid, re-place at the model gear-down clearance so the
   * spawn/held altitude equals the geometry-true wheel height (no held->armed jump). */
  if (hoff_m < 0.0) {
    double gc = fb_jsbsim_ground_clearance(1);
    if (gc > 0.1) { ic->SetAltitudeASLFtIC((elev_m + gc) / FT); g_fdm->RunIC(); }
  }
  g_thr_applied = 0.0;
  /* Start ALL engines running before trim — incl. electric: without a running motor there is no thrust
   * during FGTrim, so a powered airframe reports "udot not trimmable", the IC is no equilibrium, and the
   * untrimmed airframe departs violently on the first advance (sgs233: pitch -57, inverted, at spawn). */
  { auto pr = g_fdm->GetPropulsion();
    for (unsigned i = 0; i < pr->GetNumEngines(); i++) pr->InitRunning(i); }
  if (fbw_override) g_fdm->SetPropertyValue("fcs/fbw-override", 1.0);   /* iNav is the FLCS (F-16) */
  g_fdm->Setdt(SIM_STEP_S);
  /* tLongitudinal (pitch/throttle/alpha, wings level): more robust than tFull on light/slow airframes. */
  bool trimmed = false;
  try {
    FGTrim trim(g_fdm, tLongitudinal);
    trimmed = trim.DoTrim();
  } catch (...) { trimmed = false; }
  /* Capture the elevator the trimmer settled on = the ELEVATOR TRIM for level flight, and apply it as
   * a bias to iNav's pitch command (set_controls). This is a real trim tab: iNav's neutral stick then
   * holds LEVEL, not the airframe's nose-up-at-neutral. Without it a slow/floaty model departs on the
   * first advance (neutral elevator -> +40 deg pitch -> stall) before iNav's AHRS can catch it. */
  g_elev_trim = g_fdm->GetPropertyValue("fcs/elevator-cmd-norm");
  g_fdm->RunIC();   /* clean, level IC (attitude+speed); the trim tab, not the perturbed search state, holds it */
  fprintf(stderr, "[jsbsim] %s loaded, %.1f m/s, elevator trim %.3f%s\n",
          ac, speed_ms, g_elev_trim, trimmed ? "" : " (trim search did not fully converge; using best-effort)");
  return 0;
}

/* Ground clearance from the aircraft MODEL, not a fixed 2 m: the height of the CG above the ground when
 * the lowest active contact point touches, at level attitude. gear_down=1 uses all contacts (wheels on
 * their struts); gear_down=0 uses only the NON-retractable ones (belly/skid) — how far the airframe sits
 * off the ground with the gear up. Per-model + gear-state, so takeoff/touchdown/crash and the camera eye
 * height are geometry-true. Returns metres; 0 if the model has no matching contact (query after LoadModel). */
extern "C" double fb_jsbsim_ground_clearance(int gear_down) {
  if (!g_fdm) return 0.0;
  auto gr = g_fdm->GetGroundReactions();
  double maxz = 0.0;
  for (int i = 0; i < gr->GetNumGearUnits(); i++) {
    auto lg = gr->GetGearUnit(i);
    if (!lg) continue;
    if (!gear_down && lg->GetRetractable()) continue;   /* gear up: only fixed structure touches */
    double z = lg->GetBodyLocation(3);                   /* body z (down+), ft below the CG */
    if (z > maxz) maxz = z;
  }
  return maxz * FT;
}

extern "C" void fb_jsbsim_set_controls(double roll, double pitch, double yaw, double thr) {
  if (!g_fdm) return;
  /* slew, don't step: a 0->0.95 throttle jump blows the light prop's RPM ODE up and departs the airframe. */
  if      (thr > g_thr_applied + THROTTLE_SLEW) g_thr_applied += THROTTLE_SLEW;
  else if (thr < g_thr_applied - THROTTLE_SLEW) g_thr_applied -= THROTTLE_SLEW;
  else                                          g_thr_applied  = thr;
  g_fdm->SetPropertyValue("fcs/aileron-cmd-norm",  roll);
  g_fdm->SetPropertyValue("fcs/elevator-cmd-norm", -pitch + g_elev_trim);   /* JSBSim +elevator = nose DOWN; iNav +pitch = nose UP; +trim tab = level at neutral */
  g_fdm->SetPropertyValue("fcs/rudder-cmd-norm",   yaw);      /* +yaw coordinates the turn; -yaw slips it (measured, strong adverse yaw) */
  g_fdm->SetPropertyValue("fcs/throttle-cmd-norm", g_thr_applied);
}

extern "C" void fb_jsbsim_set_aux(double gear, double flap, double speedbrake) {
  if (!g_fdm) return;   /* negative = not mapped by this model -> leave the FDM default untouched */
  if (gear       >= 0.0) g_fdm->SetPropertyValue("gear/gear-cmd-norm",     gear);        /* 1=down, 0=up */
  if (flap       >= 0.0) g_fdm->SetPropertyValue("fcs/flap-cmd-norm",      flap);
  if (speedbrake >= 0.0) g_fdm->SetPropertyValue("fcs/speedbrake-cmd-norm", speedbrake);
}

/* Wheel brakes, normalized [0,1] on both main-gear brake groups — for the landing rollout. */
extern "C" void fb_jsbsim_set_brake(double b) {
  if (!g_fdm) return;
  if (b < 0.0) b = 0.0; else if (b > 1.0) b = 1.0;
  g_fdm->SetPropertyValue("fcs/left-brake-cmd-norm",   b);
  g_fdm->SetPropertyValue("fcs/right-brake-cmd-norm",  b);
  g_fdm->SetPropertyValue("fcs/center-brake-cmd-norm", b);
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
  o->cas   = g_fdm->GetPropertyValue("velocities/vc-fps") * FT;   /* calibrated airspeed — density-corrected, the honest "how close to stall" metric at any field elevation */
  o->vx    =  g_fdm->GetPropertyValue("velocities/v-east-fps")  * FT;   /* +x east */
  o->vy    = -g_fdm->GetPropertyValue("velocities/v-down-fps")  * FT;   /* +y up   */
  o->vz    = -g_fdm->GetPropertyValue("velocities/v-north-fps") * FT;   /* +z south */
  o->nx    = g_fdm->GetPropertyValue("accelerations/Nx");   /* body long g (forward+) */
  o->ny    = g_fdm->GetPropertyValue("accelerations/Ny");   /* body lat g (right+) */
  o->nz    = g_fdm->GetPropertyValue("accelerations/Nz");   /* body normal g (~+1 level) */
}
