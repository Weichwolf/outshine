/* FlightBox — FBFdm implementation. See FBFdm.h's banner: this is the ONE translation unit that
 * includes JSBSim headers; every property name FlightBox touches is listed in that header.
 *
 * NOT instance-safe in JSBSim itself (verified against vendor/jsbsim at the pinned commit — everything
 * else, including the whole property tree, is per instance because FGFDMExec(nullptr) allocates its own
 * SGPropertyNode root and its own FDM counter):
 *   - FGJSBBase::debug_lvl is a static shared by ALL instances; SetDebugLevel() is process-wide in
 *     effect. Every FBFdm sets it to 0, so no instance can surprise another. It is written only from a
 *     ctor and from FGFDMExec's child-FDM trim path (which FlightBox does not use), and read-only for
 *     the whole of Run().
 *   - JSBSim::SetLogger/GetLogger (input_output/FGLog.cpp) hold ONE logger, and it is `thread_local` at
 *     the pinned commit — so per-INSTANCE log routing is impossible, but two threads never share a
 *     logger. FlightBox never sets it; JSBSim's own output stays off at debug 0.
 *   - Element::convert (input_output/FGXMLElement.cpp) is a static unit-conversion map, lazily filled
 *     from the Element ctor and read with operator[] (which can INSERT) while parsing model XML. It is
 *     touched only while LOADING an aircraft — which is why fb-gym's parallel step phase spawns its
 *     units sequentially and only STEPS them in parallel (app/FBTickPool.h).
 *   - The ctor reads the JSBSIM_DEBUG / JSBSIM_DISPERSE environment variables into that shared static,
 *     i.e. they apply to the whole process, not to one airframe.
 * None of these carry physics state, so N airframes integrate independently — and none of them is
 * reachable from Step(), so N airframes may integrate CONCURRENTLY, one thread each. Reference counting
 * inside the engine (SGReferenced) is NOT atomic, which is fine for exactly that reason: every property
 * node and every Element hangs off one FGFDMExec's own root and is never shared across instances. */
#include "FGFDMExec.h"
#include "initialization/FGInitialCondition.h"
#include "initialization/FGTrim.h"
#include "input_output/FGPropertyManager.h"
#include "input_output/FGXMLElement.h"
#include "models/FGExternalReactions.h"
#include "models/FGGroundReactions.h"
#include "models/FGLGear.h"
#include "models/FGMassBalance.h"
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

/* The external-stores force channel we ADD to whatever the loaded model already declares (see
 * EnsureStoresForce below). Its own name, so nothing about it can be confused with a force the
 * aircraft.xml declared for its own purposes. */
constexpr const char *kStoresForce = "fb-stores";

/* One numeric leaf `<name unit="U">v</name>`, built in memory — JSBSim's Element is a plain public
 * class, so the model APIs that take one (FGMassBalance::AddPointMass, FGExternalReactions::Load) can
 * be driven without a file and without touching the read-only model tree. */
Element *ValueElement(const char *name, const char *unit, double v) {
  Element *e = new Element(name);
  if (unit) e->AddAttribute("unit", unit);
  char buf[32];
  snprintf(buf, sizeof buf, "%.6f", v);
  e->AddData(buf);
  return e;
}
void AddChild(Element *parent, Element *child) {
  child->SetParent(parent);
  parent->AddChildElement(child);
}
Element *LocationElement(double xIn, double yIn, double zIn) {
  Element *loc = new Element("location");
  loc->AddAttribute("unit", "IN");
  AddChild(loc, ValueElement("x", nullptr, xIn));
  AddChild(loc, ValueElement("y", nullptr, yIn));
  AddChild(loc, ValueElement("z", nullptr, zIn));
  return loc;
}
} // namespace

struct FBFdm::Impl {
  FGFDMExec Exec;
  double ThrottleApplied = 0.0;   /* slew-limited throttle actually fed to the engine (SetControls) */
  double ElevTrim = 0.0;          /* elevator trim (from DoTrim) biasing the FCS's pitch -> neutral = level */
  double StoresCdA = 0.0;         /* carriage drag area of everything loaded (SetStoresDrag) */
  bool   StoresForce = false;     /* the fb-stores external force exists on this instance */
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
  ic->SetPsiDegIC(spawn.HeadingDeg < 0 ? spawn.HeadingDeg + 360.0 : spawn.HeadingDeg);
  if (spawn.Ballistic) {
    /* A store leaving a pylon: attitude and the full velocity VECTOR come from the carrier, so they are
     * set directly rather than derived from a calibrated speed and a level flight path. Same single IC
     * application as every other spawn (FBFdmBoot.h's contract) — only the fields differ. */
    ic->SetThetaDegIC(spawn.PitchDeg);
    ic->SetPhiDegIC(spawn.RollDeg);
    ic->SetVNorthFpsIC(spawn.VelNorthMs / kFt);
    ic->SetVEastFpsIC(spawn.VelEastMs / kFt);
    ic->SetVDownFpsIC(spawn.VelDownMs / kFt);
  } else {
    ic->SetVcalibratedKtsIC(spawn.SpeedMs * kMs2Kt);
    ic->SetFlightPathAngleDegIC(0.0);   /* level */
  }
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
   * airframe departs violently on the first advance.
   *
   * NOT for a released store (Ballistic). Nothing leaves a pylon with its motor already burning: a
   * store separates first and lights (if it lights at all) when its own module commands it. The
   * distinction is not cosmetic — FGPropulsion::InitRunning slams the throttle to 1 and then time-
   * marches the engine to a steady state, which for a SOLID rocket (FGRocket: ignition is throttle ==
   * 1, and once lit it burns to depletion regardless) means the motor would already be lit at the
   * initial condition and no command could hold it. An unpowered store has no engines, so this is
   * bit-identical for every store that ever flew before the first missile. */
  if (!spawn.Ballistic) {
    auto pr = ex.GetPropulsion();
    for (unsigned i = 0; i < pr->GetNumEngines(); i++) pr->InitRunning(i);
  }
  if (spawn.FbwOverride) ex.SetPropertyValue("fcs/fbw-override", 1.0);
  ex.Setdt(kStepS);
  /* A V=0 ground start (SpeedMs<=0, e.g. a runway spawn ahead of the takeoff roll) has no aerodynamic
   * trim solution: zero airspeed means zero aero force/moment, so udot/qdot cannot be zeroed by any
   * elevator position. FGTrim used to run anyway and report "not trimmable", leaving ElevTrim at the
   * failed search's last iterate (noise) — set the neutral trim directly instead, matching a real jet's
   * untrimmed stick before the roll. Airborne/rolling ICs still get the real search. */
  bool trimmed = false;
  /* A released store has neither a control surface to trim with nor an engine to balance against — the
   * IC above IS its flight state, and a trim search would only perturb it. */
  if (spawn.SpeedMs > 0.0 && !spawn.Ballistic) {
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
  if (spawn.Ballistic)
    FBLog::Info("fdm", "loaded", {{"aircraft", spawn.Aircraft}, {"ballistic", true},
                                  {"pitchDeg", spawn.PitchDeg}, {"rollDeg", spawn.RollDeg},
                                  {"vNorthMs", spawn.VelNorthMs}, {"vEastMs", spawn.VelEastMs},
                                  {"vDownMs", spawn.VelDownMs}});
  else if (spawn.SpeedMs > 0.0)
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
  /* The carriage drag of what is loaded, at THIS step's dynamic pressure — the one per-step write the
   * stores add, and only on an airframe that actually carries something (SetStoresDrag). */
  if (P->StoresForce && P->StoresCdA > 0.0)
    ex.SetPropertyValue(std::string("external_reactions/") + kStoresForce + "/magnitude",
                        P->StoresCdA * ex.GetPropertyValue("aero/qbar-psf"));
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

/* JSBSim's own pointmass mechanism, driven from C++ instead of from XML (see FBFdm.h). The index is
 * discovered from the property tree rather than counted here: the model may already declare pointmasses
 * of its own (the F-16 declares its pilot), and after AddPointMass ours is the last one — so the first
 * index whose weight property does NOT exist is the count, and count-1 is us. Generic over any model,
 * no assumption about how many pointmasses an aircraft.xml brought along. */
int FBFdm::AddStorePointMass(const char *name, double xIn, double yIn, double zIn) {
  FGFDMExec &ex = P->Exec;
  auto mb = ex.GetMassBalance();
  if (!mb) return -1;
  Element_ptr pm(new Element("pointmass"));   /* refcounted: never a raw stack Element into JSBSim */
  pm->AddAttribute("name", name ? name : "store");
  AddChild(pm.ptr(), ValueElement("weight", "LBS", 0.0));
  AddChild(pm.ptr(), LocationElement(xIn, yIn, zIn));
  mb->AddPointMass(pm.ptr());
  auto props = ex.GetPropertyManager();
  int n = 0;
  while (props->GetNode("inertia/pointmass-weight-lbs[" + std::to_string(n) + "]", false)) n++;
  return n - 1;
}

void FBFdm::SetStorePointMassLbs(int index, double lbs) {
  if (index < 0) return;
  P->Exec.SetPropertyValue("inertia/pointmass-weight-lbs[" + std::to_string(index) + "]",
                           lbs < 0.0 ? 0.0 : lbs);
}

/* Creates the fb-stores external force on first use. FGExternalReactions::Load APPENDS forces and then
 * re-binds its six aggregate output properties — which the loaded model already bound if IT declared
 * any external force, and a duplicate tie would log an error per property. So the aggregates are untied
 * first and Load re-ties them to the same object: no output, no model file touched, and the aircraft
 * keeps every force it declared plus ours. */
void FBFdm::SetStoresDrag(double cdaFt2, double xIn, double yIn, double zIn) {
  FGFDMExec &ex = P->Exec;
  P->StoresCdA = cdaFt2 > 0.0 ? cdaFt2 : 0.0;
  if (P->StoresCdA <= 0.0 && !P->StoresForce) return;   /* nothing loaded, nothing ever created */
  if (!P->StoresForce) {
    auto er = ex.GetExternalReactions();
    if (!er) return;
    auto props = ex.GetPropertyManager();
    for (const char *agg : {"moments/l-external-lbsft", "moments/m-external-lbsft",
                            "moments/n-external-lbsft", "forces/fbx-external-lbs",
                            "forces/fby-external-lbs", "forces/fbz-external-lbs"}) {
      SGPropertyNode *node = props->GetNode(agg, false);
      if (node && node->isTied()) props->Untie(node);
    }
    Element_ptr root(new Element("external_reactions"));
    Element *force = new Element("force");
    force->AddAttribute("name", kStoresForce);
    force->AddAttribute("frame", "BODY");
    AddChild(force, LocationElement(xIn, yIn, zIn));
    Element *dir = new Element("direction");
    AddChild(dir, ValueElement("x", nullptr, -1.0));   /* body -x = aft: the magnitude IS drag, positive */
    AddChild(dir, ValueElement("y", nullptr, 0.0));
    AddChild(dir, ValueElement("z", nullptr, 0.0));
    AddChild(force, dir);
    AddChild(root.ptr(), force);
    if (!er->Load(root.ptr())) return;
    P->StoresForce = true;
  }
  std::string base = std::string("external_reactions/") + kStoresForce;
  ex.SetPropertyValue(base + "/location-x-in", xIn);
  ex.SetPropertyValue(base + "/location-y-in", yIn);
  ex.SetPropertyValue(base + "/location-z-in", zIn);
  if (P->StoresCdA <= 0.0) ex.SetPropertyValue(base + "/magnitude", 0.0);
}

double FBFdm::GetQbarPsf() const { return P->Exec.GetPropertyValue("aero/qbar-psf"); }
double FBFdm::GetCgXIn() const { return P->Exec.GetPropertyValue("inertia/cg-x-in"); }

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
