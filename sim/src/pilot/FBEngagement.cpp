#include "FBEngagement.h"
#include "FBUnits.h"

namespace FlightBox::Pilot {

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
  /* Das Minimum laeuft ueber das GEFECHT, nicht ueber den Lauf: wer noch nicht kaempft, hat noch nichts
   * ausgegeben — sonst meldete der Kanal eine Null, die es nie gab. */
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
  /* NUR der erste Schuss beschreibt die Metrik: ein zweiter ist eine andere Entscheidung mit eigener
   * Geometrie, und Mitteln beschriebe keine von beiden. Der Zaehler sagt, wie viele fielen. */
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

/* Fuehrung wird in SEKUNDEN GEMESSEN, IN DENEN DER UPLINK TATSAECHLICH GESPEIST WURDE, nicht in
 * Sekunden seit dem Start: wer die Nase drin behaelt, aber den Track durch den Kardanwinkel verliert,
 * stuetzt den Schuss nicht mehr. `Pitbull` ist das abgeleitete Urteil, genau einmal gefaellt. */
void FBEngagement::NoteSupport(bool locked, double nowS, double dt) {
  if (ShotS_ < 0.0 || ShotTtaS_ < 0.0 || SupportDone_) return;
  /* Das Fenster schliesst VOR der Zaehlung dieses Ticks — die Summe kann es nie uebersteigen. */
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
  /* Ein ANTEIL, deshalb bei 1 gekappt: 0,1-s-Tick gegen ein auf den Integrationsschritt quantisiertes
   * Fenster — der letzte gezaehlte Tick kann dessen Ende um Hundertstel ueberspannen. */
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

} // namespace FlightBox::Pilot
