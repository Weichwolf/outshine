#include "FBMig29Director.h"
#include "FBGeodesy.h"
#include "FBLog.h"
#include "FBUnits.h"
#include <cmath>

namespace FlightBox::Modules {

namespace {
/* fb_fdm_state's velocity is X-Plane local (+x east, +y up, +z south), see fdm/FBFdm.h. */
inline void OwnVelEnu(const Fdm::fb_fdm_state &own, double &e, double &n, double &u) {
  e = own.vx; n = -own.vz; u = own.vy;
}
} // namespace

/* The aim point is the ACTIVE STEERPOINT, reconstructed off the nav block's own bearing and distance
 * exactly as modules/f16/FBF16FireControl does it — a briefed point, the same class of knowledge a
 * `wp` line is, and no sensor of any kind. The impact plane is that steerpoint's own elevation: the
 * "less accurate way" of DCS-EA p.100 for the case where the laser has not (yet) answered. */
bool FBMig29Director::AimPoint(const FBState &state, const Fdm::fb_fdm_state &own, double &latDeg,
                               double &lonDeg, double &planeM) const {
  if (!state.Nav.H.Readable()) return false;
  double distM = state.Nav.SteerDistNm * kNmToM;
  double brg = state.Nav.SteerBearingDeg * kDeg2Rad;
  double coslat = std::cos(own.lat * kDeg2Rad);
  latDeg = own.lat + distM * std::cos(brg) / kMPerDeg;
  lonDeg = own.lon + (coslat > 1e-6 ? distM * std::sin(brg) / (kMPerDeg * coslat) : 0.0);
  planeM = state.Nav.SteerElevFt * kFtToM;
  return true;
}

bool FBMig29Director::Range(const FBState &state, const Fdm::fb_fdm_state &own, double nowS) {
  double lat = 0.0, lon = 0.0, planeM = 0.0;
  if (!Engaged() || !AimPoint(state, own, lat, lon, planeM)) return false;
  double horizM = FBPlanarDistM(own.lat, own.lon, lat, lon);
  double dh = own.elev - planeM;
  double slantM = std::sqrt(horizM * horizM + dh * dh);
  if (slantM > kLaserReachM) {
    FBLog::Warn("director", "LRF_NO_RETURN", {{"slantM", slantM}, {"reachM", kLaserReachM}});
    return false;
  }
  HaveRange_ = true;
  RangeM_ = slantM;
  RangeAtS_ = nowS;
  if (State_ == FBDirectorState::Search || State_ == FBDirectorState::Refused)
    State_ = FBDirectorState::Ranged;
  FBLog::Info("director", "LRF_RANGE", {{"slantM", slantM}, {"altM", own.elev}, {"planeM", planeM}});
  return true;
}

/* THE PLAN. It marches the aircraft's OWN reference trajectory — straight, wings level, holding the
 * velocity vector it has right now — and asks at every step "would a release here land on the aim
 * point". The first crossing is the release time, and the flight-path angle of that same reference is
 * the commanded normal load factor the pilot is then handed.
 *
 * IT IS COMPUTED ONCE. Everything the aircraft subsequently does differently is the delivery error,
 * uncorrected, and that is the definition of a director (doc/modules/mig29/weapons.md §5.4.1). */
FBMig29Director::Plan FBMig29Director::Solve(const FBState &state, const Fdm::fb_fdm_state &own,
                                             double nowS) const {
  Plan p;
  double aimLat = 0.0, aimLon = 0.0, planeM = 0.0;
  if (!Sel_ || !AimPoint(state, own, aimLat, aimLon, planeM)) return p;
  double ve = 0.0, vn = 0.0, vu = 0.0;
  OwnVelEnu(own, ve, vn, vu);
  double gs = std::sqrt(ve * ve + vn * vn);
  if (gs <= 1.0) return p;
  double speed = std::sqrt(ve * ve + vn * vn + vu * vu);
  double trackDeg = std::atan2(ve, vn) * kRad2Deg;
  double coslat = std::cos(own.lat * kDeg2Rad);

  auto stateAt = [&](double t) {
    FBReleaseState r;
    r.LatDeg = own.lat + vn * t / kMPerDeg;
    r.LonDeg = own.lon + (coslat > 1e-6 ? ve * t / (kMPerDeg * coslat) : 0.0);
    r.AltM = own.elev + vu * t;
    r.VelE = ve; r.VelN = vn; r.VelU = vu;
    return r;
  };
  auto alongAt = [&](double t, FBImpactPrediction &pred) {
    FBReleaseState r = stateAt(t);
    pred = FBSolveImpactPoint(Sel_->Perf, r, planeM);
    if (!pred.Valid) return 0.0;
    FBAimSolution a = FBSolveAim(pred, r.LatDeg, r.LonDeg, aimLat, aimLon, trackDeg, gs);
    return a.Valid ? a.AlongErrM : 0.0;
  };

  FBImpactPrediction pred0{};
  double a0 = alongAt(0.0, pred0);
  /* Already at or past the release point: DCS-EA p.101 branches on "angles equal" and "release angle
   * greater", and gives no procedure for the third case because there is nothing to fly to. */
  if (!pred0.Valid || a0 <= 0.0) return p;

  double tPrev = 0.0, aPrev = a0;
  for (double t = kPlanStepS; t <= kPlanMaxS; t += kPlanStepS) {
    FBImpactPrediction pr{};
    double a = alongAt(t, pr);
    if (!pr.Valid) break;   /* the reference flew into the impact plane before a solution existed */
    if (a <= 0.0) {
      /* The crossing is INTERPOLATED and then re-integrated at that instant: the 0.1 s march quantises
       * the release point to 23 m at strike speed, and a solution recorded a whole step late would put
       * that quantisation into the measured delivery error as if the delivery had caused it. */
      double f = (aPrev - a) > 1e-9 ? aPrev / (aPrev - a) : 0.0;
      double tRel = tPrev + f * (t - tPrev);
      FBImpactPrediction at{};
      alongAt(tRel, at);
      if (!at.Valid) return p;
      p.Valid = true;
      p.ReleaseAtS = nowS + tRel;
      /* n = cos(gamma) is the load factor that HOLDS a straight flight path; that is what the ring
       * asks for and it is 1.0 in a level run and less in a dive. */
      p.FpaDeg = std::asin(speed > 1e-6 ? vu / speed : 0.0) * kRad2Deg;
      p.CmdG = std::cos(p.FpaDeg * kDeg2Rad);
      p.TasMs = speed;
      p.TrackDeg = trackDeg;
      p.AltM = own.elev + vu * tRel;
      p.PredLatDeg = at.LatDeg;
      p.PredLonDeg = at.LonDeg;
      p.PredTofS = at.TofS;
      p.ArmMarginS = at.ArmMarginS;
      return p;
    }
    tPrev = t; aPrev = a;
  }
  return p;
}

bool FBMig29Director::Consent(bool down, const FBState &state, const Fdm::fb_fdm_state &own,
                              double nowS) {
  if (!down) {
    /* DCS-EA p.104 allows the trigger to be let go and pressed again. Letting go abandons the
     * countdown — and on this airframe that is a real cost, because it is the pilot who has to. */
    if (State_ == FBDirectorState::Steer) {
      FBLog::Warn("director", "DIRECTOR_ABANDON",
                  {{"ttrS", Plan_.ReleaseAtS - nowS}, {"cmdG", Plan_.CmdG}});
      Plan_ = Plan{};
      Solution_ = FBReleaseSolution{};
      State_ = HaveRange_ ? FBDirectorState::Ranged : FBDirectorState::Search;
    }
    return true;
  }

  auto refuse = [&](FBDirectorRefusal r) {
    Refusal_ = r;
    State_ = FBDirectorState::Refused;
    FBLog::Warn("director", "DIRECTOR_REFUSED", {{"reason", FBDirectorRefusalStr(r)},
        {"rangeM", HaveRange_ ? RangeM_ : -1.0}, {"rangeAgeS", HaveRange_ ? nowS - RangeAtS_ : -1.0},
        {"altM", own.elev}, {"gsMs", own.gs}});
    return false;
  };

  if (!Engaged()) return refuse(FBDirectorRefusal::NotEngaged);
  if (!HaveRange_) return refuse(FBDirectorRefusal::NoRange);
  double ageS = nowS - RangeAtS_;
  if (ageS < kConsentMinAgeS) return refuse(FBDirectorRefusal::RangeTooFresh);
  if (ageS > kConsentMaxAgeS) return refuse(FBDirectorRefusal::RangeStale);

  Plan p = Solve(state, own, nowS);
  if (!p.Valid) return refuse(FBDirectorRefusal::NoReleasePoint);
  if (p.ArmMarginS <= 0.0) return refuse(FBDirectorRefusal::WouldNotArm);

  Plan_ = p;
  Refusal_ = FBDirectorRefusal::None;
  State_ = FBDirectorState::Steer;
  TrackSumGErr_ = TrackMaxGErr_ = TrackSumS_ = TrackMaxBankDeg_ = 0.0;

  double aimLat = 0.0, aimLon = 0.0, planeM = 0.0;
  AimPoint(state, own, aimLat, aimLon, planeM);
  Solution_ = FBReleaseSolution{};
  Solution_.Valid = true;
  Solution_.Mode = FBDeliveryMode::Opt;
  Solution_.ImpactLatDeg = p.PredLatDeg;
  Solution_.ImpactLonDeg = p.PredLonDeg;
  Solution_.ImpactElevM = planeM;
  Solution_.TofS = p.PredTofS;
  Solution_.AimLatDeg = aimLat;
  Solution_.AimLonDeg = aimLon;
  Solution_.AimMissM = FBPlanarDistM(aimLat, aimLon, p.PredLatDeg, p.PredLonDeg);
  Solution_.ArmMarginS = p.ArmMarginS;
  Solution_.StampS = nowS;

  FBLog::Info("director", "DIRECTOR_ARM", {{"rangeAgeS", ageS}, {"rangeM", RangeM_},
      {"countdownS", p.ReleaseAtS - nowS}, {"cmdG", p.CmdG}, {"fpaDeg", p.FpaDeg},
      {"tasMs", p.TasMs}, {"altM", own.elev}, {"planAltM", p.AltM},
      {"predTofS", p.PredTofS}, {"planMissM", Solution_.AimMissM}, {"armMarginS", p.ArmMarginS}});
  return true;
}

void FBMig29Director::NotifyRelease(bool accepted, double nowS) {
  double meanGErr = TrackSumS_ > 0.0 ? TrackSumGErr_ / TrackSumS_ : 0.0;
  FBLog::Info("director", "DIRECTOR_RELEASE", {{"accepted", accepted},
      {"plannedAtS", Plan_.ReleaseAtS}, {"nowS", nowS},
      {"cmdG", Plan_.CmdG}, {"meanGErr", meanGErr}, {"maxGErr", TrackMaxGErr_},
      {"maxBankDeg", TrackMaxBankDeg_}, {"trackedS", TrackSumS_},
      /* THE PROCEDURE'S OWN COST, table error excluded by construction. */
      {"openLoopAlongM", OpenLoopAlongM_}, {"openLoopCrossM", OpenLoopCrossM_},
      {"devAltM", DevAltM_}, {"devSpeedMs", DevSpeedMs_}, {"devFpaDeg", DevFpaDeg_}});
  State_ = FBDirectorState::Spent;
  Plan_ = Plan{};
}

void FBMig29Director::Run(FBState &state, const Fdm::fb_fdm_state &own, double nowS) {
  FBFireControlBlock &b = state.FireControl;
  b.DirEngaged = Engaged();
  b.DirRanged = false;
  b.DirRangeM = 0.0f;
  b.DirRangeAgeS = 0.0f;
  b.DirMarkErrM = 0.0f;
  b.DirArmed = false;
  b.DirTimeToReleaseS = 0.0f;
  b.DirCmdG = 0.0f;
  b.DirCurG = 0.0f;
  b.DirAudio = false;

  if (!b.DirEngaged) {
    if (State_ != FBDirectorState::Spent) State_ = FBDirectorState::Off;
    HaveRange_ = false;
    b.DirState = (uint8_t)State_;
    b.DirRefusal = (uint8_t)Refusal_;
    return;
  }
  if (State_ == FBDirectorState::Off) State_ = FBDirectorState::Search;

  /* THE COUNTDOWN HALF. Nothing here re-solves: the only arithmetic is a subtraction from a number
   * committed to at the consent, which is exactly what the source's "time scale" is. */
  if (State_ == FBDirectorState::Steer && Plan_.Valid) {
    double ttrS = Plan_.ReleaseAtS - nowS;
    double curG = state.AirData.H.Readable() ? state.AirData.GLoad : 1.0f;
    double gErr = std::fabs(curG - Plan_.CmdG);
    double bank = std::fabs(FBWrap180(own.roll));
    TrackSumGErr_ += gErr * kSweepS;
    TrackSumS_ += kSweepS;
    if (gErr > TrackMaxGErr_) TrackMaxGErr_ = gErr;
    if (bank > TrackMaxBankDeg_) TrackMaxBankDeg_ = bank;
    b.DirArmed = true;
    b.DirTimeToReleaseS = (float)ttrS;
    b.DirCmdG = (float)Plan_.CmdG;
    b.DirCurG = (float)curG;
    b.DirAudio = ttrS <= kAudioLeadS;
    /* THE NEAREST SWEEP, NOT THE FIRST ONE PAST ZERO. The device releases AT zero; a box that can only
     * look every kSweepS would otherwise release systematically half a sweep late, which at strike
     * speed is a 12 m bias in every delivery this file ever measures — a property of the sampling rate
     * masquerading as a property of the procedure. Rounding to the nearest sweep is the unbiased
     * reading of "the countdown reached zero" and leaves the measured open-loop error to be the state
     * deviation it is meant to be. */
    if (ttrS <= 0.5 * kSweepS) {
      State_ = FBDirectorState::Release;
      /* THE SAME TABLE, TWICE, ONE TICK APART FROM ITSELF — see the member's comment. What the plan
       * promised is Plan_.PredLatDeg/LonDeg; what a release here actually buys is `now`. */
      FBReleaseState rel;
      rel.LatDeg = own.lat; rel.LonDeg = own.lon; rel.AltM = own.elev;
      OwnVelEnu(own, rel.VelE, rel.VelN, rel.VelU);
      FBImpactPrediction now = FBSolveImpactPoint(Sel_->Perf, rel, Solution_.ImpactElevM);
      if (now.Valid)
        FBTrackProjectM(Plan_.PredLatDeg, Plan_.PredLonDeg, Plan_.TrackDeg, now.LatDeg, now.LonDeg,
                        OpenLoopAlongM_, OpenLoopCrossM_);
      double speed = std::sqrt(rel.VelE * rel.VelE + rel.VelN * rel.VelN + rel.VelU * rel.VelU);
      DevAltM_ = own.elev - Plan_.AltM;
      DevSpeedMs_ = speed - Plan_.TasMs;
      DevFpaDeg_ = std::asin(speed > 1e-6 ? rel.VelU / speed : 0.0) * kRad2Deg - Plan_.FpaDeg;
    }
    b.DirState = (uint8_t)State_;
    b.DirRefusal = (uint8_t)Refusal_;
    return;
  }

  /* THE RANGE HALF: a slant range and where the aiming mark sits, and deliberately no time at all.
   * The pilot divides the distance by his own groundspeed — the source shows a range scale before the
   * trigger and a time scale only after it (doc/modules/mig29/weapons.md §5.4.3). */
  if (HaveRange_) {
    double ageS = nowS - RangeAtS_;
    b.DirRanged = true;
    b.DirRangeM = (float)RangeM_;
    b.DirRangeAgeS = (float)ageS;
  }
  double aimLat = 0.0, aimLon = 0.0, planeM = 0.0;
  if (Sel_ && State_ != FBDirectorState::Spent && AimPoint(state, own, aimLat, aimLon, planeM)) {
    FBReleaseState rel;
    rel.LatDeg = own.lat; rel.LonDeg = own.lon; rel.AltM = own.elev;
    OwnVelEnu(own, rel.VelE, rel.VelN, rel.VelU);
    FBImpactPrediction pred = FBSolveImpactPoint(Sel_->Perf, rel, planeM);
    if (pred.Valid && state.AirData.H.Readable()) {
      FBAimSolution aim = FBSolveAim(pred, own.lat, own.lon, aimLat, aimLon, state.AirData.TrackDeg,
                                     own.gs);
      if (aim.Valid) b.DirMarkErrM = (float)aim.AlongErrM;
    }
  }
  b.DirState = (uint8_t)State_;
  b.DirRefusal = (uint8_t)Refusal_;
}

} // namespace FlightBox::Modules
