#include "FBAirMover.h"

#include <cmath>

#include "FBGeodesy.h"
#include "FBLog.h"
#include "FBUnits.h"

namespace FlightBox::Modules {

namespace {
constexpr double kG = 9.80665;
/* [SET] A turn is rolled out of when the heading error is inside this — small enough that the leg is
 * flown straight, large enough that the state machine cannot chatter at one decision tick. */
constexpr double kRolloutDeg = 5.0;
} // namespace

double FBAirMover::TurnRadiusM(double vMs) const {
  double t = std::tan(Spec_.BankDeg * kDeg2Rad);
  return t > 1e-6 ? vMs * vMs / (kG * t) : 1e9;
}

void FBAirMover::Advance(Fdm::fb_fdm_state &st, FBFlightPlan &plan, double dt, double simTimeS) {
  if (!Initialised_) {
    /* THE SPAWN IS ALREADY THE WHOLE TRUTH about a mover, so the first tick inherits it rather than
     * trimming to it: there is no airframe to trim. Where the mission declared no speed, the row's own
     * published cruise fills in. */
    if (st.speed <= 1.0) st.speed = Spec_.CruiseMs;
    TargetAltM_ = st.elev;
    Initialised_ = true;
  }

  double vMs = st.speed;
  double turnR = TurnRadiusM(vMs > 1.0 ? vMs : 1.0);

  /* ---- WHERE IT IS GOING. Three sources in strict priority: a commanded heading (the T1 drag
   * reflex, which overrides everything because that is what a reflex is), the racetrack once the route
   * is flown out, and the active waypoint. */
  double wantHdg = st.yaw;
  double wantAlt = TargetAltM_;

  if (const FBWaypoint *wp = plan.ActiveWaypoint()) {
    double d = FBPlanarDistM(st.lat, st.lon, wp->LatDeg, wp->LonDeg);
    if (wp->AltM > 0.0) wantAlt = wp->AltM;
    if (wp->SpeedKt > 0.0) vMs = wp->SpeedKt * kKtToMs;
    /* A waypoint is made good at one TURN RADIUS, which is the distance at which the turn onto the next
     * leg physically begins — not at an arbitrary capture circle. */
    if (d <= turnR && plan.ActiveIndex() + 1 < plan.Size()) {
      plan.SetActiveIndex(plan.ActiveIndex() + 1);
      wp = plan.ActiveWaypoint();
    } else if (d <= turnR && OrbitLegM_ > 0.0 && !OrbitEntered_) {
      /* THE STATION IS REACHED. The racetrack's axis is the inbound leg's own bearing, so an orbit is
       * a continuation of the route rather than a second geometry the mission has to state. */
      OrbitAxisDeg_ = st.yaw;
      OrbitEntered_ = true;
      OrbitPhase_ = 0;
      OrbitLegRunM_ = 0.0;
      FBLog::Info("air", "ORBIT", {{"axisDeg", OrbitAxisDeg_}, {"legM", OrbitLegM_},
                                   {"periodS", OrbitPeriodS_}, {"t", simTimeS}});
    }
    if (wp) wantHdg = FBBearingDeg(st.lat, st.lon, wp->LatDeg, wp->LonDeg);
  } else if (OrbitLegM_ > 0.0 && !OrbitEntered_) {
    OrbitAxisDeg_ = st.yaw;
    OrbitEntered_ = true;
  }

  if (OrbitEntered_) {
    /* THE RACETRACK, four states and no more: out, turn, back, turn. The turn law below already limits
     * the rate to the row's own radius, so the pattern is commanded as HEADINGS and the geometry falls
     * out of the bank angle instead of being drawn. */
    double back = OrbitAxisDeg_ + 180.0;
    switch (OrbitPhase_) {
      case 0:
        wantHdg = OrbitAxisDeg_;
        if (OrbitLegRunM_ >= OrbitLegM_) { OrbitPhase_ = 1; OrbitLegRunM_ = 0.0; }
        break;
      case 1:
        wantHdg = back;
        if (std::fabs(FBWrap180(back - st.yaw)) < kRolloutDeg) { OrbitPhase_ = 2; OrbitLegRunM_ = 0.0; }
        break;
      case 2:
        wantHdg = back;
        if (OrbitLegRunM_ >= OrbitLegM_) { OrbitPhase_ = 3; OrbitLegRunM_ = 0.0; }
        break;
      default:
        wantHdg = OrbitAxisDeg_;
        if (std::fabs(FBWrap180(OrbitAxisDeg_ - st.yaw)) < kRolloutDeg) {
          OrbitPhase_ = 0;
          OrbitLegRunM_ = 0.0;
        }
        break;
    }
    /* THE DECLARED PERIOD IS THE STRAIGHT-LEG TIME BUDGET, and the file says so rather than pretending
     * to solve a circuit whose turn time depends on the speed it is solving for: v = 2*leg/period. The
     * two semicircles add 2*pi*R/v on top, so the FLOWN circuit is longer than the declared period by
     * exactly that, and the mission gets the number it asked for on the part it controls. */
    if (OrbitPeriodS_ > 0.0) vMs = 2.0 * OrbitLegM_ / OrbitPeriodS_;
  }

  if (HaveCmdHdg_) wantHdg = CmdHdgDeg_;
  if (Spec_.MaxMs > 0.0 && vMs > Spec_.MaxMs) vMs = Spec_.MaxMs;
  if (Spec_.CeilingM > 0.0 && wantAlt > Spec_.CeilingM) wantAlt = Spec_.CeilingM;
  TargetAltM_ = wantAlt;
  turnR = TurnRadiusM(vMs > 1.0 ? vMs : 1.0);

  /* ---- THE TURN. Rate = V/R at the declared bank. */
  double err = FBWrap180(wantHdg - st.yaw);
  double step = (vMs / turnR) * kRad2Deg * dt;
  double turn = std::fabs(err) <= step ? err : (err > 0.0 ? step : -step);
  st.yaw = std::fmod(st.yaw + turn + 360.0, 360.0);
  st.r = dt > 0.0 ? turn / dt : 0.0;
  /* Roll is PUBLISHED, not integrated: a mover has no rotational dynamics, and a bank angle that did
   * not match the turn being flown would be a lie a renderer and an infrared head both read. */
  st.roll = std::fabs(err) < kRolloutDeg ? 0.0 : (turn >= 0.0 ? Spec_.BankDeg : -Spec_.BankDeg);

  /* ---- THE CLIMB, at the row's published rate and never faster. */
  double dAlt = wantAlt - st.elev;
  double climb = Spec_.ClimbMs * dt;
  double dz = std::fabs(dAlt) <= climb ? dAlt : (dAlt > 0.0 ? climb : -climb);
  st.elev += dz;
  st.vy = dt > 0.0 ? dz / dt : 0.0;
  st.pitch = vMs > 1.0 ? std::asin(std::max(-1.0, std::min(1.0, st.vy / vMs))) * kRad2Deg : 0.0;

  /* ---- THE LEG. Plain planar advance on the flown heading; the pose is geodetic, so the same
   * conversion the whole tree uses runs backwards here. */
  double hdg = st.yaw * kDeg2Rad;
  double ds = vMs * dt;
  double dn = ds * std::cos(hdg), de = ds * std::sin(hdg);
  st.lat += dn / kMPerDeg;
  double coslat = std::cos(st.lat * kDeg2Rad);
  if (coslat > 1e-9) st.lon += de / (kMPerDeg * coslat);
  OrbitLegRunM_ += ds;

  st.speed = st.gs = st.cas = vMs;
  /* A coarse constant-a stand-in. It reaches no decision — a mover has no envelope — and is published
   * only so the telemetry schema every unit shares carries a finite number. */
  st.mach = vMs / 300.0;
  st.vx = dt > 0.0 ? de / dt : 0.0;
  st.vz = dt > 0.0 ? -dn / dt : 0.0;
  st.nx = st.ny = 0.0;
  st.nz = 1.0;
  st.alphaDeg = 0.0;
  st.p = st.q = 0.0;
}

} // namespace FlightBox::Modules
