#include "FBEngagement.h"
#include "FBUnits.h"

namespace FlightBox {

const char *FBEngageStateStr(FBEngageState s) {
  switch (s) {
    case FBEngageState::Idle: return "idle";
    case FBEngageState::Search: return "search";
    case FBEngageState::Closing: return "closing";
    case FBEngageState::Attack: return "attack";
    case FBEngageState::Support: return "support";
    case FBEngageState::Defend: return "defend";
    case FBEngageState::Abort: return "abort";
  }
  return "?";
}

void FBEngagement::Reset() {
  *this = FBEngagement{};
}

void FBEngagement::Report(FBEngageState state, bool haveTarget, bool locked, double rangeM,
                          double ataDeg, double aspectDeg, double closureMs, double esFt, double dt) {
  State_ = state;
  Have_ = haveTarget;
  Locked_ = locked;
  RangeM_ = rangeM;
  AtaDeg_ = ataDeg;
  AspectDeg_ = aspectDeg;
  ClosureMs_ = closureMs;
  EsFt_ = esFt;
  /* The minimum is taken over the ENGAGEMENT, not over the run: an aircraft that has not started
   * fighting yet has not spent anything, and seeding the minimum with the first sample keeps the channel
   * from reporting a zero that never happened. */
  if (!HaveEs_ || esFt < EsMinFt_) { EsMinFt_ = esFt; HaveEs_ = true; }
  EngagedS_ += dt;
  if (state == FBEngageState::Defend) DefendS_ += dt;
}

void FBEngagement::NoteContact(double nowS) {
  if (DetectS_ < 0.0) DetectS_ = nowS;
}

void FBEngagement::NoteLock(double nowS) {
  if (LockS_ < 0.0) LockS_ = nowS;
}

void FBEngagement::NoteShot(double nowS, double rangeM, double ataDeg, double aspectDeg, double raeroM,
                            double rtrM, double rminM, double ttaS, double ttiS) {
  Shots_++;
  /* The FIRST shot is the one the metrics describe. A second round at the same target is a different
   * decision with its own geometry, and averaging the two would describe neither — the count says how
   * many were taken, the zone figures stay those of the shot the support metrics belong to. */
  if (ShotS_ >= 0.0) return;
  ShotS_ = nowS;
  ShotRangeM_ = rangeM;
  ShotAtaDeg_ = ataDeg;
  ShotAspectDeg_ = aspectDeg;
  ShotRaeroM_ = raeroM;
  ShotRtrM_ = rtrM;
  ShotRminM_ = rminM;
  ShotTtaS_ = ttaS;
  ShotTtiS_ = ttiS;
}

/* Support is measured in SECONDS THE UPLINK WAS ACTUALLY FED inside the window the round needs it, not
 * in seconds since the launch: a pilot who keeps the nose in but loses the track through the gimbal has
 * stopped supporting the shot even though he is still pointing at it. Pitbull is the derived verdict,
 * taken exactly once — at the end of the window, i.e. at the moment the fire control predicted the
 * seeker would take over, which is when the shot stops needing him either way. */
void FBEngagement::NoteSupport(bool locked, double nowS, double dt) {
  if (ShotS_ < 0.0 || ShotTtaS_ < 0.0 || SupportDone_) return;
  /* The window closes BEFORE this tick is counted, so the accumulated seconds can never exceed the
   * window they are a fraction of. */
  if (nowS - ShotS_ >= ShotTtaS_) { SupportDone_ = true; Pitbull_ = locked; return; }
  if (locked) SupportS_ += dt;
}

void FBEngagement::NoteThreat(double nowS) {
  if (ThreatS_ < 0.0) ThreatS_ = nowS;
}

void FBEngagement::NoteDefensiveAction(double nowS, double cueS) {
  if (ReactS_ < 0.0 && cueS >= 0.0) ReactS_ = nowS - cueS;
}

void FBEngagement::NoteChaff(int n) { if (n > 0) Chaff_ += n; }

void FBEngagement::DeclareTelemetry(FBTelemetrySchema &schema) const {
  schema.Add("eng_state");
  schema.Add("eng_tgt_nm", "nm");
  schema.Add("eng_ata", "deg");
  schema.Add("eng_aspect", "deg");
  schema.Add("eng_clos", "kt");
  schema.Add("eng_locked");
  schema.Add("eng_detect_s", "s");
  schema.Add("eng_lock_s", "s");
  schema.Add("eng_shot_s", "s");
  schema.Add("eng_shot_nm", "nm");
  schema.Add("eng_shot_ata", "deg");
  schema.Add("eng_shot_aspect", "deg");
  schema.Add("eng_shot_rtr_nm", "nm");
  schema.Add("eng_shot_raero_nm", "nm");
  schema.Add("eng_shot_rmin_nm", "nm");
  schema.Add("eng_tta_s", "s");
  schema.Add("eng_tti_s", "s");
  schema.Add("eng_support_s", "s");
  schema.Add("eng_support_f");
  schema.Add("eng_pitbull");
  schema.Add("eng_threat_s", "s");
  schema.Add("eng_react_s", "s");
  schema.Add("eng_defend_s", "s");
  schema.Add("eng_shots");
  schema.Add("eng_chaff");
  schema.Add("eng_es", "ft");
  schema.Add("eng_es_min", "ft");
}

void FBEngagement::SampleTelemetry(FBTelemetryRow &row) const {
  row.Push(std::string(FBEngageStateStr(State_)));
  row.Push(Have_ ? RangeM_ * kMToNm : -1.0);
  row.Push(Have_ ? AtaDeg_ : 0.0);
  row.Push(Have_ ? AspectDeg_ : -1.0);
  row.Push(Have_ ? ClosureMs_ * kMsToKt : 0.0);
  row.Push(Locked_);
  row.Push(DetectS_);
  row.Push(LockS_);
  row.Push(ShotS_);
  row.Push(ShotRangeM_ >= 0.0 ? ShotRangeM_ * kMToNm : -1.0);
  row.Push(ShotAtaDeg_);
  row.Push(ShotAspectDeg_);
  row.Push(ShotRtrM_ >= 0.0 ? ShotRtrM_ * kMToNm : -1.0);
  row.Push(ShotRaeroM_ >= 0.0 ? ShotRaeroM_ * kMToNm : -1.0);
  row.Push(ShotRminM_ >= 0.0 ? ShotRminM_ * kMToNm : -1.0);
  row.Push(ShotTtaS_);
  row.Push(ShotTtiS_);
  row.Push(SupportS_);
  /* A FRACTION, so it is clamped at 1: the run's tick is 0.1 s and the predicted window is quantised to
   * the launch-zone integration's own step, so the last counted tick can straddle the window's end by a
   * few hundredths. Over-support is not a thing this channel can measure — the window is closed. */
  double supportFrac = ShotTtaS_ > 0.0 ? SupportS_ / ShotTtaS_ : 0.0;
  row.Push(supportFrac > 1.0 ? 1.0 : supportFrac);
  row.Push(Pitbull_);
  row.Push(ThreatS_);
  row.Push(ReactS_);
  row.Push(DefendS_);
  row.Push(Shots_);
  row.Push(Chaff_);
  row.Push(EsFt_);
  row.Push(HaveEs_ ? EsMinFt_ : 0.0);
}

} // namespace FlightBox
