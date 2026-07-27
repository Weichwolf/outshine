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

/* Jede Verriegelung antwortet mit ihrem EIGENEN Grund — sie in ein „nein" zusammenzufassen ist genau
 * das, wogegen ein Kommandobus mit Gruenden existiert. */
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
  /* Ein zweiter Abzugsdruck verlaengert den laufenden; der Hochlauf startet NICHT neu, weil die Laeufe
   * nie aufgehoert haben zu drehen. */
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
/* Das INTEGRAL der Rate zwischen zwei Momenten eines Abzugsdrucks (Hochlauf linear ueber `spool`), damit
 * die Taktung des Slots weder Schuesse erfindet noch verschluckt. */
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
      /* Die MUENDUNG, nicht das CG: eine Kanone sitzt Meter davor und daneben, und auf Kanonenentfernung
       * ist dieser Versatz der Unterschied zwischen Treffer und Fehlschuss. */
      double oe = 0.0, on = 0.0, ou = 0.0;
      FBBodyVecToEnu(st.roll, st.pitch, st.yaw, MuzzleFwdM_, MuzzleRightM_, MuzzleDownM_, oe, on, ou);
      double coslat = std::cos(st.lat * kDeg2Rad);
      b.LatDeg = st.lat + on / kMPerDeg;
      b.LonDeg = st.lon + (coslat > 1e-6 ? oe / (kMPerDeg * coslat) : 0.0);
      b.AltM = st.elev + ou;
      /* ...und wie schnell: Eigengeschwindigkeit plus Muendungsgeschwindigkeit entlang des Bore. */
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
  /* Nur ein LESEN eines fremden Blocks, fuer die Telemetrie. Es sitzt auf der Quelle der Kanone, weil
   * „wohin sie zeigte" und „was herauskam" EINE Messung sind. */
  const FBFireControlBlock &fc = state.FireControl;
  bool fcOk = fc.H.Readable();
  SolRangeM_ = fcOk && fc.GunValid ? fc.GunRangeM : -1.0f;
  SolAimErrDeg_ = fcOk && fc.GunValid ? fc.GunAimErrorDeg : -1.0f;
  SolSpanMr_ = fcOk && fc.GunValid ? fc.GunSpanMr : -1.0f;
  SolInFunnel_ = fcOk && fc.GunInFunnel ? 1 : 0;
}

void FBGunSystem::DeclareTelemetry(FBTelemetrySchema &schema) const {
  /* ERSTE Spalte ist die Blockgueltigkeit — Begruendung in FBRwrSystems Schema. */
  schema.Add("blk_gun");
  schema.Add("gun_rounds");
  schema.Add("gun_fired");
  schema.Add("gun_firing");
  schema.Add("gun_triggers");
  schema.Add("gun_refused");
  schema.Add("gun_burst");
  /* Die Loesung, mit der gezielt wurde: Entfernung zum vorhergesagten Treffpunkt, Ablage der Nase vom
   * geforderten Bore, Winkelausdehnung des Ziels dort, und das Urteil des Trichters. */
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
