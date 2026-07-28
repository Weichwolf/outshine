/* FlightBox — FBFlightControl: die FBW-Innenschleife. Guidance + JSBSim-Zustand -> normierte Stick-/
 * Throttle-Werte. Zwei innere Arten hinter dem `Flcs`-Flag (Konfiguration, keine Subklasse: Tuning
 * statt Verhalten). Gesetz und Gain-Tabellen: doc/flightbox/systems.md, Abschnitt 3. */
#ifndef FBFLIGHTCONTROL_H
#define FBFLIGHTCONTROL_H

#include "FBAutopilot.h"
#include "FBTelemetry.h"

namespace FlightBox::Systems {

struct FBControls { double Roll, Pitch, Yaw, Thr; };

class FBFlightControl : public FBTelemetrySource {
public:
  FBFlightControl();
  virtual ~FBFlightControl() = default;

  /* Ein 100-Hz-Schritt; mutiert die Integratoren. Der EINE Override-Punkt. */
  virtual FBControls Run(const FBGuidance &g, const Fdm::fb_fdm_state &s);

  const char *TelemetryName() const override { return "flightcontrol"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

  void Reset(void);                      /* alle Integratoren nullen (neuer Flug) */
  static FBFlightControl F16(void);      /* das geflogene F-16-Preset */
  static FBFlightControl Mig29(void);    /* das geflogene MiG-29-Preset — Herleitung in der .cpp */

  /* Gains — public Config-Block; Defaults im ctor, Zellenwerte via F16()/Mig29(). */
  int    Flcs;                           /* 1 = FLCS kommandieren, 0 = rohe Ruder-PD */
  double RollStickMax;
  /* Das Gegenstueck zu RollStickMax in der Nickachse. 1,0 = kein Deckel (die F-16: hinter dem Ausgang
   * sitzt ihre FLCS mit eigenem Anstellwinkel-Begrenzer, der Deckel waere dort eine zweite Meinung).
   * Bei einer mechanisch gesteuerten Zelle IST der Ausgang der Ausschlag und es sitzt nichts dahinter —
   * dort ist dieser Deckel der Grenzwertgeber, den die Zelle nicht hat. */
  double PitchStickMax;
  double KRollRate, KG, KGi, KVs2g, KVsi, KNy, KNyi, KTi, KpSpd, ThrTrim;
  double KpRoll, KdRoll, KpPitch, KdPitch, KdYaw, KCoord, PitchMaxDeg, KAltRaw;
  double NzSlew;                         /* max. g-Kommandoaenderung je Sekunde */
  /* DIE DAEMPFERGLIEDER DES g-ZWEIGS, und beide sind 0, wenn die ZELLE ihre eigenen hat. Der g-Ausgang
   * ist bei der F-16 ein Kommando an eine echte FLCS, die Nick- und Rollrate selbst ausregelt; bei
   * einer mechanisch gesteuerten Zelle IST derselbe Ausgang ein Ruderausschlag, und ein reiner
   * Proportionalregler darauf schwingt in der Anstellwinkelschwingung. Die Zahl gehoert damit zur Zelle
   * und ist Preset wie jede andere hier. */
  double KqDamp, KpDampRoll;
  /* DER ANSTELLWINKEL-BEGRENZER (die MiG-29 nennt ihn SOS), in Grad. 0 = AUS, und das ist die F-16:
   * ihr Begrenzer sitzt in ihrer eigenen FLCS, ein zweiter hier waere eine konkurrierende Meinung.
   * Er gehoert in diese Klasse und nicht ins Modell-Deck — die Begruendung steht seit der Modellrunde
   * in doc/mig29/flight-model-spec.md §7.3 und gilt unveraendert: er ist eine STICK-KRAFT und damit
   * uebersteuerbar, und im Deck vergraben waere er vor core/FBFlightMonitor versteckt, dessen ganzer
   * Sinn es ist, NICHT der Begrenzer zu sein. Er darf nur ZIEHEN verbieten, nie druecken erzwingen. */
  double AlphaLimitDeg;

  double GetGIterm(void) const { return GIterm; }
  double GetVsIterm(void) const { return VsIterm; }

private:
  double GIterm, VsIterm, NyIterm, ThrIterm, NzPrev;
  /* Zustand NUR des Begrenzers: die gefilterte Anstellwinkelrate und ihr Vorwert. Existiert und laeuft
   * ausschliesslich, wenn AlphaLimitDeg > 0. */
  double AlphaDot, AlphaPrev;
  bool AlphaPrimed;
  FBControls LastControls_{0.0, 0.0, 0.0, 0.0};
};

} // namespace FlightBox::Systems
#endif
