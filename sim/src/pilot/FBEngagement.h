/* FlightBox — FBEngagement: EIN BVR-Gefecht als Daten plus sein Debriefing. Scoreboard, nicht Gehirn:
 * FBPilot::Run entscheidet, diese Klasse protokolliert. Jeder Kanal ist aus den EIGENEN Instrumenten
 * berechenbar. doc/pilot-ai.md, Abschnitt 8. */
#ifndef FBENGAGEMENT_H
#define FBENGAGEMENT_H

#include "FBTelemetry.h"

namespace FlightBox::Pilot {

/* Die Zustandsmaschine des Abfangs. Telemetrie-sichtbare Strings — anhaengen, nie umsortieren. */
enum class FBEngageState { Idle, Search, Closing, Attack, Support, Defend, Abort };
const char *FBEngageStateStr(FBEngageState s);

class FBEngagement : public FBTelemetrySource {
public:
  /* Das Bild, das der Pilot je Tick herunterreicht. */
  void Report(FBEngageState state, bool haveTarget, bool locked, double rangeM, double ataDeg,
              double aspectDeg, double closureMs, double esFt, double dt);

  /* Die Ereignisse, je genau einmal — erstes Vorkommen gewinnt. */
  void NoteContact(double nowS);          /* erster fester Radarkontakt auf dem bearbeiteten Ziel */
  void NoteLock(double nowS);             /* erster Single-Target-Track */
  /* Der Abzug MIT dem ganzen Startbereich, wie ihn die Feuerleitung in DIESEM Augenblick meldete. */
  void NoteShot(double nowS, double rangeM, double ataDeg, double aspectDeg, double raeroM, double rtrM,
                double rminM, double ttaS, double ttiS);
  void NoteThreat(double nowS);           /* erste Track-Klassen-Warnung */
  /* Gemessen ab dem CUE — dem Moment, in dem die Warnung eine wurde, die beantwortet werden musste —
   * nicht ab dem ersten je gesehenen Symbol. */
  void NoteDefensiveAction(double nowS, double cueS);
  /* Tatsaechlich ausgestossene PATRONEN (die Zaehlung der Anlage, nicht die der Schalterwuerfe). */
  void NoteChaff(int n);

  /* Ein Tick im FUEHRUNGSFENSTER [Start, Start + vorhergesagte Zeit bis Eigenlenkung]; `locked` sagt,
   * ob der Uplink tatsaechlich gespeist wird. Begrenzt, sonst waere eng_support_f kein Anteil mehr. */
  void NoteSupport(bool locked, double nowS, double dt);

  bool HaveShot() const { return ShotS_ >= 0.0; }
  double ShotS() const { return ShotS_; }
  double ShotTtaS() const { return ShotTtaS_; }
  double ShotTtiS() const { return ShotTtiS_; }
  int  Shots() const { return Shots_; }
  bool Pitbull() const { return Pitbull_; }
  /* Hat die letzte Runde den Schuetzen nicht mehr noetig? Bei einer aktiven Runde endet das Fenster mit
   * der Suchereinschaltung, bei einer HALBAKTIVEN erst mit dem Einschlag — beides steckt schon in
   * NoteSupport, und dies ist nur die Frage von aussen. */
  bool SupportComplete() const { return SupportDone_; }
  double SupportS() const { return SupportS_; }

  void Reset();

  const char *TelemetryName() const override { return "eng"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

private:
  FBEngageState State_ = FBEngageState::Idle;
  bool   Have_ = false, Locked_ = false;
  double RangeM_ = 0.0, AtaDeg_ = 0.0, AspectDeg_ = 0.0, ClosureMs_ = 0.0;
  double EsFt_ = 0.0, EsMinFt_ = 0.0;
  bool   HaveEs_ = false;

  double EngagedS_ = 0.0, DefendS_ = 0.0;
  double DetectS_ = -1.0, LockS_ = -1.0;
  double ShotS_ = -1.0, ShotRangeM_ = -1.0, ShotAtaDeg_ = 0.0, ShotAspectDeg_ = -1.0;
  double ShotRaeroM_ = -1.0, ShotRtrM_ = -1.0, ShotRminM_ = -1.0, ShotTtaS_ = -1.0, ShotTtiS_ = -1.0;
  double SupportS_ = 0.0;
  bool   Pitbull_ = false, SupportDone_ = false;
  double ThreatS_ = -1.0, ReactS_ = -1.0;
  int    Shots_ = 0, Chaff_ = 0;
};

} // namespace FlightBox::Pilot
#endif
