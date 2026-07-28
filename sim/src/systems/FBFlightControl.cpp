#include "FBFlightControl.h"
#include <cmath>

namespace FlightBox::Systems {

static double Clamp(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }

/* Das Gesetz des Anstellwinkel-Begrenzers, gemessen auf einer Zelle OHNE eigenen (s. Run()). Keine
 * Preset-Zahlen: der Begrenzer hat EINE Stellgroesse, seinen Grenzwinkel, und die ist AlphaLimitDeg.
 * kSosRateFilter ~ 0,07 s Zeitkonstante bei 100 Hz — eine rohe Differenz ist ueberwiegend Quantisierung. */
static const double kSosKp = 0.20, kSosLeadS = 0.45, kSosRateFilter = 0.15;
/* Der g-Zweig desselben Begrenzers. Ein PROPORTIONALER Begrenzer laesst die Groesse um Stick/kp unter
 * dem Grenzwert stehen; kGosKp ist deshalb aus einem genannten Kriterium GESETZT und nicht aus einer
 * Analogie: der Abfall soll bei voller Nickautoritaet unter 3 % des Grenzwerts bleiben. Die schaerfste
 * Zeile des Katalogs (P = 0,28 bei einem 4,5-g-Limit) verlangt kp >= 0,28/(0,03*4,5) = 2,07. Mit 0,25 —
 * dem Wert des g-REGELZWEIGS, der ein Ziel ausregelt und keine Grenze haelt — gab derselbe Begrenzer
 * 7,94 g auf eine veroeffentlichte 9,0 (-11,8 %), und das war der Regler und nicht die Zelle.
 * Der Vorhalt ist kuerzer als der des Anstellwinkels, weil die Lastvielfache dem Ruder ohne die
 * Zeitkonstante des Anstellwinkelaufbaus folgt. */
static const double kGosKp = 2.10, kGosLeadS = 0.35;

FBFlightControl::FBFlightControl()
  : Flcs(0), RollStickMax(1.0), PitchStickMax(1.0),
    KRollRate(0.06), KG(0.4), KGi(2.0), KVs2g(0.05), KVsi(0.02), KNy(1.5), KNyi(2.0),
    KTi(0.002), KpSpd(0.03), ThrTrim(0.55),
    KpRoll(0.022), KdRoll(0.004), KpPitch(0.06), KdPitch(0.010), KdYaw(0.006), KCoord(0.004),
    PitchMaxDeg(15), KAltRaw(0.05), NzSlew(1.5), KqDamp(0.0), KpDampRoll(0.0), AlphaLimitDeg(0.0),
    GLimitG(0.0),
    GIterm(0), VsIterm(0), NyIterm(0), ThrIterm(0), NzPrev(0),
    AlphaDot(0), AlphaPrev(0), AlphaPrimed(false),
    NzLimDot(0), NzLimPrev(0), NzLimPrimed(false) {}

void FBFlightControl::Reset(void) {
  GIterm = VsIterm = NyIterm = ThrIterm = NzPrev = 0;
  AlphaDot = AlphaPrev = 0;
  AlphaPrimed = false;
  NzLimDot = NzLimPrev = 0;
  NzLimPrimed = false;
}

FBFlightControl FBFlightControl::F16(void) {
  FBFlightControl c;
  c.Flcs = 1;
  c.RollStickMax = 0.15;   /* gemessen: nz bleibt 0,7..1,9 g; bei 0,35 waren es -1,1..+3,0 g */
  c.KRollRate = 0.05; c.KG = 0.25; c.KGi = 0.8;
  c.KpSpd = 0.02; c.ThrTrim = 0.85;
  return c;
}

/* DAS MiG-29-PRESET. Es unterscheidet sich vom F-16-Preset an genau EINER strukturellen Stelle, und
 * alles andere folgt daraus: die F-16 hat eine echte FLCS, dieser Ausgang ist dort ein g-WUNSCH an
 * einen schnellen inneren Kreis, der ihn ausregelt und dabei den Anstellwinkel selbst begrenzt. Die
 * MiG-29 ist mechanisch gesteuert (assets/aircraft/mig29/mig29.xml §7, doc/mig29/flight-controls.md) —
 * derselbe Ausgang IST der Ausschlag, hinter nichts als der ARU-Uebersetzung und deren 2,0/s
 * Stellratenbegrenzung. Zwischen Kommando und g liegen damit zwei Verzoegerungen, die es bei der F-16
 * nicht gibt, und die drei Unterschiede unten sind genau die Antwort darauf.
 *
 * WIE DIE ZAHLEN ENTSTANDEN SIND: gemessen, im Missions-Regelkreis, an missions/mig29-full.fbm. Die
 * drei gemessenen Fehlschlaege stehen hier, weil sie das Ergebnis erklaeren (CLAUDE.md: ein gemessener
 * Fehlschlag ist Wissen):
 *
 *   1. DER RUDDER-ZWEIG MUSS AUS (KNy = KNyi = 0). Mit den generischen 1,5/2,0 saettigte das
 *      Seitenruder in einem ~1,2-s-Zyklus und riss die Zelle ueber die Rollachse in ein Departure
 *      (t=28 s, LOC). Die Gains stammen aus dem Rest-ny der F-16; dieses Ruder (zwei Flossen, +-30 deg,
 *      gegen den Fall asymmetrischen Schubs ausgelegt) ist deutlich staerker. Der Stufe-1-Abnahmelauf
 *      hat die ganze Huellkurve dieser Zelle mit yaw = 0 vermessen, und genau das steht hier.
 *   2. ES DARF NUR EINEN INTEGRATOR GEBEN (KVsi = 0). Mit VS- UND g-Integrator hinter den zwei Lags
 *      lief der Anflug in einen Grenzzyklus von ~20 s Periode: Nicklage -6..+28 deg, VS -24..+15 m/s,
 *      CAS 127..163 kt, dazu ein Gashebel im Gegentakt — gemessen ueber die letzten 250 s von
 *      mig29-full. Weder ein kleineres KVs2g (0,015..0,035) noch mehr Nickdaempfung hat ihn beseitigt;
 *      das Streichen des VS-Integrators hat es. KGi bleibt bei 0,05 (6 % des F-16-Werts): genug, um
 *      einen stationaeren Bahnfehler ueber ein Bein wegzunehmen, zu wenig, um den Kreis zu fuehren.
 *   3. DER BEGRENZER IST PFLICHT (AlphaLimitDeg = 26). Ohne ihn beantwortete jeder Wegpunkt-Einrollvorgang
 *      den g-Sprung beim Wiedereinblenden des Nickzweigs mit vollem Stabilator und die Zelle tumbelte
 *      (Anstellwinkel-Spitzen 30..90 deg, LOC bei t=122 s). 26 deg ist die dokumentierte SOS-Grenze
 *      [T4, gestuetzt durch die 25-deg-Marke der Anzeige, DCS-EA p.19].
 *
 *   NICKDAEMPFUNG. KqDamp = 0,050 je deg/s. Der Stufe-1-Harness fliegt dieselbe Zelle mit 0,030..0,040
 *   (clients/FBTestMig29Envelope.cpp, alle drei Regler) — das ist die Groessenordnung, sie stammt von
 *   diesem Flugzeug, und der hoehere Wert ist gegen den Grenzzyklus oben gemessen. KG = 0,25 ist
 *   der F-16-Wert und traegt hier unveraendert: die Aufgabe des g-Zweigs ist dieselbe, nur was er
 *   antreibt, ist ein Ruder statt einer FLCS.
 *
 *   PitchStickMax = 0,60. Das Gegenstueck zu RollStickMax, und hier eine ECHTE Grenze: der Begrenzer
 *   haelt den Anstellwinkel, dieser Deckel haelt den Ausschlag. Gemessen wurde er von unten — 0,30
 *   liess die Zelle einen 400-m-Bahnfehler nicht mehr aufholen (sie flog mit gesaettigtem Kommando bei
 *   exakt 1,00 g ins Gelaende), 0,60 kann es.
 *
 *   ROLLEN. KRollRate = 0,030 und KpDampRoll = 0,010 sind der Wingslevel-Regler des Stufe-1-Harness,
 *   Gain fuer Gain — es ist dasselbe PD auf denselben Fehler. Sein +-0,5-Deckel ist NICHT uebernommen:
 *   der Harness hat nie eine Schraeglage KOMMANDIERT, sein Deckel wurde nie erreicht, und mit 0,5
 *   schaukelte sich die Wegpunkt-Kurve in eine Roll-PIO auf (+-0,5 Kommando, Schraeglage -15..+27 deg
 *   in 1 s). 0,15 sind bei 300 kt ~24 deg/s Rollrate ([MESS] GAP-ROLL: 157,8 deg/s bei vollem
 *   Ausschlag, linear in V) — eine Navigationsrolle, keine Kampfrolle.
 *
 *   SCHUB. ThrTrim = 0,5 = MILITAERSCHUB in der Konvention, die dieses Deck ausdruecklich mit dem
 *   F-16-Deck teilt (fcs/throttle-pos = 2 * cmd). Der Integrator hat damit von der Trimmstellung aus
 *   nach beiden Seiten denselben Weg — zum Leerlauf wie in den vollen Nachbrenner. Die 0,85 der F-16
 *   liegen tief im Nachbrenner und gehoeren zu deren Schub-Gewichts-Verhaeltnis, nicht zu diesem. */
FBFlightControl FBFlightControl::Mig29(void) {
  FBFlightControl c;
  c.Flcs = 1;
  c.RollStickMax = 0.15;
  c.KRollRate = 0.030; c.KpDampRoll = 0.010;
  c.PitchStickMax = 0.60;
  c.KG = 0.25; c.KGi = 0.050; c.KqDamp = 0.050;
  c.KNy = 0.0; c.KNyi = 0.0;
  c.KVs2g = 0.050; c.KVsi = 0.000;
  c.AlphaLimitDeg = 26.0;
  c.KpSpd = 0.02; c.ThrTrim = 0.5;
  return c;
}

/* DAS PRESET EINER ROHEN ZELLE OHNE EIGENE VERMESSUNG — der Satz, den doc/modules/air/
 * flight-model-recipe.md §6 bis zu dieser Runde NICHT genannt hat und ohne den jede Katalogzelle mit
 * den Gains der MiG-29 flog. Gemessen, was das kostet: air-bomber-intercept.fbm endete im Aufschlag,
 * 8 000 -> 510 m in 242 s bei gehaltener Fahrt und Leerlaufschub.
 *
 * ES IST DAS MiG-29-PRESET mit genau zwei ersetzten Zahlen und EINER Normierung:
 *
 *   1. AlphaLimitDeg ist der Grenzwinkel DER ZEILE. Die 26 deg der MiG-29 auf eine MiG-25 (12 deg) zu
 *      legen heisst, den Begrenzer 14 deg hinter dem Abriss zu parken — er begrenzt dann nichts.
 *   2. PitchStickMax ist die Nickautoritaet DER ZEILE, hergeleitet statt gesetzt: der Deckel, bei dem
 *      voller Stick das 1,5-fache des eigenen Grenzwinkels TRIMMT.  P = 1,5*|Cma|*a_lim/(|Cmde|*de_max).
 *      Damit hat der Begrenzer auf jeder Zeile dieselbe Reserve — 50 % — und keine Zeile kann sich mit
 *      dem Stick allein ins Tumble ziehen, was die MiG-29-Runde als die eine echte Gefahr der rohen
 *      Zelle vermessen hat.
 *   3. GLimitG ist das veroeffentlichte Lastvielfachen-Limit (A5) der Zeile, 0 wo keins veroeffentlicht
 *      ist. Erst damit ist §6s Satz "das g-Limit lebt in FBFlightControl" eingeloest.
 *   4. DIE NORMIERUNG: die drei g-Zweig-Gains skalieren mit P/0,60 (0,60 ist der gemessene Wert der
 *      MiG-29). Ein g-Fehler kauft damit auf jeder Zeile denselben BRUCHTEIL ihrer eigenen Autoritaet
 *      statt denselben absoluten Ausschlag — und weil P nach 2. selbst in Vielfachen des Grenzwinkels
 *      definiert ist, ist der geschlossene Kreis zeilenunabhaengig in der Groesse, die er begrenzt.
 *      Ohne sie regelt eine Zelle mit dreifacher Ruderwirkung mit dreifacher Kreisverstaerkung. */
FBFlightControl FBFlightControl::Raw(double pitchStickMax, double alphaLimitDeg, double gLimitG) {
  FBFlightControl c = Mig29();
  double p = pitchStickMax > 0.0 ? pitchStickMax : c.PitchStickMax;
  double scale = p / 0.60;
  c.PitchStickMax = p;
  c.AlphaLimitDeg = alphaLimitDeg;
  c.GLimitG = gLimitG;   /* 0 = die Zeile veroeffentlicht keins; dann begrenzt allein der Grenzwinkel */
  c.KG *= scale; c.KGi *= scale; c.KqDamp *= scale;
  return c;
}

/* DER g-BEGRENZER, EIN Gesetz fuer beide Zweige — dieselbe Form wie der Anstellwinkel-Zweig, mit der
 * ABLEITUNG DER BEGRENZTEN GROESSE als Vorhalt und nicht der Nickrate: ein stationaerer Zug traegt eine
 * von null verschiedene Nickrate, die dort als drohende Ueberschreitung gelesen wuerde. */
double FBFlightControl::NzLimit(const Fdm::fb_fdm_state &s) {
  if (NzLimPrimed) NzLimDot += kSosRateFilter * ((s.nz - NzLimPrev) / 0.01 - NzLimDot);
  NzLimPrev = s.nz;
  NzLimPrimed = true;
  return kGosKp * (GLimitG - s.nz - kGosLeadS * NzLimDot);
}

FBControls FBFlightControl::Run(const FBGuidance &g, const Fdm::fb_fdm_state &s) {
  FBControls o{};
  if (g.Mode == FBMode::Manual) {
    o.Roll = g.ManualRoll; o.Pitch = g.ManualPitch; o.Yaw = g.ManualYaw; o.Thr = g.ManualThr;
    /* DER BEGRENZER GILT AUCH AM HANDSTICK, und dass er das bis hierher nicht tat war ein echter Fehler
     * und keine Auslassung: er modelliert die STICK-KRAFT der Zelle (Klassenkommentar), und eine
     * Stick-Kraft weiss nicht, welchen Modus der Autopilot gerade fliegt. Sichtbar wurde es erst, als
     * ein Muster OHNE eigenen Begrenzer im Deck (die MiG-29, doc/mig29/flight-model-spec.md §7.3) die
     * erste Phase flog, die wirklich zieht: BFM kommandiert Manual, der Zweig oben reichte den Stick
     * ungefiltert durch, und die Zelle ging bei 32 deg Anstellwinkel und 11,8 g weg (gemessen,
     * missions/mig29-gun). Takeoff/Flare/Rollout sind die anderen Manual-Phasen und ziehen nie in die
     * Naehe des Grenzwinkels, weshalb es dort nie auffiel.
     * DAS GESETZ IST DASSELBE wie unten, Zeile fuer Zeile — es gibt nur einen Begrenzer. Fuer eine
     * Zelle mit eigener FLCS im Deck ist AlphaLimitDeg 0 (F16()), der Zweig also tot und der Ausdruck
     * bitgleich zu dem ohne ihn. */
    if (AlphaLimitDeg > 0.0) {
      if (AlphaPrimed) AlphaDot += kSosRateFilter * ((s.alphaDeg - AlphaPrev) / 0.01 - AlphaDot);
      AlphaPrev = s.alphaDeg;
      AlphaPrimed = true;
      /* AM HANDSTICK DARF ER DRUECKEN, ABER NUR BIS ZUR AUSSCHLAGGRENZE DER ZELLE. Der fruehere Deckel
       * `if (byAlpha < 0) byAlpha = 0` (nie Druecken) hatte einen echten Grund — eine Sprungstelle in der
       * abgetasteten Anstellwinkelrate beim Trimm-Einschwingen kam als Vollausschlag nach vorn an (-44,6
       * gemessen) —, aber er nahm dem Begrenzer die einzige RUECKHOLAUTORITAET, die eine Zelle ohne
       * eigene FLCS hat: schiesst der Anstellwinkel im Kampf ueber 26 deg (dynamisch, bei niedriger
       * Fahrt), reicht der -0,3-Druck des Piloten nicht, und der Anstellwinkel laeuft auf 150 deg in den
       * Mush (gemessen, mig29-bfm t=67..76). Der FLCS-Zweig unten liess den Begrenzer schon immer
       * druecken (kein 0-Deckel dort) — genau diese Zweiseitigkeit fehlte hier. Die Sprungstelle ist
       * heute anders abgefangen: AlphaPrimed haelt AlphaDot im ersten Tick auf 0 (byAlpha dann positiv),
       * und der Druck ist auf -PitchStickMax begrenzt, also im schlimmsten Fall der Ausschlag, den die
       * Zelle ohnehin fahren darf, nicht -44,6. */
      double byAlpha = kSosKp * (AlphaLimitDeg - s.alphaDeg - kSosLeadS * AlphaDot);
      if (byAlpha < -PitchStickMax) byAlpha = -PitchStickMax;
      if (byAlpha < o.Pitch) o.Pitch = byAlpha;
    }
    if (GLimitG > 0.0) {
      double byG = NzLimit(s);
      if (byG < -PitchStickMax) byG = -PitchStickMax;
      if (byG < o.Pitch) o.Pitch = byG;
    }
    /* PitchStickMax ist eine AUTORITAETSGRENZE der Zelle, kein Modus-Tuning wie RollStickMax (0,15 ist
     * eine Navigationsrolle, im Kampf falsch — die Rolle regelt der BFM-Rollratendeckel). Sie galt bis
     * hierher nur im FLCS-Zweig; am Handstick fiel sie still weg, und auf einem Deck OHNE eigene FLCS
     * (MiG-29) heisst 1,0 Nickstick 35 deg Stabilator = Tumble (doc/modules/mig29/module.md). Der
     * Begrenzer haelt den ANSTELLWINKEL, dieser Deckel den AUSSCHLAG — beide gehoeren an den Handstick
     * so wie an den Reglerausgang. Fuer die F-16 ist PitchStickMax 1,0 und der BFM-Nickstick liegt in
     * [-0,3;1,0], der Ausdruck also bitgleich. */
    o.Pitch = Clamp(o.Pitch, -PitchStickMax, PitchStickMax);
    return LastControls_ = o;
  }
  if (Flcs) {
    /* Vier Konstruktionsentscheidungen, jede gegen das nackte Modell gemessen: Kurven-g-Feedforward aus
     * der TATSAECHLICHEN Schraeglage, VS-Fehler-Integral, slew-limitiertes g-Kommando, und der g-Stick
     * erst EINGEBLENDET, wenn die Schraeglage steht. doc/systems.md, Abschnitt 3.3. */
    double bc = std::fmin(std::fabs(s.roll), 80.0) * (M_PI / 180.0);
    double targetVs = g.TargetVsMs;
    double vsErr = targetVs - s.vy;
    VsIterm = Clamp(VsIterm + KVsi * vsErr * 0.01, -0.5, 0.5);
    double nzRaw = 1.0 / std::cos(bc) + KVs2g * vsErr + VsIterm;
    if (NzPrev == 0.0) NzPrev = s.nz;
    double step = NzSlew * 0.01;
    double nzCmd = Clamp(nzRaw, NzPrev - step, NzPrev + step);
    NzPrev = nzCmd;
    double gErr = nzCmd - s.nz;
    double bankErr = std::fabs(g.BankCmdDeg - s.roll);
    double blend = Clamp(1.0 - (bankErr - 5.0) / 15.0, 0.0, 1.0);
    /* Die beiden Daempferglieder sind fuer eine Zelle mit eigener FLCS EXAKT 0 (F16()), der Ausdruck
     * ist dort also bitgleich zu dem ohne sie — nachgemessen ueber den vollen Missions-Sweep. */
    o.Roll = Clamp(KRollRate * (g.BankCmdDeg - s.roll) - KpDampRoll * s.p, -RollStickMax, RollStickMax);
    double gHold = GIterm;
    GIterm = Clamp(GIterm + blend * KGi * gErr * 0.01, -1.0, 1.0);
    double cmd = KG * gErr + GIterm - KqDamp * s.q;
    /* DER BEGRENZER, und er ist der KLEINERE VON ZWEI ZWEIGEN statt eines Deckels auf dem ersten: er
     * verbietet Ziehen, erzwingt aber nie Druecken. Sein Vorhalt ist die ABLEITUNG DER BEGRENZTEN
     * GROESSE und nicht die Nickrate — die ist im stationaeren Zug ungleich null und wuerde dort als
     * drohende Ueberschreitung gelesen, waehrend der Anstellwinkel selbst stillsteht. Gesetz, Gains und
     * dieser Fehlschlag sind auf DIESER Zelle gemessen: clients/FBTestMig29Envelope.cpp trug sie als
     * ausdrueckliche Wegwerf-Skizze, weil ihr Platz hier ist (flight-model-spec.md §7.3).
     * ANTI-WINDUP: fuehrt der Begrenzer, behaelt der Integrator seinen Stand von VOR diesem Schritt —
     * sonst kommandiert er nach dem Manoever genau den Zug nach, den der Begrenzer eben verboten hat. */
    if (AlphaLimitDeg > 0.0) {
      if (AlphaPrimed) AlphaDot += kSosRateFilter * ((s.alphaDeg - AlphaPrev) / 0.01 - AlphaDot);
      AlphaPrev = s.alphaDeg;
      AlphaPrimed = true;
      double byAlpha = kSosKp * (AlphaLimitDeg - s.alphaDeg - kSosLeadS * AlphaDot);
      if (byAlpha < cmd) {
        cmd = byAlpha;
        if (gErr > 0.0) GIterm = gHold;
      }
    }
    if (GLimitG > 0.0) {
      double byG = NzLimit(s);
      if (byG < cmd) {
        cmd = byG;
        if (gErr > 0.0) GIterm = gHold;
      }
    }
    o.Pitch = blend * Clamp(cmd, -PitchStickMax, PitchStickMax);
    /* Pedale nullen den Querlastfaktor; ein Rest-ny von ~-0,10 g bleibt modell-intrinsisch (Prinzip 5). */
    NyIterm = Clamp(NyIterm + KNyi * s.ny * 0.01, -0.6, 0.6);
    o.Yaw = Clamp(KNy * s.ny + NyIterm, -1.0, 1.0);
  } else {
    double pitchCmd = Clamp(KAltRaw * g.AltErrM, -PitchMaxDeg, PitchMaxDeg);
    o.Roll = Clamp(KpRoll * (g.BankCmdDeg - s.roll) - KdRoll * s.p, -1.0, 1.0);
    o.Pitch = -Clamp(KpPitch * (pitchCmd - s.pitch) - KdPitch * s.q, -1.0, 1.0);
    o.Yaw = Clamp(-KdYaw * s.r + KCoord * g.BankCmdDeg, -1.0, 1.0);
  }
  double spdErr = g.TargetSpeedMs - s.speed;
  ThrIterm = Clamp(ThrIterm + KTi * spdErr * 0.01, -ThrTrim, 1.0 - ThrTrim);   /* physischer Weg von der Trimmstellung zu beiden Anschlaegen */
  o.Thr = Clamp(ThrTrim + KpSpd * spdErr + ThrIterm, 0.0, 1.0);
  return LastControls_ = o;
}

void FBFlightControl::DeclareTelemetry(FBTelemetrySchema &schema) const {
  schema.Add("rollCmd");
  schema.Add("pitchCmd");
  schema.Add("yawCmd");
  schema.Add("throttleNorm");
}

void FBFlightControl::SampleTelemetry(FBTelemetryRow &row) const {
  row.Push(LastControls_.Roll);
  row.Push(LastControls_.Pitch);
  row.Push(LastControls_.Yaw);
  row.Push(LastControls_.Thr);
}

} // namespace FlightBox::Systems
