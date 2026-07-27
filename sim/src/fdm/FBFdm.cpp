/* FlightBox — FBFdm implementation, the ONE translation unit that includes JSBSim headers.
 *
 * NOT instance-safe in JSBSim itself (verified against vendor/jsbsim at the pinned commit); none of the
 * four carries physics state, and none is reachable from Step(), so N airframes may integrate
 * concurrently, one thread each — why and with what consequence: doc/flightbox/fdm.md §3.
 *   - FGJSBBase::debug_lvl — static, SetDebugLevel() is process-wide in effect.
 *   - JSBSim::SetLogger/GetLogger — ONE logger, thread_local at the pinned commit.
 *   - Element::convert — static unit-conversion map, read with an INSERTING operator[] while parsing
 *     model XML; touched only while LOADING, which is why fb-gym spawns sequentially (app/FBTickPool.h).
 *   - JSBSIM_DEBUG / JSBSIM_DISPERSE — read in the ctor into that same shared static. */
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

using namespace JSBSim;

namespace FlightBox {

namespace {
constexpr double kFt    = kFtToM;              /* ft -> m */
constexpr double kR2D   = kRad2Deg;            /* rad -> deg */
constexpr double kMs2Kt = kMsToKt;             /* m/s -> knots */
constexpr double kEscSpinupS  = 0.5;           /* spool ramp; a throttle STEP blows the engine's RPM ODE up */
constexpr double kThrottleSlew = FBFdm::kStepS / kEscSpinupS;

double Clamp01(double v) { return v < 0.0 ? 0.0 : v > 1.0 ? 1.0 : v; }

/* Own force-channel names, so neither can be confused with a force the aircraft.xml declared — nor with
 * each other: carriage drag and battle damage must be able to exist at the same time. */
constexpr const char *kStoresForce = "fb-stores";
constexpr const char *kDamageForce = "fb-damage";

/* One numeric leaf built in memory, so the model APIs that take an Element can be driven without a file
 * and without touching the read-only model tree. */
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
  double ElevTrim = 0.0;          /* trim bias from DoTrim, so neutral stick holds LEVEL */
  double StoresCdA = 0.0;         /* carriage drag area of everything loaded (SetStoresDrag) */
  bool   StoresForce = false;     /* the fb-stores external force exists on this instance */
  double DamageCdA = 0.0;         /* battle-damage drag area (SetDamageDrag) */
  bool   DamageForce = false;
  double Authority = 1.0;         /* control-surface authority scale (SetControlAuthority) */
  double ThrottleMax = 1.0;       /* throttle ceiling (SetThrottleLimit) */
};

FBFdm::FBFdm() : P(std::make_unique<Impl>()) { P->Exec.SetDebugLevel(0); }
FBFdm::~FBFdm() = default;

/* Exception firewall: caught by std::exception (JSBSim's hierarchy derives from it) plus a catch-all, so
 * no JSBSim type has to be named here. doc/flightbox/fdm.md §7. */
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
  /* Every FlightBox model is self-contained under <root>/<ac> — the layout JSBSim's own loaders search
   * FIRST, so no probing: a model without an engine simply never resolves the path. */
  const std::string eng = d + "/engine";
  const std::string sys = d + "/Systems";
  if (!ex.LoadModel(SGPath(r), SGPath(eng), SGPath(sys), spawn.Aircraft)) {
    FBLog::Error("fdm", "LoadModel_failed", {{"aircraft", spawn.Aircraft}, {"root", r}, {"engine", eng}});
    return false;
  }

  auto ic = ex.GetIC();
  ic->SetGeodLatitudeDegIC(spawn.LatDeg);
  ic->SetLongitudeDegIC(spawn.LonDeg);
  /* explicit AGL, or a safe airborne value [SET] */
  double prov = (spawn.HeightOffsetM > 0.0) ? spawn.HeightOffsetM : 3.0;
  ic->SetAltitudeASLFtIC((spawn.GroundElevM + prov) / kFt);
  ic->SetPsiDegIC(spawn.HeadingDeg < 0 ? spawn.HeadingDeg + 360.0 : spawn.HeadingDeg);
  if (spawn.Ballistic) {
    /* A store leaving a pylon takes attitude and the full velocity VECTOR from its carrier, not a
     * calibrated speed on a level flight path. */
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
  /* HeightOffsetM < 0 = "sit on the gear": only now is the CG valid, so re-place at the model's own
   * gear-down clearance — the spawn altitude is the geometry-true wheel height, no first-step jump. */
  if (spawn.HeightOffsetM < 0.0) {
    double gc = GetGroundClearanceM(true);
    if (gc > 0.1) { ic->SetAltitudeASLFtIC((spawn.GroundElevM + gc) / kFt); ex.RunIC(); }
  }
  P->ThrottleApplied = 0.0;
  /* Running engines before trim: no thrust during FGTrim means "udot not trimmable" and a violent
   * departure on the first advance. NOT for a released store — InitRunning slams the throttle to 1, and
   * for a SOLID rocket that means the motor is already lit at the IC with no command able to hold it. */
  if (!spawn.Ballistic) {
    auto pr = ex.GetPropulsion();
    for (unsigned i = 0; i < pr->GetNumEngines(); i++) pr->InitRunning(i);
  }
  if (spawn.FbwOverride) ex.SetPropertyValue("fcs/fbw-override", 1.0);
  ex.Setdt(kStepS);
  /* No trim at V=0 (zero aero force, so no elevator position can zero udot/qdot — FGTrim would leave
   * ElevTrim on a failed search's last iterate) and none for a released store (the IC above IS its
   * flight state). tLongitudinal is more robust than tFull on light/slow airframes. */
  bool trimmed = false;
  if (spawn.SpeedMs > 0.0 && !spawn.Ballistic) {
    try {
      FGTrim trim(&ex, tLongitudinal);
      trimmed = trim.DoTrim();
    } catch (...) { trimmed = false; }
  }
  P->ElevTrim = spawn.SpeedMs > 0.0 ? ex.GetPropertyValue("fcs/elevator-cmd-norm") : 0.0;
  ex.RunIC();   /* clean level IC; held by the trim tab, not by the perturbed search state */
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

/* Same firewall, but LATCHING: once the integrator has raised, every later Step is a no-op and `o` keeps
 * its last good values — a frozen but FINITE state for the caller. */
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
  /* Carriage drag at THIS step's dynamic pressure — one write, only on an airframe that carries. */
  if (P->StoresForce && P->StoresCdA > 0.0)
    ex.SetPropertyValue(std::string("external_reactions/") + kStoresForce + "/magnitude",
                        P->StoresCdA * ex.GetPropertyValue("aero/qbar-psf"));
  if (P->DamageForce && P->DamageCdA > 0.0)
    ex.SetPropertyValue(std::string("external_reactions/") + kDamageForce + "/magnitude",
                        P->DamageCdA * ex.GetPropertyValue("aero/qbar-psf"));
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
  o.cas   = ex.GetPropertyValue("velocities/vc-fps") * kFt;   /* density-corrected */
  o.mach  = ex.GetPropertyValue("velocities/mach");
  o.vx    =  ex.GetPropertyValue("velocities/v-east-fps")  * kFt;   /* +x east */
  o.vy    = -ex.GetPropertyValue("velocities/v-down-fps")  * kFt;   /* +y up   */
  o.vz    = -ex.GetPropertyValue("velocities/v-north-fps") * kFt;   /* +z south */
  o.nx    = ex.GetPropertyValue("accelerations/Nx");   /* long g (forward+) */
  o.ny    = ex.GetPropertyValue("accelerations/Ny");   /* lat g (right+) */
  o.nz    = ex.GetPropertyValue("accelerations/Nz");   /* normal g (~+1 level) */
  o.alphaDeg = ex.GetPropertyValue("aero/alpha-deg");
}

void FBFdm::SetControls(double roll, double pitch, double yaw, double thr) {
  /* Battle damage acts HERE, between commanding system and physics: the FCS goes on issuing what it
   * always issued and the aircraft stops answering. Both are 1.0 undamaged, so no effect until hit. */
  roll *= P->Authority; pitch *= P->Authority; yaw *= P->Authority;
  if (thr > P->ThrottleMax) thr = P->ThrottleMax;
  /* Slew, don't step: a 0->0.95 jump blows the engine's RPM ODE up and departs the airframe. */
  double &applied = P->ThrottleApplied;
  if      (thr > applied + kThrottleSlew) applied += kThrottleSlew;
  else if (thr < applied - kThrottleSlew) applied -= kThrottleSlew;
  else                                    applied  = thr;
  FGFDMExec &ex = P->Exec;
  ex.SetPropertyValue("fcs/aileron-cmd-norm",  roll);
  ex.SetPropertyValue("fcs/elevator-cmd-norm", -pitch + P->ElevTrim);   /* JSBSim +elevator = nose DOWN */
  ex.SetPropertyValue("fcs/rudder-cmd-norm",   yaw);   /* +yaw coordinates the turn, -yaw slips it */
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

/* The index is DISCOVERED, not counted: the model may declare pointmasses of its own, and after
 * AddPointMass ours is the last — first index whose weight property is absent is the count, minus 1. */
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

/* Creates one named body-axis DRAG force on first use. The six aggregate output properties are UNTIED
 * first because FGExternalReactions::Load re-binds them and a duplicate tie logs an error per property —
 * the model already bound them if it declared an external force of its own. */
bool FBFdm::EnsureDragForce(const char *name, double xIn, double yIn, double zIn) {
  FGFDMExec &ex = P->Exec;
  auto er = ex.GetExternalReactions();
  if (!er) return false;
  auto props = ex.GetPropertyManager();
  for (const char *agg : {"moments/l-external-lbsft", "moments/m-external-lbsft",
                          "moments/n-external-lbsft", "forces/fbx-external-lbs",
                          "forces/fby-external-lbs", "forces/fbz-external-lbs"}) {
    SGPropertyNode *node = props->GetNode(agg, false);
    if (node && node->isTied()) props->Untie(node);
  }
  Element_ptr root(new Element("external_reactions"));
  Element *force = new Element("force");
  force->AddAttribute("name", name);
  force->AddAttribute("frame", "BODY");
  AddChild(force, LocationElement(xIn, yIn, zIn));
  Element *dir = new Element("direction");
  AddChild(dir, ValueElement("x", nullptr, -1.0));   /* body -x = aft: the magnitude IS drag, positive */
  AddChild(dir, ValueElement("y", nullptr, 0.0));
  AddChild(dir, ValueElement("z", nullptr, 0.0));
  AddChild(force, dir);
  AddChild(root.ptr(), force);
  return er->Load(root.ptr());
}

void FBFdm::SetStoresDrag(double cdaFt2, double xIn, double yIn, double zIn) {
  FGFDMExec &ex = P->Exec;
  P->StoresCdA = cdaFt2 > 0.0 ? cdaFt2 : 0.0;
  if (P->StoresCdA <= 0.0 && !P->StoresForce) return;   /* nothing loaded, nothing ever created */
  if (!P->StoresForce) {
    if (!EnsureDragForce(kStoresForce, xIn, yIn, zIn)) return;
    P->StoresForce = true;
  }
  std::string base = std::string("external_reactions/") + kStoresForce;
  ex.SetPropertyValue(base + "/location-x-in", xIn);
  ex.SetPropertyValue(base + "/location-y-in", yIn);
  ex.SetPropertyValue(base + "/location-z-in", zIn);
  if (P->StoresCdA <= 0.0) ex.SetPropertyValue(base + "/magnitude", 0.0);
}

void FBFdm::SetControlAuthority(double norm) {
  P->Authority = norm < 0.0 ? 0.0 : norm > 1.0 ? 1.0 : norm;
}

void FBFdm::SetThrottleLimit(double maxNorm) {
  P->ThrottleMax = maxNorm < 0.0 ? 0.0 : maxNorm > 1.0 ? 1.0 : maxNorm;
}

void FBFdm::SetDamageDrag(double cdaFt2) {
  P->DamageCdA = cdaFt2 > 0.0 ? cdaFt2 : 0.0;
  if (P->DamageCdA <= 0.0 && !P->DamageForce) return;   /* undamaged: the channel never exists */
  /* Through the CURRENT CG: drag without a claimed pitching moment — where the holes are is not
   * something this model knows. */
  double xIn = GetCgXIn();
  if (!P->DamageForce) {
    if (!EnsureDragForce(kDamageForce, xIn, 0.0, 0.0)) return;
    P->DamageForce = true;
  }
  std::string base = std::string("external_reactions/") + kDamageForce;
  P->Exec.SetPropertyValue(base + "/location-x-in", xIn);
  if (P->DamageCdA <= 0.0) P->Exec.SetPropertyValue(base + "/magnitude", 0.0);
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
