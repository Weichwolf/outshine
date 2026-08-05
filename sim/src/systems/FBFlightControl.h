/* FlightBox — FBFlightControl: die FBW-Innenschleife. Guidance + JSBSim-Zustand -> normierte Stick-/
 * Throttle-Werte. Zwei innere Arten hinter dem `Flcs`-Flag (Konfiguration, keine Subklasse: Tuning
 * statt Verhalten). Gesetz und Gain-Tabellen: doc/systems.md, Abschnitt 3. */
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
  /* Das Preset einer ROHEN Zelle, die keine eigene Vermessung hat: das MiG-29-Preset mit den ZWEI
   * Zahlen, die eine solche Zelle selbst mitbringt (Nickautoritaet und Grenzwinkel), und den g-Gains
   * auf die Autoritaet normiert. Herleitung in der .cpp. */
  static FBFlightControl Raw(double pitchStickMax, double alphaLimitDeg, double gLimitG);

  /* Gains — public Config-Block; Defaults im ctor, Zellenwerte via F16()/Mig29(). */
  int    Flcs;                           /* 1 = FLCS kommandieren, 0 = rohe Ruder-PD */
  double RollStickMax;
  /* Das Gegenstueck zu RollStickMax in der Nickachse. 1,0 = KEIN DECKEL, und mehr behauptet der Vorgabe-
   * wert nicht: was hinter dem Ausgang sitzt, weiss allein die Zelle. Schliesst dort ein eigener innerer
   * Kreis mit eigenem Anstellwinkel-Begrenzer an, waere ein Deckel hier eine zweite Meinung; IST der
   * Ausgang der Ausschlag, ist dieser Deckel der Grenzwertgeber, den die Zelle sonst nicht hat. */
  double PitchStickMax;
  double KRollRate, KG, KGi, KVs2g, KVsi, KNy, KNyi, KTi, KpSpd, ThrTrim;
  double KpRoll, KdRoll, KpPitch, KdPitch, KdYaw, KCoord, PitchMaxDeg, KAltRaw;
  double NzSlew;                         /* max. g-Kommandoaenderung je Sekunde */
  /* DIE DAEMPFERGLIEDER DES g-ZWEIGS, in Stick je deg/s. 0 = KEIN eigenes Glied hier, und das ist die
   * Vorgabe, weil eine Zelle, deren Ausgang in einen eigenen inneren Kreis geht, ihre Raten dort schon
   * ausgeregelt bekommt — ein zweites Glied waere eine zweite Meinung. IST derselbe Ausgang ein Ruder-
   * ausschlag, schwingt ein reiner Proportionalregler darauf in der Anstellwinkelschwingung, und die
   * Zelle braucht die Glieder. Die Zahl gehoert damit zur Zelle und ist Preset wie jede andere hier. */
  double KqDamp, KpDampRoll;
  /* DER ANSTELLWINKEL-BEGRENZER, in Grad, mit seinem Gesetz darunter. 0 = AUS, und mehr sagt die Vorgabe
   * nicht: ob eine Zelle hier einen braucht, haengt daran, ob hinter dem Ausgang schon einer sitzt.
   * Er gehoert in diese Klasse und nicht ins Modell-Deck — die Begruendung steht seit der Modellrunde
   * in doc/modules/mig29/flight-model-spec.md §7.3 und gilt unveraendert: er ist eine STICK-KRAFT und
   * damit uebersteuerbar, und im Deck vergraben waere er vor core/FBFlightMonitor versteckt, dessen
   * ganzer Sinn es ist, NICHT der Begrenzer zu sein. Er darf nur ZIEHEN verbieten, nie druecken
   * erzwingen. */
  double AlphaLimitDeg;
  /* SEIN GESETZ, und es ist Preset wie der Grenzwert selbst: ein reiner P-Zweig laesst die begrenzte
   * Groesse um Stick/Kp UNTER dem Grenzwert stehen, Kp ist also die Aussage darueber, wie hart diese
   * Zelle an ihre eigene Grenze gehalten werden darf, und der Vorhalt die Zeitkonstante, mit der ihr
   * Anstellwinkel aufbaut. 0 = kein Gesetz, dann laeuft der Zweig nicht: eine Zelle, die einen
   * Grenzwinkel nennt, nennt auch dessen Gesetz. */
  double AlphaLimitKp, AlphaLimitLeadS;
  /* DER LASTVIELFACHEN-BEGRENZER, in g. 0 = AUS — eine Zelle ohne veroeffentlichtes Limit und eine, die
   * ihres hinter dem Ausgang selbst haelt, sagen hier beide dasselbe. doc/modules/air/
   * flight-model-recipe.md §6 nennt diese Klasse seit der Katalogrunde als den Ort des veroeffentlichten
   * g-Limits (A5) — bis hierher stand dort nichts, und der Anker mass folglich die aerodynamische Grenze
   * statt der strukturellen (gemessen: f15c 13,3 g gegen veroeffentlichte 9,0). Gesetz, Vorhalt und
   * Anti-Windup sind Zeile fuer Zeile die des Anstellwinkel-Begrenzers: der KLEINERE von zwei Zweigen,
   * nie ein erzwungener Druck. */
  double GLimitG;

  double GetGIterm(void) const { return GIterm; }
  double GetVsIterm(void) const { return VsIterm; }

private:
  double NzLimit(const Fdm::fb_fdm_state &s);   /* der g-Zweig des Begrenzers, beiden Zweigen gemeinsam */

  double GIterm, VsIterm, NyIterm, ThrIterm, NzPrev;
  /* Zustand NUR des Begrenzers: die gefilterte Anstellwinkelrate und ihr Vorwert. Existiert und laeuft
   * ausschliesslich, wenn AlphaLimitDeg > 0. */
  double AlphaDot, AlphaPrev;
  bool AlphaPrimed;
  double NzLimDot, NzLimPrev;
  bool NzLimPrimed;
  FBControls LastControls_{0.0, 0.0, 0.0, 0.0};
};

} // namespace FlightBox::Systems
#endif
