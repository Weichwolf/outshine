/* FlightBox — FBRwrSystem: die PASSIVE Haelfte des Defensiv-Slots und das Spiegelbild des Radars —
 * nicht „was ist da draussen", sondern „wer schaut mich an". Liest ausschliesslich publizierte
 * Emissions-Signaturen, prueft zwei Geometrien, meldet KEINE Entfernung.
 * doc/flightbox/sensors.md, Abschnitt 5. */
#ifndef FBRWRSYSTEM_H
#define FBRWRSYSTEM_H

#include "FBRwrThreat.h"
#include "FBState.h"
#include "FBTeam.h"
#include "FBTelemetry.h"
#include "FBUnits.h"
#include "FBFdm.h"

namespace FlightBox {

class FBUnit;
class FBUnitRegistry;

class FBRwrSystem : public FBTelemetrySource {
public:
  /* [SET, DERIVED] Einweg gegen Zweiweg: der Sender braucht Hin- UND Rueckweg (1/r^4), dieser Empfaenger
   * sitzt nur in der Hinhaelfte (1/r^2) — Hoerweite ist ein VIELFACHES der Erfassungsreichweite, kein
   * Bruchteil. Literatur nennt 1,5 bis 3; 2,0 ist die Mitte. */
  static constexpr double kBeamRangeFactor = 2.0;
  /* [SET] Haltezeit: eine wegstreichende Keule und ein abgeschalteter Sender sehen im ersten Moment
   * gleich aus. In SEKUNDEN, weil dieser Empfaenger kein eigenes Frame hat (er hoert durchgehend). */
  static constexpr double kHoldS = 2.0;
  /* [SET] Der Ersatz fuer den Erkennungston: „neu" ist ein publizierter Zustand mit Lebensdauer. */
  static constexpr double kNewThreatS = 1.0;
  /* [SET] Die Radialskala des Scopes: der Modus waehlt den Ring, die Empfangsleistung bewegt das Symbol
   * innerhalb seines Rings (der Abstand vom Zentrum ist LETHALITAET, nicht Entfernung). */
  static constexpr double kLethalitySearch = 0.20;
  static constexpr double kLethalityTrack = 0.55;
  static constexpr double kLethalityMissile = 0.85;
  static constexpr double kLethalitySignalWeight = 0.15;

  ~FBRwrSystem() override = default;

  /* Die FRAKTION wird gespeichert und absichtlich NIE gelesen: ein Warnempfaenger hoert eine Wellenform,
   * keine Zugehoerigkeit — ein befreundetes Radar erzeugt exakt dasselbe Symbol. */
  void SetIdentity(int selfId, FBUnitTeam team) { SelfId_ = selfId; SelfTeam_ = team; }

  void SetPowered(bool on);
  bool Powered() const { return Powered_; }

  /* SEARCH-Filter. Dass etwas VERSTECKT wird, bleibt sichtbar (FBRwrBlock::HiddenSearch): Unterdrueckung
   * muss von Abwesenheit unterscheidbar bleiben. */
  void SetSearchShown(bool on) { SearchShown_ = on; }
  bool SearchShown() const { return SearchShown_; }

  /* `net` null = nichts zu hoeren; `simTimeS` ist die absolute Simuhr des Moduls. */
  virtual void Run(FBState &state, const fb_fdm_state &st, const FBUnitRegistry *net, double simTimeS);

  const char *TelemetryName() const override { return "rwr"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

protected:
  /* Die Zahlen des Empfaengers als Hooks. Azimut ist keiner davon: jeder Warnempfaenger deckt 360° ab —
   * verschieden ist, wie weit ueber und unter der Rumpfebene er hoert und wieviel das Scope zeigt. */
  virtual double ElevCoverageDeg() const { return 60.0; }   /* generischer 4-Quadranten-Empfaenger */
  virtual int    MaxDisplayed() const { return kMaxRwrThreats; }

  /* Die Bedrohungsbibliothek ist heute einen Eintrag tief und deshalb immer richtig; das Feld existiert,
   * damit an dem Tag, an dem sie es nicht mehr ist, kein Konsument seine Form aendert. */
  virtual FBEmitterKind Classify(const FBEmitterSignature &sig) const { return sig.Kind; }

private:
  struct Threat {
    int    Id = 0;
    int    UnitId = 0;          /* Korrelationsschluessel von Erkennung zu Erkennung; NIE publiziert */
    double BearingDeg = 0.0, ElDeg = 0.0;
    double SignalNorm = 0.0;
    double FirstS = 0.0, LastS = 0.0;
    FBRwrThreatMode Mode = FBRwrThreatMode::Search;
    FBEmitterKind Kind = FBEmitterKind::Unknown;
    bool   Heard = false;       /* in diesem Tick */
    bool   Blind = false;       /* die Keule kommt an, die Antennen hoeren sie nicht (einmal loggen) */
  };

  /* Liegt dieses Flugzeug in der Keule des Senders? Reine Geometrie AM SENDER, mit dessen publizierter
   * Lage — die Keule ist koerperfest. */
  static bool BeamCovers(const FBEmitterSignature &sig, double rollDeg, double pitchDeg, double yawDeg,
                         double eastM, double northM, double upM);
  static FBRwrThreatMode ModeOf(const FBEmitterSignature &sig, FBEmitterKind kind);
  double Lethality(FBRwrThreatMode mode, double signalNorm) const;
  void   Publish(FBState &state, double simTimeS);

  Threat Threats_[kMaxRwrThreats]{};
  int    Count_ = 0;
  int    NextId_ = 1;

  int SelfId_ = 0;
  FBUnitTeam SelfTeam_ = FBUnitTeam::Friendly;
  bool Powered_ = true;
  bool SearchShown_ = true;

  /* Telemetriesicht auf den letzten Run. */
  int   ThreatCount_ = 0, PriorityMode_ = 0;
  float TopBearingDeg_ = 0.0f, TopElDeg_ = 0.0f, TopLethality_ = 0.0f;
  bool  Launch_ = false, Activity_ = false, NewThreat_ = false;
  int   BlockStatus_ = 0;
};

} // namespace FlightBox
#endif
