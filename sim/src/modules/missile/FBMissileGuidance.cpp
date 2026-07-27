#include "FBMissileGuidance.h"
#include "FBAtmosphere.h"
#include "FBGeodesy.h"
#include "FBLog.h"
#include "FBMissileSeeker.h"
#include "FBUnits.h"
#include <cmath>

namespace FlightBox::Modules {

namespace {
constexpr double kG = 9.80665;

double Clamp(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
} // namespace

const char *FBMissileGuidance::PhaseName(Phase p) {
  switch (p) {
    case Phase::Inertial: return "INERTIAL";
    case Phase::Midcourse: return "MIDCOURSE";
    case Phase::Terminal: return "TERMINAL";
  }
  return "?";
}

void FBMissileGuidance::Bind(const FBStoreSpec &spec, FBMissileSeeker &seeker) {
  Spec_ = &spec;
  Seeker_ = &seeker;
  Seeker_->SetRangeM(spec.Perf.SeekerRangeM);
}

void FBMissileGuidance::Program(const FBWeaponTargetState &target, int launcherId, double launchS) {
  LauncherId_ = launcherId;
  LaunchS_ = launchS;
  HaveTarget_ = target.Valid;
  TgtLatDeg_ = target.LatDeg; TgtLonDeg_ = target.LonDeg; TgtAltM_ = target.AltM;
  TgtVelE_ = target.VelE; TgtVelN_ = target.VelN; TgtVelU_ = target.VelU;
  /* The LAUNCHER'S look time in the mission-wide clock, so the programming's age is real from the first
   * tick rather than zero because it was just handed over. */
  TgtStampS_ = target.Valid ? target.StampS : -1e9;
  FBLog::Info("missile", "PROGRAMMED", {{"launcher", launcherId}, {"haveTarget", HaveTarget_},
      {"tgtLat", TgtLatDeg_}, {"tgtLon", TgtLonDeg_}, {"tgtAltM", TgtAltM_},
      {"tgtVelE", TgtVelE_}, {"tgtVelN", TgtVelN_}, {"tgtVelU", TgtVelU_}});
}

void FBMissileGuidance::EnterPhase(Phase p, const char *why) {
  if (p == Phase_) return;
  FBLog::Info("missile", "PHASE", {{"from", PhaseName(Phase_)}, {"to", PhaseName(p)}, {"why", why},
      {"tofS", NowS_ - LaunchS_}, {"rangeM", RangeM_}, {"closureMs", ClosureMs_},
      {"losAzDeg", LosAzDeg_}, {"losElDeg", LosElDeg_}});
  Phase_ = p;
}

/* Strict priority: own seeker > uplink > last known. Every branch writes the SAME four fields, so the
 * law below never asks where its numbers came from — only how old they are. */
void FBMissileGuidance::UpdateTarget(const FBState &state, const Fdm::fb_fdm_state &st) {
  /* --- Terminal. The velocity comes from the tracker every pilot owns, fed from the seeker's own radar
   * block: a contact is an echo without velocity, and the law needs one. --- */
  BfmTrack().Update(state, st, NowS_);
  const FBBfmBlock &trk = BfmTrack().Block();
  SeekerLocked_ = state.Radar.H.Readable() && state.Radar.LockIndex >= 0;
  if (SeekerLocked_ && trk.H.Readable() && trk.Locked) {
    double coslat = std::cos(st.lat * kDeg2Rad);
    TgtLatDeg_ = st.lat + trk.NorthM / kMPerDeg;
    TgtLonDeg_ = st.lon + (coslat > 1e-6 ? trk.EastM / (kMPerDeg * coslat) : 0.0);
    TgtAltM_ = st.elev + trk.UpM;
    TgtVelE_ = trk.VelE; TgtVelN_ = trk.VelN; TgtVelU_ = trk.VelU;
    TgtStampS_ = trk.H.StampS;
    HaveTarget_ = true;
    EnterPhase(Phase::Terminal, "own seeker acquired");
    return;
  }
  /* Once terminal, always terminal: a seeker that has held the target does not go back to asking. */
  if (Phase_ == Phase::Terminal) return;

  /* --- Midcourse: the uplink, read like any other instrument — head first, age included. --- */
  if (state.Datalink.H.Readable() && state.Datalink.TrackCount > 0) {
    const FBDatalinkTrack &m = state.Datalink.Tracks[0];
    if (NowS_ - m.ReportTimeS <= kUplinkTimeoutS) {
      TgtLatDeg_ = m.LatDeg; TgtLonDeg_ = m.LonDeg; TgtAltM_ = m.AltM;
      double hdg = m.HeadingDeg * kDeg2Rad;
      TgtVelE_ = m.SpeedMs * std::sin(hdg);
      TgtVelN_ = m.SpeedMs * std::cos(hdg);
      TgtVelU_ = 0.0;   /* the message carries a ground track: there is no climb rate in it */
      TgtStampS_ = m.ReportTimeS;
      HaveTarget_ = true;
      EnterPhase(Phase::Midcourse, "uplink from launcher");
      return;
    }
  }
  /* --- Inertial: nobody is telling this round anything, so it flies what it last knew. --- */
  EnterPhase(Phase::Inertial, Phase_ == Phase::Midcourse ? "uplink lost" : "no uplink yet");
}

Pilot::FBPilotCommands FBMissileGuidance::Run(const FBState &state, FBCommandBus &avionics,
                                       const Systems::FBAirframeControls &airframe, const Fdm::fb_fdm_state &st,
                                       const FBFlightPlan &plan, const FBRunway *runway, double dt) {
  (void)avionics; (void)airframe; (void)plan; (void)runway; (void)dt;
  Pilot::FBPilotCommands c;
  c.Guidance = Pilot::FBPilotGuidance::Manual;
  /* A solid motor has no throttle: full command from separation, and FBFdm's own throttle slew is the
   * safe-separation delay before ignition. */
  c.ManualThr = 1.0;

  UpdateTarget(state, st);
  if (!HaveTarget_) {
    /* Fins centred, motor lit: it flies straight and the lifetime cap ends it. Not a special case,
     * just an absence of guidance. */
    FinPitch_ = FinYaw_ = 0.0;
    return c;
  }

  /* ---- geometry: the estimate, extrapolated to NOW at its last known velocity ---- */
  double ageS = NowS_ - TgtStampS_;
  if (ageS < 0.0) ageS = 0.0;
  double coslat = std::cos(st.lat * kDeg2Rad);
  double predLat = TgtLatDeg_ + TgtVelN_ * ageS / kMPerDeg;
  double predLon = TgtLonDeg_ + (coslat > 1e-6 ? TgtVelE_ * ageS / (kMPerDeg * coslat) : 0.0);
  double predAlt = TgtAltM_ + TgtVelU_ * ageS;

  double rE = 0.0, rN = 0.0;
  FBEnuOffsetM(st.lat, st.lon, predLat, predLon, rE, rN);
  double rU = predAlt - st.elev;
  double rMag = std::sqrt(rE * rE + rN * rN + rU * rU);
  if (rMag < 1.0) rMag = 1.0;
  RangeM_ = rMag;

  /* Own velocity in ENU. fb_fdm_state's vx/vy/vz are X-Plane local (+x east, +y up, +z south). */
  double vE = st.vx, vN = -st.vz, vU = st.vy;
  double relE = TgtVelE_ - vE, relN = TgtVelN_ - vN, relU = TgtVelU_ - vU;

  /* Vc = -(r . v_rel)/|r| ; Omega = (r x v_rel)/|r|^2 ; a_cmd = N*Vc*(Omega x r_hat). */
  double closure = -(rE * relE + rN * relN + rU * relU) / rMag;
  ClosureMs_ = closure;
  double omE = (rN * relU - rU * relN) / (rMag * rMag);
  double omN = (rU * relE - rE * relU) / (rMag * rMag);
  double omU = (rE * relN - rN * relE) / (rMag * rMag);
  LosRateDegS_ = std::sqrt(omE * omE + omN * omN + omU * omU) * kRad2Deg;

  double uE = rE / rMag, uN = rN / rMag, uU = rU / rMag;
  double gain = kNavConstant * (closure > 0.0 ? closure : 0.0);
  double aE = gain * (omN * uU - omU * uN);
  double aN = gain * (omU * uE - omE * uU);
  double aU = gain * (omE * uN - omN * uE);
  aU += kG;   /* gravity bias (class banner): PN says nothing about weight */

  /* Into the body frame through the ONE shared rotation primitive — no second Euler spelling. */
  double aMag = std::sqrt(aE * aE + aN * aN + aU * aU);
  double aAz = 0.0, aEl = 0.0;
  FBEnuToBodyLos(st.roll, st.pitch, st.yaw, aE, aN, aU, aAz, aEl);
  double aUpBody = aMag * std::sin(aEl * kDeg2Rad);        /* body +up = -Z */
  double aRightBody = aMag * std::cos(aEl * kDeg2Rad) * std::sin(aAz * kDeg2Rad);

  double nzCmd = Clamp(aUpBody / kG, -kMaxCommandG, kMaxCommandG);
  double nyCmd = Clamp(aRightBody / kG, -kMaxCommandG, kMaxCommandG);
  NzCmdG_ = nzCmd; NyCmdG_ = nyCmd;

  /* ---- The two lateral-acceleration loops + the roll holder, closed on the round's own accelerometer
   * (st.nz/st.ny) and rate gyros (st.q/st.r): exactly the two instruments a missile has. ---- */
  double qbar = FBDynamicPressure(st.speed, st.elev);
  double scale = qbar > 1.0 ? kQRefPa / qbar : kGainScaleMax;
  scale = Clamp(scale, kGainScaleMin, kGainScaleMax);
  double kp = kFinPerG * kLoopP * scale;
  double ki = kFinPerG * kLoopI * scale;
  double kd = kRateGain * scale;
  double ez = nzCmd - st.nz, ey = nyCmd - st.ny;
  /* Integrate in FIN units and clamp there, so the anti-windup limit is the PHYSICAL one rather than a
   * number in error units that would have to be re-derived per gain. */
  NzInt_ = Clamp(NzInt_ + ki * ez * dt, -kIntegralClamp, kIntegralClamp);
  NyInt_ = Clamp(NyInt_ + ki * ey * dt, -kIntegralClamp, kIntegralClamp);
  double pitchCmd = kp * ez + NzInt_ - kd * (st.q * kDeg2Rad);
  double yawCmd = kp * ey + NyInt_ - kd * (st.r * kDeg2Rad);
  double rollCmd = -kRollGain * st.roll - kRollRateGain * st.p;

  FinPitch_ = Clamp(pitchCmd, -1.0, 1.0);
  FinYaw_ = Clamp(yawCmd, -1.0, 1.0);
  c.ManualPitch = FinPitch_;
  c.ManualYaw = FinYaw_;
  c.ManualRoll = Clamp(rollCmd, -1.0, 1.0);

  /* ---- the seeker: slave it to the estimate, switch it on at the activation range ---- */
  double losAz = 0.0, losEl = 0.0;
  FBEnuToBodyLos(st.roll, st.pitch, st.yaw, rE, rN, rU, losAz, losEl);
  LosAzDeg_ = losAz; LosElDeg_ = losEl;
  if (Seeker_ && Spec_) {
    if (!SeekerLocked_) Seeker_->SlewTo(losAz, losEl);   /* SLAVE until it has its own track */
    if (!Seeker_->Active() && rMag <= Spec_->Perf.ActivationRangeM) {
      Seeker_->SetActive(true);
      FBLog::Info("missile", "SEEKER_ACTIVE", {{"rangeM", rMag}, {"tofS", NowS_ - LaunchS_},
          {"losAzDeg", losAz}, {"losElDeg", losEl}, {"tgtAgeS", ageS}});
    }
  }
  return c;
}

void FBMissileGuidance::DeclareTelemetry(FBTelemetrySchema &schema) const {
  schema.Add("msl_phase");            /* FBMissileGuidance::Phase ordinal */
  schema.Add("msl_range", "m");       /* to the ESTIMATE the law is flying at, never to the truth */
  schema.Add("msl_closure", "m/s");
  schema.Add("msl_losrate", "deg/s"); /* what PN is driving to zero */
  schema.Add("msl_los_az", "deg");    /* the estimate off the round's own nose */
  schema.Add("msl_los_el", "deg");
  schema.Add("msl_nz_cmd", "g");
  schema.Add("msl_ny_cmd", "g");
  schema.Add("msl_fin_pitch");        /* -1..1, what actually reached the fins */
  schema.Add("msl_fin_yaw");
  schema.Add("msl_seeker");           /* 0 = off, 1 = active/searching, 2 = locked */
  schema.Add("msl_tgt_age", "s");     /* since the last real measurement of the target */
}

void FBMissileGuidance::SampleTelemetry(FBTelemetryRow &row) const {
  row.Push((int)Phase_);
  row.Push(RangeM_);
  row.Push(ClosureMs_);
  row.Push(LosRateDegS_);
  row.Push(LosAzDeg_);
  row.Push(LosElDeg_);
  row.Push(NzCmdG_);
  row.Push(NyCmdG_);
  row.Push(FinPitch_);
  row.Push(FinYaw_);
  row.Push(SeekerLocked_ ? 2 : (Seeker_ && Seeker_->Active() ? 1 : 0));
  row.Push(HaveTarget_ ? NowS_ - TgtStampS_ : -1.0);
}

} // namespace FlightBox::Modules
