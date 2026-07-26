/* FlightBox — FBFdm implementation. See FBFdm.h's banner: this is the ONE translation unit that
 * includes JSBSim headers; every property name FlightBox touches is listed in that header.
 *
 * NOT instance-safe in JSBSim itself (verified against vendor/jsbsim at the pinned commit — everything
 * else, including the whole property tree, is per instance because FGFDMExec(nullptr) allocates its own
 * SGPropertyNode root and its own FDM counter):
 *   - FGJSBBase::debug_lvl is a static shared by ALL instances; SetDebugLevel() is process-wide in
 *     effect. Every FBFdm sets it to 0, so no instance can surprise another.
 *   - JSBSim::SetLogger/GetLogger (input_output/FGLog.cpp) hold ONE process-global logger; per-instance
 *     log routing is impossible. FlightBox never sets it — JSBSim's own output stays off at debug 0.
 *   - The ctor reads the JSBSIM_DEBUG / JSBSIM_DISPERSE environment variables into that shared static,
 *     i.e. they apply to the whole process, not to one airframe.
 * None of these carry physics state, so N airframes integrate independently. */
#include "FGFDMExec.h"
#include "initialization/FGInitialCondition.h"
#include "initialization/FGTrim.h"
#include "models/FGGroundReactions.h"
#include "models/FGLGear.h"
#include "models/FGPropulsion.h"
#include "models/propulsion/FGEngine.h"
#include "models/propulsion/FGTank.h"
#include "FBFdm.h"
#include "FBFdmBoot.h"
#include "FBLog.h"
#include "FBUnits.h"
#include <cmath>
#include <cstdio>
#include <string>
#include <sys/stat.h>

using namespace JSBSim;

namespace FlightBox {

namespace {
constexpr double kFt    = kFtToM;              /* ft -> m */
constexpr double kR2D   = kRad2Deg;            /* rad -> deg */
constexpr double kMs2Kt = kMsToKt;             /* m/s -> knots */
constexpr double kEscSpinupS  = 0.5;           /* the spool ramps over this; a throttle STEP blows the
                                                * engine's own RPM ODE up */
constexpr double kThrottleSlew = FBFdm::kStepS / kEscSpinupS;

double Clamp01(double v) { return v < 0.0 ? 0.0 : v > 1.0 ? 1.0 : v; }
} // namespace

struct FBFdm::Impl {
  FGFDMExec Exec;
  double ThrottleApplied = 0.0;   /* slew-limited throttle actually fed to the engine (SetControls) */
  double ElevTrim = 0.0;          /* elevator trim (from DoTrim) biasing the FCS's pitch -> neutral = level */
};

FBFdm::FBFdm() : P(std::make_unique<Impl>()) { P->Exec.SetDebugLevel(0); }
FBFdm::~FBFdm() = default;

/* The exception firewall (see FBFdm.h): JSBSim throws JSBSim::BaseException (a std::runtime_error) out of
 * XML parsing and FGJSBBase::FloatingPointException out of table evaluation — uncaught, a broken
 * aircraft.xml kills the process with std::terminate and the mission loop never gets a RESULT line to
 * branch on. Caught by std::exception (JSBSim's hierarchy derives from it) plus a catch-all, so no JSBSim
 * type has to be named here. Works identically in WASM: the vendored libJSBSim, this TU and the final
 * link all carry -fexceptions (vendor/build_jsbsim_wasm.sh, the wasm make target) — the ONE place
 * exceptions are enabled, and the firewall sits inside it. */
bool FBFdm::Load(const FBFdmSpawn &spawn) {
  try {
    return LoadUnguarded(spawn);
  } catch (const std::exception &e) {
    FBLog::Error("fdm", "load_exception", {{"aircraft", spawn.Aircraft}, {"root", spawn.ModelsRoot},
                                           {"what", std::string(e.what())}});
  } catch (...) {
    FBLog::Error("fdm", "load_exception", {{"aircraft", spawn.Aircraft}, {"root", spawn.ModelsRoot},
                                           {"what", "non-standard exception"}});
  }
  return false;
}

bool FBFdm::LoadUnguarded(const FBFdmSpawn &spawn) {
  FGFDMExec &ex = P->Exec;
  const std::string r = spawn.ModelsRoot, d = r + "/" + spawn.Aircraft;
  /* Engine/Systems paths: our bundled models carry their own <ac>/engine + <ac>/Systems; the vanilla
   * JSBSim models (e.g. the full-scale f16) share JSBSim's standard <root>/../engine and keep Systems
   * under <ac>/Systems. Fall back to the shared layout so a vanilla model loads with no duplication.
   * Parent by truncation, NOT "..": emscripten's virtual FS does not normalize ".." in a path. */
  auto exists = [](const std::string &p) { struct stat st; return ::stat(p.c_str(), &st) == 0; };
  const std::string parent = r.substr(0, r.find_last_of('/'));
  const std::string eng = exists(d + "/engine") ? d + "/engine" : parent + "/engine";
  const std::string sys = exists(d + "/Systems") ? d + "/Systems" : parent + "/systems";
  if (!ex.LoadModel(SGPath(r), SGPath(eng), SGPath(sys), spawn.Aircraft)) {
    FBLog::Error("fdm", "LoadModel_failed", {{"aircraft", spawn.Aircraft}, {"root", r}, {"engine", eng}});
    return false;
  }

  auto ic = ex.GetIC();
  ic->SetGeodLatitudeDegIC(spawn.LatDeg);
  ic->SetLongitudeDegIC(spawn.LonDeg);
  double prov = (spawn.HeightOffsetM > 0.0) ? spawn.HeightOffsetM : 3.0;   /* explicit AGL, or a safe airborne value */
  ic->SetAltitudeASLFtIC((spawn.GroundElevM + prov) / kFt);
  ic->SetVcalibratedKtsIC(spawn.SpeedMs * kMs2Kt);
  ic->SetPsiDegIC(spawn.HeadingDeg < 0 ? spawn.HeadingDeg + 360.0 : spawn.HeadingDeg);
  ic->SetFlightPathAngleDegIC(0.0);   /* level */
  ex.RunIC();
  /* HeightOffsetM < 0 = "sit on the gear": now the CG is valid, re-place at the model gear-down
   * clearance so the spawn altitude equals the geometry-true wheel height (no held->armed jump). */
  if (spawn.HeightOffsetM < 0.0) {
    double gc = GetGroundClearanceM(true);
    if (gc > 0.1) { ic->SetAltitudeASLFtIC((spawn.GroundElevM + gc) / kFt); ex.RunIC(); }
  }
  P->ThrottleApplied = 0.0;
  /* Start ALL engines running before trim: without a running engine there is no thrust during FGTrim,
   * so a powered airframe reports "udot not trimmable", the IC is no equilibrium, and the untrimmed
   * airframe departs violently on the first advance. */
  { auto pr = ex.GetPropulsion();
    for (unsigned i = 0; i < pr->GetNumEngines(); i++) pr->InitRunning(i); }
  if (spawn.FbwOverride) ex.SetPropertyValue("fcs/fbw-override", 1.0);
  ex.Setdt(kStepS);
  /* A V=0 ground start (SpeedMs<=0, e.g. a runway spawn ahead of the takeoff roll) has no aerodynamic
   * trim solution: zero airspeed means zero aero force/moment, so udot/qdot cannot be zeroed by any
   * elevator position. FGTrim used to run anyway and report "not trimmable", leaving ElevTrim at the
   * failed search's last iterate (noise) — set the neutral trim directly instead, matching a real jet's
   * untrimmed stick before the roll. Airborne/rolling ICs still get the real search. */
  bool trimmed = false;
  if (spawn.SpeedMs > 0.0) {
    /* tLongitudinal (pitch/throttle/alpha, wings level): more robust than tFull on light/slow airframes. */
    try {
      FGTrim trim(&ex, tLongitudinal);
      trimmed = trim.DoTrim();
    } catch (...) { trimmed = false; }
  }
  /* Capture the elevator the trimmer settled on = the ELEVATOR TRIM for level flight, applied as a bias
   * to the FCS's pitch command (SetControls). A real trim tab: neutral stick then holds LEVEL, not the
   * airframe's nose-up-at-neutral. */
  P->ElevTrim = spawn.SpeedMs > 0.0 ? ex.GetPropertyValue("fcs/elevator-cmd-norm") : 0.0;
  ex.RunIC();   /* clean, level IC (attitude+speed); the trim tab, not the perturbed search state, holds it */
  if (spawn.SpeedMs > 0.0)
    FBLog::Info("fdm", "loaded", {{"aircraft", spawn.Aircraft}, {"speedMs", spawn.SpeedMs},
                                  {"elevTrim", P->ElevTrim}, {"trimConverged", trimmed}});
  else
    FBLog::Info("fdm", "loaded", {{"aircraft", spawn.Aircraft}, {"groundStart", true}, {"elevTrim", 0.0}});
  return true;
}

/* Same firewall as Load (see its banner), latching: once the integrator has raised, this airframe's
 * physics is over — every later Step is a no-op and `o` keeps its last good values, so the caller reads
 * a frozen but FINITE state while FBFlightMonitor turns Faulted() into the NumericalDivergence K.O. */
void FBFdm::Step(fb_fdm_state &o) {
  if (Faulted_) return;
  try {
    StepUnguarded(o);
    return;
  } catch (const std::exception &e) {
    FBLog::Error("fdm", "step_exception", {{"what", std::string(e.what())}});
  } catch (...) {
    FBLog::Error("fdm", "step_exception", {{"what", "non-standard exception"}});
  }
  Faulted_ = true;
}

void FBFdm::StepUnguarded(fb_fdm_state &o) {
  FGFDMExec &ex = P->Exec;
  ex.Run();
  o.roll  = ex.GetPropertyValue("attitude/phi-deg");
  o.pitch = ex.GetPropertyValue("attitude/theta-deg");
  o.yaw   = ex.GetPropertyValue("attitude/psi-deg");
  o.p     = ex.GetPropertyValue("velocities/p-rad_sec") * kR2D;
  o.q     = ex.GetPropertyValue("velocities/q-rad_sec") * kR2D;
  o.r     = ex.GetPropertyValue("velocities/r-rad_sec") * kR2D;
  o.lat   = ex.GetPropertyValue("position/lat-geod-deg");
  o.lon   = ex.GetPropertyValue("position/long-gc-deg");
  o.elev  = ex.GetPropertyValue("position/h-sl-ft") * kFt;
  o.speed = ex.GetPropertyValue("velocities/vt-fps") * kFt;
  o.gs    = ex.GetPropertyValue("velocities/vg-fps") * kFt;
  o.cas   = ex.GetPropertyValue("velocities/vc-fps") * kFt;   /* density-corrected — the honest
                                                               * "how close to stall" metric at any field elevation */
  o.mach  = ex.GetPropertyValue("velocities/mach");
  o.vx    =  ex.GetPropertyValue("velocities/v-east-fps")  * kFt;   /* +x east */
  o.vy    = -ex.GetPropertyValue("velocities/v-down-fps")  * kFt;   /* +y up   */
  o.vz    = -ex.GetPropertyValue("velocities/v-north-fps") * kFt;   /* +z south */
  o.nx    = ex.GetPropertyValue("accelerations/Nx");   /* body long g (forward+) */
  o.ny    = ex.GetPropertyValue("accelerations/Ny");   /* body lat g (right+) */
  o.nz    = ex.GetPropertyValue("accelerations/Nz");   /* body normal g (~+1 level) */
  o.alphaDeg = ex.GetPropertyValue("aero/alpha-deg");
}

void FBFdm::SetControls(double roll, double pitch, double yaw, double thr) {
  /* slew, don't step: a 0->0.95 throttle jump blows the engine's RPM ODE up and departs the airframe. */
  double &applied = P->ThrottleApplied;
  if      (thr > applied + kThrottleSlew) applied += kThrottleSlew;
  else if (thr < applied - kThrottleSlew) applied -= kThrottleSlew;
  else                                    applied  = thr;
  FGFDMExec &ex = P->Exec;
  ex.SetPropertyValue("fcs/aileron-cmd-norm",  roll);
  ex.SetPropertyValue("fcs/elevator-cmd-norm", -pitch + P->ElevTrim);   /* JSBSim +elevator = nose DOWN;
                                                                         * our FCS +pitch = nose UP; +trim tab = level at neutral */
  ex.SetPropertyValue("fcs/rudder-cmd-norm",   yaw);   /* +yaw coordinates the turn; -yaw slips it (measured, strong adverse yaw) */
  ex.SetPropertyValue("fcs/throttle-cmd-norm", applied);
}

void FBFdm::SetGear(double cmd) { P->Exec.SetPropertyValue("gear/gear-cmd-norm", cmd); }
void FBFdm::SetFlap(double cmd) { P->Exec.SetPropertyValue("fcs/flap-cmd-norm", cmd); }
void FBFdm::SetSpeedbrake(double cmd) { P->Exec.SetPropertyValue("fcs/speedbrake-cmd-norm", cmd); }

void FBFdm::SetWheelBrakes(double left, double right) {
  P->Exec.SetPropertyValue("fcs/left-brake-cmd-norm", Clamp01(left));
  P->Exec.SetPropertyValue("fcs/right-brake-cmd-norm", Clamp01(right));
}

void FBFdm::SetNosewheelSteer(double cmd) { P->Exec.SetPropertyValue("fcs/steer-cmd-norm", cmd); }

void FBFdm::EngineStart() {
  P->Exec.SetPropertyValue("propulsion/cutoff_cmd", 0.0);
  P->Exec.SetPropertyValue("propulsion/starter_cmd", 1.0);
}

void FBFdm::EngineCutoff() {
  P->Exec.SetPropertyValue("propulsion/starter_cmd", 0.0);
  P->Exec.SetPropertyValue("propulsion/cutoff_cmd", 1.0);
}

void FBFdm::SetFuelTankLbs(int idx, double lbs) {
  auto pr = P->Exec.GetPropulsion();
  if (idx < 0 || (size_t)idx >= pr->GetNumTanks()) return;
  pr->GetTank((unsigned)idx)->SetContents(lbs < 0.0 ? 0.0 : lbs);
}

void FBFdm::SetFuelTotalLbs(double lbs) {
  double capacity = GetFuelCapacityLbs();
  if (capacity <= 0.0) return;
  double frac = Clamp01(lbs / capacity);
  auto pr = P->Exec.GetPropulsion();
  for (unsigned i = 0; i < pr->GetNumTanks(); i++) {
    auto tank = pr->GetTank(i);
    tank->SetContents(tank->GetCapacity() * frac);
  }
}

void FBFdm::SetFuelPct(double pct) { SetFuelTotalLbs(GetFuelCapacityLbs() * (pct / 100.0)); }

void FBFdm::SetGroundElevM(double m) {
  P->Exec.SetPropertyValue("position/terrain-elevation-asl-ft", m / kFt);
}

double FBFdm::GetGroundElevM() const {
  return P->Exec.GetPropertyValue("position/terrain-elevation-asl-ft") * kFt;
}

double FBFdm::GetGroundClearanceM(bool gearDown) const {
  auto gr = P->Exec.GetGroundReactions();
  double maxz = 0.0;
  for (int i = 0; i < gr->GetNumGearUnits(); i++) {
    auto lg = gr->GetGearUnit(i);
    if (!lg) continue;
    if (!gearDown && lg->GetRetractable()) continue;   /* gear up: only fixed structure touches */
    double z = lg->GetBodyLocation(3);                  /* body z (down+), ft below the CG */
    if (z > maxz) maxz = z;
  }
  return maxz * kFt;
}

double FBFdm::GetGearPos() const { return P->Exec.GetPropertyValue("gear/gear-pos-norm"); }
double FBFdm::GetSpeedbrakePos() const { return P->Exec.GetPropertyValue("fcs/speedbrake-pos-norm"); }
bool   FBFdm::GetWow() const { return P->Exec.GetPropertyValue("gear/wow") != 0.0; }

bool FBFdm::GetStructureContact() const {
  auto gr = P->Exec.GetGroundReactions();
  for (int i = 0; i < gr->GetNumGearUnits(); i++) {
    auto lg = gr->GetGearUnit(i);
    if (lg && !lg->IsBogey() && lg->GetWOW()) return true;
  }
  return false;
}

double FBFdm::GetMaxGearForceLbs() const {
  auto gr = P->Exec.GetGroundReactions();
  double maxF = 0.0;
  for (int i = 0; i < gr->GetNumGearUnits(); i++) {
    auto lg = gr->GetGearUnit(i);
    if (!lg || !lg->IsBogey()) continue;
    double f = std::fabs(lg->GetCompForce());
    if (f > maxF) maxF = f;
  }
  return maxF;
}

double FBFdm::GetWeightLbs() const { return P->Exec.GetPropertyValue("inertia/weight-lbs"); }

bool FBFdm::GetEngineRunning(int engineIndex) const {
  char buf[48];
  snprintf(buf, sizeof buf, "propulsion/engine[%d]/set-running", engineIndex);
  return P->Exec.GetPropertyValue(buf) != 0.0;
}

int FBFdm::GetFuelTankCount() const { return (int)P->Exec.GetPropulsion()->GetNumTanks(); }

double FBFdm::GetFuelTankLbs(int idx) const {
  auto pr = P->Exec.GetPropulsion();
  if (idx < 0 || (size_t)idx >= pr->GetNumTanks()) return 0.0;
  return pr->GetTank((unsigned)idx)->GetContents();
}

double FBFdm::GetFuelTotalLbs() const {
  auto pr = P->Exec.GetPropulsion();
  double sum = 0.0;
  for (unsigned i = 0; i < pr->GetNumTanks(); i++) sum += pr->GetTank(i)->GetContents();
  return sum;
}

double FBFdm::GetFuelCapacityLbs() const {
  auto pr = P->Exec.GetPropulsion();
  double sum = 0.0;
  for (unsigned i = 0; i < pr->GetNumTanks(); i++) sum += pr->GetTank(i)->GetCapacity();
  return sum;
}

} // namespace FlightBox
