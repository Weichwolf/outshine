#include "FBGunSystem.h"
#include "FBGeodesy.h"
#include "FBLog.h"
#include "FBUnits.h"
#include <cmath>

namespace FlightBox {

void FBGunSystem::Install(const FBGunSpec &spec, double fwdM, double rightM, double downM,
                          double boreDownDeg, double boreRightDeg) {
  Spec_ = &spec;
  MuzzleFwdM_ = fwdM; MuzzleRightM_ = rightM; MuzzleDownM_ = downM;
  BoreDownDeg_ = boreDownDeg; BoreRightDeg_ = boreRightDeg;
  Rounds_ = spec.Capacity;
}

bool FBGunSystem::SetRounds(int rounds) {
  if (!Spec_ || rounds < 0 || rounds > Spec_->Capacity) return false;
  Rounds_ = rounds;
  return true;
}

/* The interlocks, in the order a jet applies them: is there a gun, is it armed, is the aircraft flying,
 * is there anything in the drum. Each answers with its own reason — collapsing them into "no" is exactly
 * what a command bus with reasons exists to avoid. */
bool FBGunSystem::Trigger(double seconds, double nowS, FBCommandOutcome &outcome,
                          FBCommandReason &reason) {
  Triggers_++;
  if (!Spec_) {
    outcome = FBCommandOutcome::Rejected; reason = FBCommandReason::NotImplemented;
    Refused_++;
    return false;
  }
  if (Arm_ != FBArmState::Arm) {
    outcome = FBCommandOutcome::Rejected; reason = FBCommandReason::HardwarePrecedence;
    Refused_++;
    return false;
  }
  if (Wow_) {
    outcome = FBCommandOutcome::Rejected; reason = FBCommandReason::HardwarePrecedence;
    Refused_++;
    return false;
  }
  if (Rounds_ <= 0) {
    outcome = FBCommandOutcome::Rejected; reason = FBCommandReason::Depleted;
    Refused_++;
    return false;
  }
  if (seconds <= 0.0) {
    outcome = FBCommandOutcome::Rejected; reason = FBCommandReason::OutOfRange;
    Refused_++;
    return false;
  }
  double want = seconds;
  outcome = FBCommandOutcome::Accepted;
  reason = FBCommandReason::None;
  if (want > Spec_->MaxBurstS) {
    want = Spec_->MaxBurstS;
    outcome = FBCommandOutcome::Clamped;
    reason = FBCommandReason::ValueClamped;
  }
  /* A second squeeze while one is running extends it; the spool-up is not restarted, because the barrels
   * never stopped turning. */
  if (FireUntilS_ <= 0.0) FireStartS_ = nowS;
  double end = nowS + want;
  if (end > FireUntilS_) FireUntilS_ = end;
  FBLog::Info("gun", "TRIGGER", {{"burstS", want}, {"rounds", Rounds_}});
  return true;
}

bool FBGunSystem::TakeBurst(FBGunBurst &out) {
  if (PendingCount_ <= 0) return false;
  out = Pending_[0];
  for (int i = 1; i < PendingCount_; i++) Pending_[i - 1] = Pending_[i];
  PendingCount_--;
  return true;
}

namespace {
/* Rounds fired between two moments of a squeeze, `a` and `b` seconds after the trigger came down, with
 * the barrels spooling linearly to full rate over `spool`. The integral of the rate, so no rounds are
 * created or lost by the cadence the slot happens to be cycled at. */
double RoundsBetween(double a, double b, double ratePerS, double spool) {
  auto integral = [&](double x) {
    if (x <= 0.0) return 0.0;
    if (spool <= 0.0) return ratePerS * x;
    if (x < spool) return ratePerS * x * x / (2.0 * spool);
    return ratePerS * (spool * 0.5 + (x - spool));
  };
  double n = integral(b) - integral(a);
  return n > 0.0 ? n : 0.0;
}
} // namespace

void FBGunSystem::Run(FBState &state, const fb_fdm_state &st, double nowS, double dt) {
  if (state.Airframe.H.Readable()) Wow_ = state.Airframe.WeightOnWheels;

  double whole = 0.0;
  if (Spec_ && FireUntilS_ > 0.0 && dt > 0.0) {
    if (Rounds_ <= 0) {
      FireUntilS_ = 0.0;
      Fraction_ = 0.0;
    } else {
      double tickStart = nowS - dt;
      double a = tickStart > FireStartS_ ? tickStart : FireStartS_;
      double b = nowS < FireUntilS_ ? nowS : FireUntilS_;
      double n = b > a ? RoundsBetween(a - FireStartS_, b - FireStartS_, Spec_->RoundsPerMin / 60.0,
                                       Spec_->SpoolUpS)
                       : 0.0;
      Fraction_ += n;
      whole = std::floor(Fraction_);
      Fraction_ -= whole;
      if (whole > Rounds_) whole = Rounds_;
      if (nowS >= FireUntilS_) { FireUntilS_ = 0.0; Fraction_ = 0.0; }
    }
  }

  if (whole >= 1.0) {
    Rounds_ -= (int)whole;
    Fired_ += (int)whole;
    LastBurstRounds_ = whole;
    if (PendingCount_ < kMaxPendingBursts) {
      FBGunBurst &b = Pending_[PendingCount_++];
      b = FBGunBurst{};
      b.LauncherId = SelfId_;
      b.Kind = Spec_->Kind;
      b.Rounds = (int)whole;
      /* WHERE the rounds leave: the muzzle's own place on the airframe, not the CG — a gun sits metres
       * ahead of and beside the centre of gravity, and at gun range that offset is the difference
       * between a hit and a miss. */
      double oe = 0.0, on = 0.0, ou = 0.0;
      FBBodyVecToEnu(st.roll, st.pitch, st.yaw, MuzzleFwdM_, MuzzleRightM_, MuzzleDownM_, oe, on, ou);
      double coslat = std::cos(st.lat * kDeg2Rad);
      b.LatDeg = st.lat + on / kMPerDeg;
      b.LonDeg = st.lon + (coslat > 1e-6 ? oe / (kMPerDeg * coslat) : 0.0);
      b.AltM = st.elev + ou;
      /* ...and how fast: the aircraft's own velocity plus the muzzle velocity along the bore. Both
       * halves are physics and this class knows both, so what crosses the boundary is a plain
       * projectile state (core/FBGun.h's banner). */
      double be = 0.0, bn = 0.0, bu = 0.0;
      FBBodyLosToEnu(st.roll, st.pitch, st.yaw, BoreRightDeg_, -BoreDownDeg_, be, bn, bu);
      b.VelE = st.vx + be * Spec_->MuzzleVelMs;
      b.VelN = -st.vz + bn * Spec_->MuzzleVelMs;
      b.VelU = st.vy + bu * Spec_->MuzzleVelMs;
      b.SimTimeS = nowS;
    }
    if (Rounds_ <= 0) {
      FireUntilS_ = 0.0;
      FBLog::Warn("gun", "DRY", {{"fired", Fired_}});
    }
  }

  FBGunBlock &b = state.Gun;
  b.Arm = Arm_;
  b.Kind = Spec_ ? (uint8_t)Spec_->Kind : (uint8_t)FBGunKind::None;
  b.RoundsRemaining = Rounds_;
  b.RoundsFired = Fired_;
  b.Firing = FireUntilS_ > 0.0;
  b.Ready = Spec_ != nullptr && Arm_ == FBArmState::Arm && Rounds_ > 0 && !Wow_;
  b.H.Publish(state.NowS);
  BlockStatus_ = (int)b.H.Status;
  /* The AIMING SOLUTION this gun is being pointed with, cached for telemetry only. It is a READ of
   * another system's block (the fire control writes it, modules/f16/FBF16FireControl), which any system
   * may do — what would be wrong is writing it. It sits on the gun's own source because "where the gun
   * was pointing when it fired" and "what came out of it" are one measurement, and splitting them
   * across two files would make the funnel impossible to check against the rounds. */
  const FBFireControlBlock &fc = state.FireControl;
  bool fcOk = fc.H.Readable();
  SolRangeM_ = fcOk && fc.GunValid ? fc.GunRangeM : -1.0f;
  SolAimErrDeg_ = fcOk && fc.GunValid ? fc.GunAimErrorDeg : -1.0f;
  SolSpanMr_ = fcOk && fc.GunValid ? fc.GunSpanMr : -1.0f;
  SolInFunnel_ = fcOk && fc.GunInFunnel ? 1 : 0;
}

void FBGunSystem::DeclareTelemetry(FBTelemetrySchema &schema) const {
  /* FIRST column is this source's own block validity, for the reason systems/FBRwrSystem states: the gun
   * block was added to FBState long after core/FBStateBusTelemetry's list settled in the middle of every
   * measured telemetry.csv, and appending a name there would move every column to its right. */
  schema.Add("blk_gun");
  schema.Add("gun_rounds");
  schema.Add("gun_fired");
  schema.Add("gun_firing");
  schema.Add("gun_triggers");
  schema.Add("gun_refused");
  schema.Add("gun_burst");
  /* ...and the solution it was aimed with (see Run): range to the predicted intercept point, how far the
   * nose was from the required bore, the target's angular span there, and the funnel's own verdict. */
  schema.Add("gun_sol_rng", "m");
  schema.Add("gun_sol_err", "deg");
  schema.Add("gun_sol_span", "mr");
  schema.Add("gun_in_funnel");
}

void FBGunSystem::SampleTelemetry(FBTelemetryRow &row) const {
  row.Push(BlockStatus_);
  row.Push(Rounds_);
  row.Push(Fired_);
  row.Push(FireUntilS_ > 0.0 ? 1 : 0);
  row.Push(Triggers_);
  row.Push(Refused_);
  row.Push(LastBurstRounds_);
  row.Push((double)SolRangeM_);
  row.Push((double)SolAimErrDeg_);
  row.Push((double)SolSpanMr_);
  row.Push(SolInFunnel_);
}

} // namespace FlightBox
