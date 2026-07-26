/* FlightBox — fb-test-two-fdm: the coexistence proof for the instance-capable FDM (multi-unit stage 1).
 * Two FBFdm objects live in ONE process at the same time, are spawned at DIFFERENT initial conditions,
 * are commanded DIFFERENTLY every step, and are stepped in lockstep. The test asserts what a future
 * multi-unit mission depends on:
 *   - both airframes load and trim independently (two FGFDMExec instances, each with its own property
 *     tree — see FBFdm.cpp's "not instance-safe in JSBSim" note for the two things that stay global),
 *   - their states DIVERGE according to their own commands (A rolls right, B rolls left), i.e. neither
 *     is reading or writing the other's physics,
 *   - a THIRD airframe spawned at the same IC as A and given the SAME commands reproduces A's state
 *     bit-for-bit, i.e. an instance carries no hidden cross-talk from its neighbours.
 * No threading, no unit list: this stage proves instance capability only.
 *
 * `make test-fdm` builds this -> build/fb-test-two-fdm. Exit 0 = proven; 1 = the airframes did not
 * behave independently; 2 = setup failure (JSBSim init). */
#include "FBFdmBoot.h"
#include "FBLog.h"
#include "FBLogSinks.h"
#include <cmath>
#include <memory>
#include <string>

using namespace FlightBox;

namespace {

FBFdmSpawn MakeSpawn(double lat, double lon, double groundM, double hdgDeg) {
  FBFdmSpawn ic;
  ic.ModelsRoot = "vendor/jsbsim/aircraft";
  ic.Aircraft = "f16";
  ic.LatDeg = lat; ic.LonDeg = lon;
  ic.GroundElevM = groundM;
  ic.HeightOffsetM = 1500.0;   /* airborne, well clear of the ground */
  ic.SpeedMs = 200.0;
  ic.HeadingDeg = hdgDeg;
  ic.FbwOverride = true;       /* flat stick commands below drive the surfaces directly */
  return ic;
}

/* One tick's commands for airframe `roll`: a constant aileron deflection, everything else neutral. */
void Fly(FBFdm &fdm, double roll, fb_fdm_state &st) {
  fdm.SetControls(roll, 0.0, 0.0, 0.6);
  fdm.Step(st);
}

} // namespace

int main() {
  FBStdoutLogSink sink;
  FBLog::SetSink(&sink);
  FBLog::SetLevel(FBLogLevel::Debug);

  /* A: Payerne, heading 090, rolls RIGHT.  B: 300 km north-east, heading 270, rolls LEFT, different
   * ground elevation. C: A's twin — same IC, same commands, spawned and stepped alongside both. */
  std::unique_ptr<FBFdm> a = FBFdmBoot::Spawn(MakeSpawn(46.84335, 6.91523, 441.0, 90.0));
  std::unique_ptr<FBFdm> b = FBFdmBoot::Spawn(MakeSpawn(49.50000, 9.50000, 250.0, 270.0));
  std::unique_ptr<FBFdm> c = FBFdmBoot::Spawn(MakeSpawn(46.84335, 6.91523, 441.0, 90.0));
  if (!a || !b || !c) {
    FBLog::Error("test", "RESULT", {{"result", "SETUP_FAILED"}, {"reason", "jsbsim init"}});
    return 2;
  }
  a->SetGroundElevM(441.0);
  b->SetGroundElevM(250.0);
  c->SetGroundElevM(441.0);
  /* Different fuel loads too — a per-instance propulsion inventory, not a shared one. */
  a->SetFuelPct(80.0);
  b->SetFuelPct(20.0);
  c->SetFuelPct(80.0);

  fb_fdm_state sa{}, sb{}, sc{};
  /* 1 s at the FDM's own 100 Hz step: long enough for the opposite aileron commands to produce a large,
   * unambiguous bank split, short enough that neither bank angle wraps past +-180 deg (the vanilla f16's
   * roll rate is ~190 deg/s — CLAUDE.md Prinzip 5, an accepted model property, not a defect). */
  const int steps = 100;
  for (int i = 0; i < steps; i++) {
    Fly(*a, 0.5, sa);
    Fly(*b, -0.5, sb);
    Fly(*c, 0.5, sc);
  }

  FBLog::Info("test", "fdm_a", {{"lat", sa.lat}, {"lon", sa.lon}, {"altM", sa.elev}, {"roll", sa.roll},
      {"hdg", sa.yaw}, {"casMs", sa.cas}, {"fuelLbs", a->GetFuelTotalLbs()}, {"gndM", a->GetGroundElevM()}});
  FBLog::Info("test", "fdm_b", {{"lat", sb.lat}, {"lon", sb.lon}, {"altM", sb.elev}, {"roll", sb.roll},
      {"hdg", sb.yaw}, {"casMs", sb.cas}, {"fuelLbs", b->GetFuelTotalLbs()}, {"gndM", b->GetGroundElevM()}});
  FBLog::Info("test", "fdm_c", {{"lat", sc.lat}, {"lon", sc.lon}, {"altM", sc.elev}, {"roll", sc.roll},
      {"hdg", sc.yaw}, {"casMs", sc.cas}, {"fuelLbs", c->GetFuelTotalLbs()}, {"gndM", c->GetGroundElevM()}});

  /* Independence: A and B must have diverged in the direction their OWN commands demanded, and must
   * still sit at their own spawn regions / ground floors / fuel loads. */
  bool rolledApart = sa.roll > 45.0 && sb.roll < -45.0 && sa.p > 0.0 && sb.p < 0.0;
  bool ownRegion = std::fabs(sa.lat - 46.84) < 1.0 && std::fabs(sb.lat - 49.50) < 1.0;
  bool ownGround = std::fabs(a->GetGroundElevM() - 441.0) < 0.5 && std::fabs(b->GetGroundElevM() - 250.0) < 0.5;
  bool ownFuel = a->GetFuelTotalLbs() > 2.0 * b->GetFuelTotalLbs();
  /* Reproducibility: C is A's twin — identical IC, identical commands, run interleaved with B. */
  bool twinIdentical = sa.lat == sc.lat && sa.lon == sc.lon && sa.elev == sc.elev &&
                       sa.roll == sc.roll && sa.yaw == sc.yaw && sa.cas == sc.cas;

  bool ok = rolledApart && ownRegion && ownGround && ownFuel && twinIdentical;
  FBLog::Info("test", "RESULT", {{"result", ok ? "COEXIST_OK" : "COEXIST_FAILED"},
      {"rolledApart", rolledApart}, {"ownRegion", ownRegion}, {"ownGround", ownGround},
      {"ownFuel", ownFuel}, {"twinIdentical", twinIdentical},
      {"rollA", sa.roll}, {"rollB", sb.roll}, {"pA", sa.p}, {"pB", sb.p},
      {"deltaLatDeg", sb.lat - sa.lat}});
  return ok ? 0 : 1;
}
