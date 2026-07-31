#include "FBPilot.h"
#include "FBGeodesy.h"
#include "FBLog.h"
#include <cmath>

namespace FlightBox::Pilot {

namespace {
const double kPreflightHoldS = 2.0;

/* Klein gehalten: die modelleigene steer-cmd-norm->Grad-Kennlinie ist bei Rollgeschwindigkeit selbst
 * sehr steil (~80 °/Einheit bei ~6 kt, f16.xml), ein sanftes Kommando ist also schon eine feste
 * Korrektur. */
const double kSteerXtGainPerM   = 0.01;
const double kSteerHdgGainPerDeg = 0.02;
const double kSteerCmdMax = 0.6;

/* Ein PD auf eine Ziel-Laengslage. Rotation, Flare und Aerobrake/Derotate sind DASSELBE Gesetz mit
 * anderem Ziel und anderer Autoritaetsgrenze. */
const double kRotateKp = 0.15, kRotateKd = 0.02, kRotateStickMax = 1.0;
const double kFlareStickMax = 0.6;       /* sanfter am Boden: der Flare ist eine weiche Korrektur */
const double kPositiveRateMs = 0.5;      /* echtes Steigen, kein Bodenrauschen */
/* JSBSims FGLGear friert WOW ein, sobald gear-pos-norm erstmals <= 0,99 faellt — eine Einfahrt mitten
 * im Aufsetzer wuerde ein veraltetes WOW=true einfrieren. Klein, weil das Modell im vollen Nachbrenner
 * sonst die 300-kt-Fahrwerksgrenze erreicht. */
const double kGearUpAglFt = 10.0;

/* BFM-Innenschleife. Diese Phase fliegt Manual-Stick wie Takeoff/Flare/Rollout, weil Direct/Course
 * NAVIGATIONS-Modi sind, deren Querlagendeckel und sanfter Rolleinsatz fuer einen Kampf strukturell
 * falsch waeren. Das Gesetz — EIN Auftriebsvektor, EIN Lastvielfaches — und alle Zahlen unten:
 * doc/pilot-ai.md, Abschnitt 5. */
const double kBfmRollFullDeg = 60.0;      /* Rollfehler, der vollen Querausschlag verdient */
const double kBfmTurnTimeS = 2.0;         /* in dieser Zeit will der Pilot den Lenkfehler weghaben */
/* Eine Rollrate, die der Pilot auch STOPPEN kann — und jetzt die geschlossene Form statt eines Knopfes:
 * der GROESSTE Lenkfehler, den dieses Gesetz erzeugen kann, ist 180° (es nimmt immer den kurzen Weg),
 * und auch der schlimmste Fall muss in der Zeitkonstante geflogen werden, der die Rolle dient. Damit ist
 * kBfmReverseS unten identisch kBfmTurnTimeS: eine Umkehr IST eine 180°-Rolle. Die frueheren 120 waren
 * der NOMINALWERT eines Deckels, der seinen Wert nie hielt (gemessen 91 °/s gegen 60 nominal, s.u.);
 * gegen den korrigierten Deckel neu vermessen — 16-Anflug-Sweep, Deckel 60/75/82,5/90/97,5/105 °/s:
 * 8/9/11/12/9/10 Treffer, und nur bei 90 °/s kein einziges Departure in den acht BFM-Missionen. */
const double kBfmRollRateMaxDegS = 180.0 / kBfmTurnTimeS;
/* DIESELBE 180° ALS DAUERSCHRANKE. Der Deckel darueber ist ein SPITZENWERT: er sagt, wie schnell eine
 * Umkehr geflogen werden darf, nicht wie lange. Gehalten wird er von einem Gesetz, dessen kommandierte
 * Auftriebsrichtung sich MITDREHEN kann — steht das Ziel quer und wandert die Sichtlinie schneller, als
 * die Zelle folgen kann, schliesst sich der Rollfehler nie und das Gesetz rollt weiter, Tick fuer Tick
 * mit vollem Ausschlag (gemessen, duel-merge: 290° Rolle in 3,1 s bei einem Zielfehler von 10-20°). Eine
 * Dauerrolle ist aber kein Manoever: nach 180° hat die Auftriebsachse jede Richtung einmal gezeigt, und
 * das Gesetz nimmt immer den kurzen Weg — mehr als 180° in EINE Richtung kann keine Korrektur
 * verlangen. Also gilt derselbe Satz ueber dasselbe Fenster: in kBfmTurnTimeS liegt nie mehr Rolle als
 * eine Umkehr. Der Deckel wird mit dem verbleibenden Anteil skaliert (Restbudget ueber das Fenster
 * verteilt), was bei leerem Fenster exakt der bisherige Spitzenwert ist und bei voll gehaltener Rolle
 * gegen den Fixpunkt p = Deckel/2 laeuft (p = cap(1 - p*T/180) und cap*T = 180). doc/pilot.md 5.7.3. */
const double kBfmRollExtentDeg = 180.0;
/* Die Strecke Stick -> Rollrate, IDENTIFIZIERT statt angenommen — der Deckel braucht sie, weil er den
 * Stick stellt und die RATE meint. ARX(1)-Fit p[n+1] = a*p[n] + K*(1-a)*u[n] ueber 15.325 Zehn-Hz-
 * Proben aus acht BFM-Laeufen, ausschliesslich unterhalb des Deckels (dort ist der Regler inaktiv, der
 * Fit also offen): a = 0,734 (Rollzeitkonstante 0,323 s), K = 78,7 °/s je Vollausschlag. Derselbe Fit
 * ueber ALLE Proben gibt 0,772/90,5 — die daraus folgende Deckelverstaerkung weicht um 7 % ab. */
/* Die F-16-Werte stehen jetzt als DEFAULTS der Hooks FBPilot::BfmRollPlantA/KDegS in FBPilot.h — die
 * Herleitung darueber gilt unveraendert und beschreibt, wie ein zweites Muster seine eigenen misst. */
const double kBfmGKp = 0.25, kBfmGKi = 0.5, kBfmGIMax = 0.6;
const double kBfmPushMax = 0.3;           /* ein Jaeger zieht und entlastet, er drueckt nicht */
const double kBfmSearchRangeM = 5556.0;   /* 3 nm: nur damit die kalte Suche einen Punkt hat */
/* Die Ratenschaetzung ist eine Differenz EINES publizierten Floats ueber zwei 10-Hz-Ticks, also
 * gefiltert — schnell genug fuer eine Umkehr, langsam genug, dass ein Look nicht die Nase schwenkt. */
const double kBfmLeadRateAlpha = 0.4;
/* Ki ist HERGELEITET, keine Geschmacksfrage: das Gesetz kommandiert (e+I)/T gegen einen Winkel, der
 * Kreis schliesst als s² + s/T + Ki/T = 0, also Ki = 1/(2T) → zeta = 0,707. */
const double kBfmTrackKi = 0.5 / kBfmTurnTimeS, kBfmTrackIMaxDeg = 10.0;
/* Breiter und das Weben ist kein Suchmuster mehr, sondern eine Umkehr. */
const double kBfmScanMaxAmpDeg = 45.0;
/* Eine Suche steigt fest und sinkt sanft: HOCH ziehen ist bei jeder Lage ein aufrechter Zug, ein steiler
 * ABWAERTS-Wunsch laesst das Auftriebsvektor-Gesetz invertiert rollen. */
const double kBfmSearchUpMaxDeg = 20.0, kBfmSearchDownMaxDeg = 5.0;
const double kBfmFloorPullDeg = 30.0;     /* volle Nase-hoch-Vorspannung am AGL-Boden */
/* DIE FLUEGELLINIE, eine Grenze und keine Schwelle: innerhalb ist der kurze Weg eindeutig, ausserhalb
 * ist die Nase herumzubekommen eine festgelegte Kurve, die der Pilot nicht Tick fuer Tick umentscheiden
 * darf. Die Zonenbreite folgt daraus, dass eine Umkehr kBfmReverseS dauert und die Peilung derweil
 * weiterwandert — sie oeffnet sich auf einer schnellen Passage und schliesst sich in einer Schere. */
const double kBfmConvertErrDeg = 90.0;
const double kBfmReverseS = 180.0 / kBfmRollRateMaxDegS;
const double kBfmG0 = 9.80665;
const double kBfmClosureDeadKt = 40.0;    /* Totband auf den Closure-Fahrplan */
const double kBfmOverspeedFrac = 1.15;    /* darueber kaufen die zusaetzlichen Knoten keine Kurvenrate */
const double kBfmThrTrim = 0.6, kBfmThrKpPerKt = 0.006;   /* ±67 kt Fehler = Leerlauf..voll */
const double kBfmSpeedbrakeKt = 40.0;     /* Bremsklappe nur, wenn der Gashebel allein nicht reicht */

/* INTERCEPT. Die Zahlen unten sind Eigenschaften des PILOTEN und der Geometrie, nicht der Zelle —
 * deshalb Konstanten statt virtueller Hooks: Kardanwinkel und Startbereiche unterscheiden zwei Jets,
 * eine menschliche Reaktionszeit tut es nicht. Beide Zeiten sind ueber die Variantentabelle
 * ueberschreibbar und liegen immer OBEN AUF der Bus-Latenz. doc/pilot-ai.md, Abschnitt 11. */
const double kInterceptReactionS = 1.0;   /* wahrnehmen, erkennen, entscheiden, bewegen */
const double kInterceptActionS = 0.5;     /* dasselbe fuer die HAENDE: ein Hebel nach dem anderen */
/* Totband der Antennenhoehe: ein Suchmuster ist ±10,5° hoch, 2° liegen gut innerhalb der Keule. */
const double kInterceptElDeadDeg = 2.0;
/* Zwei CRM-Frames plus Marge: ein ausgelassener Sweep ist einer, drei sind ein Ziel, das weg ist. */
const double kInterceptLostS = 10.0;
/* Die Direct-Guidance will einen PUNKT; so weit weg, dass die Peilung dorthin der geforderte Kurs ist. */
const double kInterceptAimM = 60.0 * kNmToM;
/* Deckel fuer einen Startbereich, der nie einen Countdown produziert hat. */
const double kInterceptSupportMaxS = 60.0;
/* Ein frischer Lock ist keine Feuerloesung: die Runde wird mit der GESCHAETZTEN Zielbewegung
 * programmiert, und die ist gefiltert (~0,4 s Zeitkonstante). Innerhalb einer Sekunde zu schiessen
 * startet eine Runde mit Zielgeschwindigkeit nahe null. */
const double kInterceptTrackSettleS = 2.0;

double Clamp(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }

/* Aus einem Kurswunsch den Punkt machen, den die Direct-Guidance braucht. */
void AimAlongHeading(const Fdm::fb_fdm_state &st, double hdgDeg, double &latDeg, double &lonDeg) {
  double h = hdgDeg * kDeg2Rad;
  double coslat = std::cos(st.lat * kDeg2Rad);
  latDeg = st.lat + kInterceptAimM * std::cos(h) / kMPerDeg;
  lonDeg = st.lon + kInterceptAimM * std::sin(h) / (kMPerDeg * (std::fabs(coslat) > 1e-6 ? coslat : 1e-6));
}
} // namespace

const char *FBPilot::PhaseName(Phase p) {
  switch (p) {
    case Phase::Idle: return "Idle";
    case Phase::Preflight: return "Preflight";
    case Phase::Takeoff: return "Takeoff";
    case Phase::Climb: return "Climb";
    case Phase::Route: return "Route";
    case Phase::Approach: return "Approach";
    case Phase::Flare: return "Flare";
    case Phase::Rollout: return "Rollout";
    case Phase::Shutdown: return "Shutdown";
    case Phase::Bfm: return "Bfm";
    case Phase::Intercept: return "Intercept";
    case Phase::Attack: return "Attack";
    case Phase::Formation: return "Formation";
    case Phase::Orbit: return "Orbit";
    case Phase::Drag: return "Drag";
  }
  return "?";
}

/* Ein Bein ist die Bahn zwischen zwei DEKLARIERTEN Fixen; der erste Wegpunkt hat keine und wird als
 * Peilung geflogen. */
void FBPilot::SetLegFromPlan(FBPilotCommands &c, const FBFlightPlan &plan) {
  int idx = plan.ActiveIndex();
  if (idx <= 0 || idx >= plan.Size()) return;
  const FBWaypoint &from = plan.At(idx - 1);
  c.HaveLeg = true;
  c.LegLatDeg = from.LatDeg; c.LegLonDeg = from.LonDeg;
}

/* along = 0 an der Schwelle, + in Runway-Richtung; + across = rechts der Achse — DIESELBE Konvention
 * wie FBMissionMonitor::OnRunway und FBAutopilot::SetCourse. */
void FBPilot::RunwayAxis(const FBRunway &rwy, double lat, double lon, double &alongM, double &acrossM) {
  FBTrackProjectM(rwy.ThresholdLatDeg, rwy.ThresholdLonDeg, rwy.TrueHeadingDeg, lat, lon, alongM, acrossM);
}

double FBPilot::NosewheelSteerCmd(const FBRunway &rwy, double lat, double lon, double yawDeg) const {
  double along, across;
  RunwayAxis(rwy, lat, lon, along, across);
  (void)along;
  double hdgErr = FBWrap180(yawDeg - rwy.TrueHeadingDeg);
  return Clamp(-(kSteerXtGainPerM * across + kSteerHdgGainPerDeg * hdgErr), -kSteerCmdMax, kSteerCmdMax);
}

double FBPilot::PitchHoldStick(double targetDeg, double pitchDeg, double qDegS, double stickMax) const {
  return Clamp(kRotateKp * (targetDeg - pitchDeg) - kRotateKd * qDegS, -stickMax, stickMax);
}

/* n und V sind Hooks dieses Piloten — ein Modul mit einem anderen Jet bekommt die andere Annahme
 * kostenlos. */
double FBPilot::CornerTurnRateDegS() const {
  double v = std::fmax(BfmCornerSpeedKt() * kKtToMs, 1.0);
  double n = std::fmax(BfmCornerG(), 1.001);
  return kBfmG0 * std::sqrt(n * n - 1.0) / v * kRad2Deg;
}

/* Zwei Muster: OHNE Datum gibt es nichts zu zentrieren (Amplitude/Periode der Zelle, Phase auf der
 * Missionsuhr); MIT Datum ist die BREITE, was der Pilot nicht weiss (dessen Halbwinkel), die PERIODE
 * waechst mit, damit die Kursrate sanft bleibt, und die PHASE haengt am SUCHBEGINN, damit der Sweep auf
 * der wahrscheinlichsten Peilung beginnt. doc/pilot-ai.md, Abschnitt 5.4. */
double FBPilot::SearchWeaveDeg(const FBTrackDatum &datum, bool searching) {
  double base = std::fmax(BfmScanAmplitudeDeg(), 1e-3);
  if (!searching || !datum.Valid) {
    ScanRunning_ = false;
    return searching ? base * std::sin(2.0 * kPi * TimeS_ / BfmScanPeriodS()) : 0.0;
  }
  if (!ScanRunning_) { ScanSinceS_ = TimeS_; ScanRunning_ = true; }
  double amp = Clamp(datum.HalfWidthDeg, base, kBfmScanMaxAmpDeg);
  double period = std::fmax(BfmScanPeriodS() * amp / base, 1e-3);
  return amp * std::sin(2.0 * kPi * (TimeS_ - ScanSinceS_) / period);
}

/* Die Rolle der letzten kBfmTurnTimeS, aus der gemessenen Rate integriert. Der Ring haelt den KUMULIERTEN
 * Stand mit Zeitstempel, die Fensterlaenge ist damit eine Zeit und kein Index — der Entscheidungstakt
 * darf sich aendern. Ist die Vorgeschichte kuerzer als das Fenster, zaehlt nur, was da ist (beim ersten
 * Tick also 0, und der Deckel ist dort unveraendert der Spitzenwert). doc/pilot.md 5.7.3. */
double FBPilot::BfmRollWindowDeg(double pDegS, double dt) {
  BfmRollCumDeg_ += pDegS * dt;
  BfmRollHistDeg_[BfmRollHistHead_] = BfmRollCumDeg_;
  BfmRollHistS_[BfmRollHistHead_] = TimeS_;
  BfmRollHistHead_ = (BfmRollHistHead_ + 1) % kBfmRollHistN;
  if (BfmRollHistCount_ < kBfmRollHistN) ++BfmRollHistCount_;

  const double cutoff = TimeS_ - kBfmTurnTimeS;
  double base = BfmRollCumDeg_;
  for (int k = 0; k < BfmRollHistCount_; ++k) {
    int i = (BfmRollHistHead_ - 1 - k + 2 * kBfmRollHistN) % kBfmRollHistN;
    base = BfmRollHistDeg_[i];
    if (BfmRollHistS_[i] <= cutoff) break;
  }
  return BfmRollCumDeg_ - base;
}

/* EIN Entscheidungstakt des Kampfes: Bild lesen -> Verfolgungsart waehlen -> Zielpunkt bilden -> mit der
 * vorhandenen Energie hinfliegen. Das Bild ist ein RADAR-TRACK und sonst nichts. */
FBPilotCommands FBPilot::BfmCommands(const FBState &state, FBCommandBus &avionics,
                                    const Fdm::fb_fdm_state &st, double dt) {
  Bfm_.Update(state, st, TimeS_);
  const FBBfmBlock &g = Bfm_.Block();
  /* Der Kopf beantwortet die zwei Fragen, auf die sich das ganze Gesetz verzweigt: gibt es ueberhaupt
   * etwas (Readable) und ist es jung genug, darauf VORZUHALTEN (IsValid). */
  const bool haveTrack = g.H.Readable(), validTrack = g.H.IsValid();
  const double trackAgeS = haveTrack ? TimeS_ - g.H.StampS : 0.0;
  /* Unbedingt gerechnet, obwohl nur die Suche es liest: so bleibt die Referenz der Suche dasselbe
   * Objekt, statt erst aufzutauchen, wenn die Verfolgung aufgibt. */
  const FBTrackDatum datum = Bfm_.Datum(st, TimeS_, CornerTurnRateDegS());
  FBPilotCommands c{};
  c.Guidance = FBPilotGuidance::Manual;
  c.ManualYaw = 0.0;

  double casKt = st.cas * kMsToKt;
  double rngNm = g.RangeM * kMToNm;
  double closKt = g.ClosureMs * kMsToKt;
  double tgtSpeedMs = std::sqrt(g.VelE * g.VelE + g.VelN * g.VelN + g.VelU * g.VelU);

  /* „Ohne Energie" ist RELATIV: ein Verfolger in der Kontrollposition hinter einem hart verzoegernden
   * Verteidiger IST unter seinem Eckband, und das mit vollem Nachbrenner zu korrigieren wirft ihn nach
   * vorn heraus (gemessen: die absolute Regel kostete 250 von 268 s Kontrollposition). */
  bool lowEnergy = casKt < BfmMinSpeedKt() && (!validTrack || tgtSpeedMs > st.speed);

  /* ---- 1. welche Verfolgungsart verlangt diese Geometrie? (doc/pilot-ai.md 5.2) ----
   * Die KONTROLLPOSITION laeuft ueber die Variantentabelle statt direkt ueber die Hooks: sie ist die
   * eine BFM-Zahl, die eine Mission wirklich aendern muss (eine Raketen-Halteposition liegt AUSSERHALB
   * des Kanonentrichters). */
  const double ctrlMinNm = Tuned(FBPilotParam::BfmCtrlMinNm, BfmControlMinNm());
  const double ctrlMaxNm = Tuned(FBPilotParam::BfmCtrlMaxNm, BfmControlMaxNm());
  double ctrlMidNm = 0.5 * (ctrlMinNm + ctrlMaxNm);
  /* Und der Ueberschuss, den er aufbauen darf, ist der, den er auch wieder abbaut: der Fahrplan
   * c = k·(R − Rctrl) verlangt eine Verzoegerung k·c, ist bei endlicher Bremsautoritaet a also nur bis
   * c = a/k befolgbar. Darueber schreibt der Pilot einen Scheck — gemessen: mit dem alten 200-kt-Deckel
   * erreichte er das Kontrollband mit 98 kt Ueberschuss gegen einen Fahrplan, der 5 verlangte. */
  double schedSlopePerS = BfmClosureGainKtPerNm() * kKtToMs / kNmToM;
  double capKt = std::fmin(BfmMaxClosureKt(), BfmBrakeMs2() / schedSlopePerS * kMsToKt);
  double schedKt = Clamp((rngNm - ctrlMidNm) * BfmClosureGainKtPerNm(), -capKt, capKt);
  bool overtaking = validTrack && closKt > schedKt + kBfmClosureDeadKt;

  FBBfmPursuit mode;
  if (!validTrack) mode = FBBfmPursuit::Search;
  else if (rngNm < ctrlMinNm || overtaking) mode = FBBfmPursuit::Lag;
  else if (g.AspectDeg > BfmLeadAspectDeg() || rngNm > BfmLeadRangeNm()) mode = FBBfmPursuit::Lead;
  else mode = FBBfmPursuit::Pure;

  /* ---- 2. der Zielpunkt, als Versatz zur eigenen Position ---- */
  double aimE = g.EastM, aimN = g.NorthM, aimU = g.UpM;
  if (mode == FBBfmPursuit::Lead) {
    /* Kollisionsvorhalt als ZEIT, nicht als fester Winkel: der Vorhalt schrumpft mit der Entfernung und
     * verlangt nie eine Kurve in leeren Himmel. */
    double tLead = Clamp(g.RangeM / std::fmax(st.speed, 1.0), 0.0, BfmLeadMaxS());
    aimE += g.VelE * tLead; aimN += g.VelN * tLead; aimU += g.VelU * tLead;
  } else if (mode == FBBfmPursuit::Lag) {
    /* Lag sind ZWEI Verschiebungen: HINTER ihm stoppt die Nase davor, nach vorn zu schiessen, UEBER ihm
     * baut den Ueberschuss tatsaechlich ab (High Yo-Yo — Geschwindigkeit in Hoehe statt in Widerstand,
     * den der Jet nicht hat). Die Hoehe skaliert mit dem Ueberschuss, wickelt sich also selbst ab. */
    double excess = Clamp((closKt - schedKt) / std::fmax(BfmMaxClosureKt(), 1.0), 0.0, 1.0);
    aimE -= g.VelE * BfmLagTimeS(); aimN -= g.VelN * BfmLagTimeS(); aimU -= g.VelU * BfmLagTimeS();
    aimU += BfmYoYoHeightM() * excess;
  } else if (mode == FBBfmPursuit::Search) {
    /* Die SUCHE fliegt eine RICHTUNG und eine HOEHE, nie einen Punkt, und sie fliegt das DATUM statt
     * der eingefrorenen letzten Messposition. Ohne je etwas gesehenes wird die kalte Suche VERANKERT —
     * auf „wohin meine Nase gerade zeigt" zu zielen ist ein Regelkreis ohne Referenz und endet in einem
     * 80°-Schraeglagen-Orbit. doc/pilot-ai.md, Abschnitt 5.4. */
    double brgDeg;
    /* ...solange es noch ein ORT ist: INNERHALB des Gebiets ist die Peilung zu seinem Mittelpunkt keine
     * Information mehr, und auf einen Punkt zu steuern, auf dem man sitzt, macht aus der Suche einen
     * Orbit. Dann ist die ehrliche Suche die kalte. */
    if (datum.Valid && datum.RangeM > datum.RadiusM) {
      brgDeg = datum.BearingDeg;
      aimU = datum.UpM;
    } else {
      if (!BfmSearchAnchored_ && st.speed > 1.0) {
        BfmSearchHdgDeg_ = st.yaw;
        BfmSearchAltM_ = st.elev;
        BfmSearchAnchored_ = true;
      }
      brgDeg = BfmSearchAnchored_ ? BfmSearchHdgDeg_ : st.yaw;
      aimU = BfmSearchAnchored_ ? BfmSearchAltM_ - st.elev : 0.0;
    }
    double brg = brgDeg * kDeg2Rad;
    aimE = kBfmSearchRangeM * std::sin(brg);
    aimN = kBfmSearchRangeM * std::cos(brg);
    /* AUFRECHT geflogen: ein steiler Abwaertswunsch laesst das Auftriebsvektor-Gesetz invertiert rollen
     * (ein Split-S), und ein rollender Jet sucht nichts. */
    aimU = Clamp(aimU, -std::tan(kBfmSearchDownMaxDeg * kDeg2Rad) * kBfmSearchRangeM,
                 std::tan(kBfmSearchUpMaxDeg * kDeg2Rad) * kBfmSearchRangeM);
  }

  if (haveTrack) BfmSearchAnchored_ = false;   /* es gibt wieder ein Datum: die naechste kalte Suche verankert neu */

  /* Der Zielpunkt wird IN DER WELT um die Vertikale rotiert (statt Grad auf den Koerper-Azimut zu
   * addieren), damit der Lenkfehler bei jeder Querlage eine kohaerente Richtung bleibt. */
  double w = SearchWeaveDeg(datum, !validTrack || trackAgeS > BfmScanAfterS()) * kDeg2Rad;
  double cw = std::cos(w), sw = std::sin(w);
  double weaveE = aimE * cw + aimN * sw, weaveN = -aimE * sw + aimN * cw;
  aimE = weaveE; aimN = weaveN;

  /* Energie im ZIELPUNKT statt als eigener Modus, und bewusst ein DECKEL statt einer Nase-runter-
   * Vorspannung: ein negativer Hoehenwunsch rollte den Auftriebsvektor INVERTIERT (ein Split-S) —
   * gemessen 2.900 m Hoehenverlust, waehrend der Pilot nur 50 kt zurueckwollte. */
  if (lowEnergy && aimU > 0.0) aimU = 0.0;

  double azErr = 0.0, elErr = 0.0;
  FBEnuToBodyLos(st.roll, st.pitch, st.yaw, aimE, aimN, aimU, azErr, elErr);

  /* ---- 3b. IM TRICHTER wird der TRICHTER geflogen ----
   * Verfolgungssteuerung zielt auf eine POSITION; ein Geschuetz muss woandershin zeigen. Der Lenkfehler
   * wird deshalb der vom Feuerleitrechner publizierte VORHALTE-Fehler — dieselbe Loesung, die auch das
   * Abzugstor liest, also koennen Zielen und Schiessen nicht auseinandergehen.
   * Die letzten zwei Bedingungen verhindern, dass dies ein SCHLECHTERES Verfolgungsgesetz wird: eine
   * Kanonenloesung existiert fuer ein Ziel UEBERALL, auch fuer eines 170° neben der Nase, und die als
   * Lenkfehler zu uebergeben flog den Jet in 158 s in den Boden. */
  const FBFireControlBlock &fc = state.FireControl;
  bool gunTrack = state.Gun.H.Readable() && state.Gun.Ready && fc.H.Readable() && fc.GunValid &&
                  fc.GunSpanMr >= fc.GunFunnelBottomMr &&
                  std::fabs(g.AzDeg) <= BfmControlAtaDeg() &&
                  fc.GunAimErrorDeg <= BfmGunTrackMaxErrDeg();
  if (gunTrack) {
    /* ---- 3c. Die BEWEGUNG der Loesung ist ein eigener Regelanteil ----
     * Gegen ein kurvendes Ziel ist die geforderte Rohrrichtung eine RAMPE, und ein Kreis mit reinem
     * P-Anteil bleibt konstant um (Rampenrate × Zeitkonstante) zurueck (gemessen: nie unter 4,6° bei ~1°
     * Trichtertoleranz). Die Rate wird dort genommen, wo die eigene Lage GAR NICHT vorkommt: geforderte
     * Rohrrichtung in die WELT drehen, dort differenzieren, zurueck in den Koerper — nichts von der
     * eigenen Rolle/Nick/Gier ueberlebt den Rundweg, und das ist der Punkt (koerperbezogen differenziert
     * maesse man den eigenen Zug). doc/pilot-ai.md, Abschnitt 5.6. */
    double az = fc.GunLeadAzDeg, el = fc.GunLeadElDeg;
    double be, bn, bu;
    FBBodyLosToEnu(st.roll, st.pitch, st.yaw, az, el, be, bn, bu);
    if (GunHaveLead_ && TimeS_ > GunLeadPrevS_) {
      double dtl = TimeS_ - GunLeadPrevS_;
      GunLeadRateE_ += kBfmLeadRateAlpha * ((be - GunLeadPrevE_) / dtl - GunLeadRateE_);
      GunLeadRateN_ += kBfmLeadRateAlpha * ((bn - GunLeadPrevN_) / dtl - GunLeadRateN_);
      GunLeadRateU_ += kBfmLeadRateAlpha * ((bu - GunLeadPrevU_) / dtl - GunLeadRateU_);
    }
    GunLeadPrevE_ = be; GunLeadPrevN_ = bn; GunLeadPrevU_ = bu;
    GunLeadPrevS_ = TimeS_; GunHaveLead_ = true;

    /* Gekappt bei der Kurvenrate der Zelle: eine schneller wandernde Loesung ist kein Nachfuehrproblem
     * mehr. Die Rate einer Einheitsrichtung IST ihre Winkelrate, der Deckel also ein Skalar. */
    double rate = std::sqrt(GunLeadRateE_ * GunLeadRateE_ + GunLeadRateN_ * GunLeadRateN_ +
                            GunLeadRateU_ * GunLeadRateU_);
    double rateMax = CornerTurnRateDegS() * kDeg2Rad;
    double k = kBfmTurnTimeS * (rate > rateMax ? rateMax / std::fmax(rate, 1e-9) : 1.0);
    FBEnuToBodyLos(st.roll, st.pitch, st.yaw, be + k * GunLeadRateE_, bn + k * GunLeadRateN_,
                   bu + k * GunLeadRateU_, azErr, elErr);
    GunTrackIAz_ = Clamp(GunTrackIAz_ + kBfmTrackKi * az * dt, -kBfmTrackIMaxDeg, kBfmTrackIMaxDeg);
    GunTrackIEl_ = Clamp(GunTrackIEl_ + kBfmTrackKi * el * dt, -kBfmTrackIMaxDeg, kBfmTrackIMaxDeg);
    azErr += GunTrackIAz_;
    elErr += GunTrackIEl_;
    /* Und es darf nie mehr fordern als das Tor, das es hereingelassen hat: die drei Anteile sind alle
     * WINKEL und koennen sich weit ueber den Zielfehler hinaus addieren (gemessen: 60° auf kuerzeste
     * Entfernung, voller Ausschlag in beiden Achsen, Departure). */
    double mag = std::sqrt(azErr * azErr + elErr * elErr);
    double lim = BfmGunTrackMaxErrDeg();
    if (mag > lim) { azErr *= lim / mag; elErr *= lim / mag; }
    mode = FBBfmPursuit::Lead;   /* es IST Vorhalteverfolgung, das Scoreboard soll das sagen */
  } else {
    GunHaveLead_ = false;
    GunLeadRateE_ = GunLeadRateN_ = GunLeadRateU_ = 0.0;
    GunTrackIAz_ = GunTrackIEl_ = 0.0;
  }
  if (gunTrack != GunTracking_) {
    GunTracking_ = gunTrack;
    FBLog::Info("pilot", gunTrack ? "GUN_TRACK" : "GUN_BREAK",
                {{"rangeM", fc.GunRangeM}, {"tofS", fc.GunTofS}, {"aimErrDeg", fc.GunAimErrorDeg},
                 {"leadAzDeg", fc.GunLeadAzDeg}, {"leadElDeg", fc.GunLeadElDeg},
                 {"spanMr", fc.GunSpanMr}, {"bottomMr", fc.GunFunnelBottomMr},
                 {"rounds", state.Gun.RoundsRemaining}});
  }

  /* Der Bodendeckel steht UEBER allem darueber: ein in den Boden geflogener Kampf ist kein gewonnener. */
  const FBRadarAltBlock &ra = state.RadarAlt;
  if (BfmFloorFt() > 0.0 && ra.H.Readable() && ra.AglFt < BfmFloorFt())
    elErr += kBfmFloorPullDeg * Clamp(1.0 - ra.AglFt / BfmFloorFt(), 0.0, 1.0);

  /* ---- 4. hinfliegen: EIN Auftriebsvektor, EIN Lastvielfaches (doc/pilot-ai.md 5.1) ---- */
  double errMag = std::sqrt(azErr * azErr + elErr * elErr);
  double vRatio = casKt / std::fmax(BfmCornerSpeedKt(), 1.0);
  double gAvail = Clamp(BfmCornerG() * vRatio * vRatio, 1.0, BfmMaxG());
  double aTurn = std::fmin(st.speed * (errMag * kDeg2Rad) / kBfmTurnTimeS, gAvail * kBfmG0);
  double dirRight = errMag > 1e-6 ? azErr / errMag : 0.0;
  double dirUp = errMag > 1e-6 ? elErr / errMag : 1.0;
  double gravRight = -kBfmG0 * std::sin(st.roll * kDeg2Rad) * std::cos(st.pitch * kDeg2Rad);
  double gravUp = kBfmG0 * std::cos(st.roll * kDeg2Rad) * std::cos(st.pitch * kDeg2Rad);
  double liftRight = aTurn * dirRight + gravRight;
  double liftUp = aTurn * dirUp + gravUp;

  double phiCmd = std::atan2(liftRight, liftUp) * kRad2Deg;
  double rollCmd = Clamp(phiCmd / kBfmRollFullDeg, -1.0, 1.0);

  /* ---- DIE KONVERSION: ein Peilungsgesetz ist bei 180° undefiniert, also wird der Drehsinn FESTGELEGT.
   * Kreuzt das Ziel das Heck, kippt die kommandierte Auftriebsrichtung das Vorzeichen und das Gesetz
   * sieht keinen wachsenden Fehler, sondern einen NEUEN auf der anderen Seite — es antwortet mit einer
   * Umkehr. Zwei Eintrittsbedingungen, beide Geometrie: das Ziel hinter der FLUEGELLINIE, UND die
   * Sichtlinie dreht schneller, als die Zelle kurven kann (dann ist die Peilung kein verfolgbares Signal
   * mehr). GEKAPPT statt gespiegelt, damit es begrenzt bleibt: das Schlimmste, was es fordern kann, ist
   * „halte die Querlage und zieh weiter". Der Drehsinn braucht keine eigene Regel — es ist der, in den
   * der Pilot beim Kreuzen ohnehin schon drehte. doc/pilot-ai.md, Abschnitt 5. */
  double losRateDegS = 0.0;
  if (haveTrack) {
    double rr = std::fmax(g.RangeM, 1.0);
    double le = g.EastM / rr, ln = g.NorthM / rr, lu = g.UpM / rr;
    double re = g.VelE - st.vx, rn = g.VelN + st.vz, ru = g.VelU - st.vy;
    double along = re * le + rn * ln + ru * lu;
    double ce = re - along * le, cn = rn - along * ln, cu = ru - along * lu;
    losRateDegS = std::sqrt(ce * ce + cn * cn + cu * cu) / rr * kRad2Deg;
  }
  double convertZoneDeg = Clamp(180.0 - losRateDegS * kBfmReverseS, kBfmConvertErrDeg, 180.0);
  if (errMag > convertZoneDeg) {
    if (BfmTurnSense_ == 0) BfmTurnSense_ = rollCmd >= 0.0 ? 1 : -1;
    if (rollCmd * BfmTurnSense_ < 0.0) rollCmd = 0.0;
  } else {
    BfmTurnSense_ = 0;
  }

  /* DER ROLLRATEN-DECKEL, als STRECKENINVERSION statt als Rekursion: gesucht ist der Stick, der die
   * NAECHSTE Rate genau auf den Deckel legt — p[n+1] = a*p[n] + K*(1-a)*u[n], nach u aufgeloest. Er
   * haengt nur von der GEMESSENEN Rate ab, ist also gedaechtnislos und hat einen Fixpunkt per
   * Konstruktion. Die alte Form skalierte das VORIGE Kommando und war damit kein Deckel, sondern ein
   * schwach gedaempfter Oszillator (z^2 - 2az + a = 0, also |z| = sqrt(a) = 0,86): gemessen hielt sie
   * 91 °/s gegen 60 °/s Deckel, mit einer Wiederkehr bei 0,70 s. REDUZIERT weiter nur — nie
   * Gegenruder, nie mehr als das rohe Kommando.
   * doc/pilot.md, Abschnitt 5.7. Der Deckel selbst ist seit dieser Runde kein fester Wert mehr, sondern
   * der ANTEIL einer Umkehr, der im Fenster kBfmTurnTimeS noch offen ist (kBfmRollExtentDeg, 5.7.3) —
   * bei leerem Fenster unveraendert der Spitzenwert, bei gehaltener Dauerrolle dessen Haelfte. */
  double rollWinDeg = BfmRollWindowDeg(st.p, dt);
  double rateCap = BfmRollRateMaxDegS();
  if (rollWinDeg * rollCmd > 0.0)
    rateCap *= Clamp(1.0 - std::fabs(rollWinDeg) / kBfmRollExtentDeg, 0.0, 1.0);
  if (st.p * rollCmd > 0.0) {
    double plantA = BfmRollPlantA(), plantK = BfmRollPlantKDegS();
    double lim = Clamp((rateCap - plantA * std::fabs(st.p)) / (plantK * (1.0 - plantA)), 0.0, 1.0);
    if (std::fabs(rollCmd) > lim) rollCmd = rollCmd > 0.0 ? lim : -lim;
  }
  /* IM SUCHLAUF IST DIE ROLLE EIN SCAN, KEIN KAMPFZUG. Das aim ist eine Vermutung (Heading/Datum);
   * fliegt eine haerter rollende Zelle (MiG-29, K=201) sie mit voller Rollautoritaet, treibt die eigene
   * Lagedrift die koerperfesten Lenkfehler in einen Roll-Grenzzyklus — die Nase kommt nie auf die
   * Antenne, es gibt nie einen Kontakt, und der Monitor sieht die Dauer-Rollrate als Departure
   * (gemessen, mig29-bfm/duel-merge). Nur die ROLLE wird gedeckelt: der g-/Nickzweig haelt die Hoehe
   * weiter ueber den vollen Auftriebsvektor, sonst driftet der Scan in Pitch weg. Der Deckel ist ein
   * HOOK mit F-16-Default 1,0 (bitgleich, bfm-blind), die MiG setzt ihn tief. Nach dem Lock (jede andere
   * Verfolgungsart) faellt er weg — der Kampfzug rollt voll. doc/pilot.md 5.4. */
  if (mode == FBBfmPursuit::Search) {
    double rc = BfmSearchRollCap();
    rollCmd = Clamp(rollCmd, -rc, rc);
  }
  c.ManualRoll = rollCmd;

  double gCmd = Clamp(std::sqrt(liftRight * liftRight + liftUp * liftUp) / kBfmG0, 0.0, gAvail);
  if (lowEnergy) gCmd = std::fmin(gCmd, BfmUnloadG());

  double gErr = gCmd - st.nz;
  BfmGIterm_ = Clamp(BfmGIterm_ + kBfmGKi * gErr * dt, -kBfmGIMax, kBfmGIMax);
  c.ManualPitch = Clamp(kBfmGKp * gErr + BfmGIterm_, -kBfmPushMax, 1.0);

  /* Der Gashebel ist die ZWEITE Haelfte des Annaeherungsproblems: hinter ihn zu zielen haelt die Nase
   * drin, es haelt keinen Jet auf, der 100 kt schneller ist. Geregelt wird die GESCHWINDIGKEITS-
   * DIFFERENZ und nicht die Annaeherungsrate — die traegt die ganze Verfolgungsgeometrie, den Gashebel
   * darauf zu schliessen liesse ihn gegen die Kurve arbeiten (gemessen: Trichterzeit 21,2 → 12,7 s). */
  /* G4 (doc/doctrine-evolution.md 2.1): die Geschwindigkeit, auf der der Verfolger BESTEHT, als
   * Vielfaches seiner EIGENEN Eckgeschwindigkeit. Nur der Gashebel liest sie — der Kurvenradius und der
   * g-Plan oben bleiben beim Haken des Flugzeugs, weil sie Aerodynamik sind und keine Entscheidung. */
  const double insistKt = TunedScale(FBPilotParam::BfmEnergyFrac, BfmCornerSpeedKt());
  double speedErrKt;
  if (validTrack) {
    speedErrKt = (tgtSpeedMs - st.speed) * kMsToKt + schedKt;
  } else {
    speedErrKt = insistKt - casKt;
  }
  c.ManualThr = Clamp(kBfmThrTrim + kBfmThrKpPerKt * speedErrKt, 0.0, 1.0);
  if (lowEnergy) c.ManualThr = 1.0;   /* Energiemangel schlaegt die Geometrie */
  if (casKt > insistKt * kBfmOverspeedFrac)
    c.ManualThr = std::fmin(c.ManualThr, kBfmThrTrim);
  c.Speedbrake = speedErrKt < -kBfmSpeedbrakeKt ? 1.0 : 0.0;

  bool inControl = validTrack && g.Locked && rngNm >= ctrlMinNm && rngNm <= ctrlMaxNm &&
                   g.AspectDeg <= BfmControlAspectDeg() && std::fabs(g.AzDeg) <= BfmControlAtaDeg();
  Bfm_.Report(mode, inControl, gCmd, dt);
  BfmSelectRadarMode(state, avionics);
  BfmDesignate(state, avionics);
  /* EINE Hand, EINE Waffenhandlung je Takt, und die Kanone hat Vorrang: ihr Tor ist das engere (der
   * Trichter ist ein Bruchteil der Zielspannweite) und ihr Fenster endet dort, wo die Mindestentfernung
   * der Rakete beginnt. „Beides in Parametern" ist damit ein schmales Band, und darin gewinnt die
   * Waffe, die ohnehin schon nachfuehrt. doc/pilot.md 5.11. */
  if (!BfmGunfire(state, avionics)) BfmMissileShot(state, avionics);
  return c;
}

/* Die ERFASSUNGS-Haelfte des Kurvenkampfs: waehlt EINMAL den selbst-lockenden Nahkampf-Modus, sobald die
 * Phase in den Merge geht, und danach nur wieder, wenn der Modus verstellt wurde und kein Lock steht.
 * Der Modus lockt selbst (FBRadarScanVolume::AutoAcquire), also macht er die Designation, die im
 * Nahkampf niemand von Hand faehrt — und ohne die die N019-Suchkeule den manoevrierenden Gegner im Merge
 * nie in ihre Bar bekam (gemessen, duel-merge lock_s 0). BfmRadarModeOrdinal() == -1 (die F-16) laesst
 * diesen Zweig tot: ihr acm_hud steht per Missionstext, sie ist byte-identisch. Eine Bedienhandlung je
 * Takt wie alles andere, ueber den Bus, ablehnbar. doc/modules/mig29/module.md gap 4h (a). */
void FBPilot::BfmSelectRadarMode(const FBState &state, FBCommandBus &avionics) {
  int acm = BfmRadarModeOrdinal();
  if (acm < 0) return;
  const FBRadarBlock &fcr = state.Radar;
  if (!fcr.H.Readable()) return;
  if (fcr.LockIndex >= 0) return;          /* haelt schon einen Lock — nicht stoeren */
  if (fcr.ModeOrdinal == acm) return;      /* schon im ACM-Modus */
  if (TimeS_ < BfmModeNextS_) return;
  BfmModeNextS_ = TimeS_ + kInterceptActionS;
  avionics.Post(FBCommandTarget::RadarMode, acm, TimeS_);
}

/* Kein zweites Zielen — ein FINGER. Er prueft NIE, auf wen er schiesst, und kann es nicht: der Pilot
 * sieht einen Radarkontakt, keine Besetzungsliste. Die Mission deklariert die Besetzung, der Abzug
 * beantwortet den Trichter. */
/* DER LOCK IM KURVENKAMPF, und er ist nicht immer geschenkt. Die ACM-Modi eines Musters, dessen Radar
 * sie hat, locken den naechsten festen Track selbsttaetig (FBRadarScanVolume::AutoAcquire) — in einem
 * Kurvenkampf bedient niemand ein Radar, und dieser Zweig laeuft dort nie, weil der Lock schon steht.
 * Ein Muster, dessen Nahkampf-Muster NICHT selbst lockt, hat den Kontakt trotzdem auf dem Schirm und
 * einen Daumen am TMS: dann ist die Designation eine BEDIENHANDLUNG wie jede andere — ueber den Bus,
 * mit ihrer Latenz, ablehnbar, und mit dem Bedien-Takt des Piloten dazwischen statt 10 Hz Spam.
 * Ohne das ist die BFM-Phase auf so einem Jet blind: ohne Lock kein FBBfmTrack, ohne Track kein
 * Zielpunkt, und das Gesetz fliegt sein Suchmuster, waehrend der Gegner vor ihm steht (gemessen auf
 * missions/mig29-gun: 0 Lock-Ticks in 134 s). doc/pilot.md, Abschnitt 5. */
void FBPilot::BfmDesignate(const FBState &state, FBCommandBus &avionics) {
  const FBRadarBlock &fcr = state.Radar;
  if (!fcr.H.Readable() || fcr.ContactCount <= 0 || fcr.LockIndex >= 0) return;
  if (TimeS_ < BfmDesignateNextS_) return;
  const FBRadarContact *best = nullptr;
  for (int i = 0; i < fcr.ContactCount; i++) {
    const FBRadarContact &c = fcr.Contacts[i];
    if (c.Iff == FBIffReply::Friendly) continue;   /* dieselbe Regel wie im Abfang: Schweigen beweist nichts */
    if (!best || c.RangeM < best->RangeM) best = &c;
  }
  if (!best) return;
  BfmDesignateNextS_ = TimeS_ + kInterceptActionS;
  avionics.Post(FBCommandTarget::Designate, best->TrackNum, TimeS_);
}

bool FBPilot::BfmGunfire(const FBState &state, FBCommandBus &avionics) {
  if (!state.Gun.H.Readable() || !state.Gun.Ready) return false;
  const FBFireControlBlock &fc = state.FireControl;
  if (!fc.H.Readable() || !fc.GunValid) { GunHaveErr_ = false; return false; }

  double burstS = Tuned(FBPilotParam::GunBurstS, BfmGunBurstS());
  /* Der Abzug kostet die Zeit eines Fingers: jede HOTAS-Handlung trifft eine Latenz spaeter ein, und bei
   * ~2 °/s Zielfehlerbewegung sind das auf 300 m zehn Meter Fehlabstand je Sekunde Verzug. Also wird der
   * Fehler VORHERGESAGT — aber nur bis zu dem Moment, in dem die Geschosse den Lauf VERLASSEN.
   * FBGunSolveLead beantwortet „wohin muss das Rohr zeigen, damit eine JETZT abgefeuerte Runde das Ziel
   * SPAETER trifft": die Zielbewegung waehrend der Flugzeit steckt bereits in der Loesung, eine
   * abgefeuerte Runde hat von einer sich danach bessernden Zielung also nichts. Der Horizont ist damit
   * Bus-Latenz + halber Feuerstoss (die mittlere Runde dieses Drucks), nicht + fc.GunTofS.
   * doc/pilot.md 5.8. */
  double predErrDeg = fc.GunAimErrorDeg;
  if (GunHaveErr_ && TimeS_ > GunPrevS_) {
    double rate = (fc.GunAimErrorDeg - GunPrevErrDeg_) / (TimeS_ - GunPrevS_);
    predErrDeg = fc.GunAimErrorDeg +
                 rate * (FBCommandBus::LatencyS(FBCommandTarget::GunTrigger) + 0.5 * burstS);
    /* Der publizierte Fehler ist ein BETRAG, seine Extrapolation muss also als einer gelesen werden:
     * −1,5° heisst nicht „genau drauf", sondern „1,5° daran vorbei". */
    predErrDeg = std::fabs(predErrDeg);
  }
  GunPrevErrDeg_ = fc.GunAimErrorDeg;
  GunPrevS_ = TimeS_;
  GunHaveErr_ = true;
  if (!fc.GunInRange) return false;
  /* Der Trichter ist die Toleranz des VISIERS, nicht die des Piloten: seine Waende sind die SPANNWEITE
   * des Ziels, eine Loesung knapp innerhalb setzt das Muster eine halbe Spannweite neben seine Mitte. */
  if (predErrDeg > Tuned(FBPilotParam::GunFireTolFrac, BfmGunFireTolFrac()) * fc.GunTolDeg) return false;
  if (TimeS_ < GunNextS_) return false;
  if (avionics.Post(FBCommandTarget::GunTrigger, burstS, TimeS_).Outcome == FBCommandOutcome::Rejected)
    return false;
  GunNextS_ = TimeS_ + burstS;
  return true;
}

/* DER KURZSTRECKENSCHUSS IM KURVENKAMPF. Fuenf Tore, jedes ein Instrumentenwert, keine neue Rechnung —
 * die Startzone ist die des Feuerleitrechners (weapons/FBLaunchZone, dieselben drei Zahlen, die die
 * Abfangphase liest), der Lock ist der, den die Verriegelung des SMS ohnehin verlangt, und die Grenzen
 * der Runde stehen in ihrer Katalogzeile.
 *   1. der GEWAEHLTE Store ist eine Infrarotrunde — er prueft, was auf der Schiene haengt, statt eine zu
 *      verlangen: die Waffenwahl ist keine Entscheidung, die dieser Pilot hat (doc/duels.md D4);
 *   2. ein LOCK, derselbe, den die Verriegelung liest;
 *   3. IN DER ZONE (Rmin <= R <= Raero). Die Rtr-Disziplin der Abfangphase gilt hier AUSDRUECKLICH
 *      NICHT: Rtr heisst „die Runde kommt an, auch wenn er umdreht und wegrennt", und wer schon im
 *      Kurvenkampf steckt, rennt nicht weg;
 *   4. im CUEING-Winkel dieser ZELLE (BfmWvrCueDeg, sonst der Kardan der Runde);
 *   5. die vorige Runde hatte ihre Flugzeit.
 * ASPEKT ist kein Tor — beide Runden im Baum sind dokumentiert allaspektfaehig; was den Frontalschuss
 * schlechter macht, modelliert sensors/FBIrstSystem und nicht der Pilot. EIGENE LAST ist kein Tor,
 * weil es dafuer keinen Mechanismus gibt: eine Runde verlaesst die Schiene mit Lage und Geschwindigkeit
 * des Traegers, und Schienenlast oder Trennstoerung modelliert der Freigabepfad nirgends.
 * NACH dem Start bindet nichts: FBSeekerHandoverS(Infrared) == 0. doc/pilot.md 5.11. */
void FBPilot::BfmMissileShot(const FBState &state, FBCommandBus &avionics) {
  const FBStoresBlock &sms = state.Stores;
  const FBFireControlBlock &fc = state.FireControl;
  if (!sms.H.Readable()) return;
  /* Gesehen wird der eigene Schuss am ZAEHLER, nicht am eigenen Daumen — dieselbe Regel wie im Angriff. */
  if (BfmSeenReleases_ < 0) BfmSeenReleases_ = sms.ReleasedCount;
  if (sms.ReleasedCount > BfmSeenReleases_) {
    BfmSeenReleases_ = sms.ReleasedCount;
    BfmShotNextS_ = TimeS_ + std::fmax(Tuned(FBPilotParam::ShotSpacingS, InterceptShotSpacingS()),
                                       fc.H.Readable() && fc.TimeToImpactS > 0.0f
                                           ? (double)fc.TimeToImpactS : 0.0);
  }
  if (TimeS_ < BfmShotNextS_) return;
  if (sms.SelectedStation <= 0 || sms.SelectedStation > kMaxStoreStations) return;
  const FBStoreSpec *sel = FBStoreSpecOf((FBStoreKind)sms.Station[sms.SelectedStation - 1]);
  if (!sel || sel->Seeker != FBSeekerKind::Infrared) return;
  if (!state.Radar.H.Readable() || state.Radar.LockIndex < 0) return;
  if (!fc.H.Readable() || !fc.DlzValid || !fc.InZone) return;
  const FBBfmBlock &g = Bfm_.Block();
  if (!g.H.Readable() || !g.Locked) return;
  double cueDeg = BfmWvrCueDeg();
  if (cueDeg < 0.0) cueDeg = sel->SeekerGimbalHalfDeg;
  if (cueDeg > 0.0 && std::fabs(g.AzDeg) > cueDeg) return;

  FBCommandAck r = avionics.Post(FBCommandTarget::WeaponRelease, 1.0, TimeS_);
  FBLog::Info("pilot", "BFM_SHOT", {{"store", sel->Key}, {"rangeM", (double)fc.TargetRangeM},
      {"rminM", (double)fc.RminM}, {"raeroM", (double)fc.RaeroM}, {"ataDeg", g.AzDeg},
      {"aspectDeg", g.AspectDeg}, {"cueDeg", cueDeg}, {"ttiS", (double)fc.TimeToImpactS},
      {"accepted", r.Outcome != FBCommandOutcome::Rejected}});
  /* Der Abstand gilt AB DEM DRUCK und nicht erst ab dem Zaehler: zwischen beiden liegt die
   * Betaetigungslatenz, und ein Pilot, der in ihr weiterdrueckt, leert die Schienen in einer halben
   * Sekunde (gemessen: sechs Starts in 0,6 s). Abgelehnt wird ebenso wenig wiederholt — der Jet hat
   * nein gesagt, wie bei jeder anderen Waffenhandlung. */
  BfmShotNextS_ = TimeS_ + Tuned(FBPilotParam::ShotSpacingS, InterceptShotSpacingS());
}

/* EIN Kommando je Tick, in fester Reihenfolge: deterministisch, und ein Pilot bedient einen Hebel nach
 * dem anderen. Alles laeuft ueber denselben Bus, den eine Hand bedienen wuerde. */
void FBPilot::EnterBriefedItems(FBCommandBus &avionics) {
  if (TimeS_ < BriefNextTryS_) return;
  BriefNextTryS_ = TimeS_ + kBriefRetryS;
  if (BriefAlowPending_) {
    if (avionics.Post(FBCommandTarget::AlowFt, BriefAlowFt_, TimeS_).Outcome != FBCommandOutcome::Rejected)
      BriefAlowPending_ = false;
    return;
  }
  if (BriefBingoPending_) {
    if (avionics.Post(FBCommandTarget::BingoLbs, BriefBingoLbs_, TimeS_).Outcome != FBCommandOutcome::Rejected)
      BriefBingoPending_ = false;
    return;
  }
  if (BriefArmPending_) {
    if (avionics.Post(FBCommandTarget::MasterArm, BriefArm_ ? 1.0 : 0.0, TimeS_).Outcome != FBCommandOutcome::Rejected)
      BriefArmPending_ = false;
    return;
  }
  if (BriefWeaponPending_) {
    if (avionics.Post(FBCommandTarget::WeaponSelect, BriefWeapon_, TimeS_).Outcome != FBCommandOutcome::Rejected)
      BriefWeaponPending_ = false;
    return;
  }
}

bool FBPilot::BriefRelease(double atS) {
  if (ReleaseCount_ >= kMaxBriefedReleases) return false;
  ReleaseAtS_[ReleaseCount_++] = atS;
  return true;
}

/* Die Station ist die eigene Wahl des SMS: dieser Pfad hat kein Zielen und damit keinen Grund, einen
 * Pylon zu bevorzugen — was er hat, ist die restliche Kette (Master Arm, Verriegelungen, Quittung). */
void FBPilot::ReleaseBriefedStores(FBCommandBus &avionics) {
  while (ReleaseNext_ < ReleaseCount_ && TimeS_ >= ReleaseAtS_[ReleaseNext_]) {
    ReleaseNext_++;
    avionics.Post(FBCommandTarget::WeaponRelease, 1.0, TimeS_);
  }
}

bool FBPilot::BriefChaff(double atS) {
  if (DispenseCount_ >= kMaxBriefedDispenses) return false;
  DispenseAtS_[DispenseCount_++] = atS;
  return true;
}

bool FBPilot::BriefGun(double atS, double seconds) {
  if (GunBriefCount_ >= kMaxBriefedGunBursts || seconds <= 0.0) return false;
  GunBriefAtS_[GunBriefCount_] = atS;
  GunBriefS_[GunBriefCount_] = seconds;
  GunBriefCount_++;
  return true;
}

/* Wie jeder Brief: ein abgelehnter Abzug wird NICHT wiederholt — der Jet hat nein gesagt (leere
 * Trommel, Master Arm SAFE, Raeder am Boden), und die Ablehnung steht im Protokoll. */
void FBPilot::FireBriefedGun(FBCommandBus &avionics) {
  while (GunBriefNext_ < GunBriefCount_ && TimeS_ >= GunBriefAtS_[GunBriefNext_]) {
    double burstS = GunBriefS_[GunBriefNext_];
    GunBriefNext_++;
    avionics.Post(FBCommandTarget::GunTrigger, burstS, TimeS_);
  }
}

/* Wert 0 = das vom PRGM-Knopf gewaehlte Programm (CMS vorwaerts): der Pilot wirft, womit der Jet
 * eingerichtet wurde — mitten im Manoever waehlt niemand ein Programm aus. */
void FBPilot::DispenseBriefedCm(FBCommandBus &avionics) {
  while (DispenseNext_ < DispenseCount_ && TimeS_ >= DispenseAtS_[DispenseNext_]) {
    DispenseNext_++;
    avionics.Post(FBCommandTarget::CmDispense, 0.0, TimeS_);
  }
}

/* HOECHSTENS EINE Bedienhandlung je Entscheidungstakt, in Prioritaetsreihenfolge: wer beschossen wird,
 * wirft Duppel, bevor er sich um einen Radarmodus kuemmert. */
bool FBPilot::InterceptCockpit(const FBState &state, FBCommandBus &avionics, int designateTrack,
                               bool wantShot, bool wantChaff, FBBodyAngle wantEl) {
  if (TimeS_ - IntLastActionS_ < Tuned(FBPilotParam::ActionSpacingS, kInterceptActionS)) return false;
  auto post = [&](FBCommandTarget t, double v) {
    IntLastActionS_ = TimeS_;
    avionics.Post(t, v, TimeS_);
  };
  if (wantChaff) {
    post(FBCommandTarget::CmDispense, 0.0);   /* CMS vorwaerts: das vom PRGM-Knopf gewaehlte Programm */
    IntLastChaffS_ = TimeS_;
    return true;
  }
  if (wantShot) {
    post(FBCommandTarget::WeaponRelease, 1.0);
    IntLastShotS_ = TimeS_;
    return true;
  }
  /* Nur wenn das Geraet nicht schon den verlangten Lock haelt: eine Designation ist eine Entscheidung,
   * keine wiederholte Forderung. */
  bool locked = state.Radar.H.Readable() && state.Radar.LockIndex >= 0;
  if (designateTrack > 0 && !locked) { post(FBCommandTarget::Designate, designateTrack); return true; }
  if (designateTrack < 0 && locked) { post(FBCommandTarget::Designate, 0.0); return true; }
  /* Totband, weil ein Knopf, der alle zehntel Sekunden angetippt wird, nicht geflogen wird. */
  if (std::fabs(wantEl.Deg() - IntCmdElDeg_) > kInterceptElDeadDeg) {
    IntCmdElDeg_ = wantEl.Deg();
    IntLastActionS_ = TimeS_;
    avionics.PostAntennaEl(wantEl, TimeS_);
    return true;
  }
  /* ...und, EINMAL, der Suchmodus: diese Phase wird in einem Modus geflogen, der alles findet und nichts
   * lockt. Ordinal < 0 = das Modul hat keinen; das Geraet bleibt, wie die Mission es setzte. */
  /* EMCON zuerst, denn er kann in BEIDE Richtungen schalten, waehrend der Suchmodus darunter eine
   * einmalige Setzung ist. Ein Flugzeug ohne stillen Modus faellt hier ohne einen einzigen Befehl
   * durch, also ist die Aenderung fuer es strukturell ein No-op. */
  int silentMode = SilentRadarModeOrdinal();
  if (silentMode >= 0 && state.Radar.H.Readable()) {
    int want = EmconSilent_ ? silentMode : SearchRadarModeOrdinal();
    if (want >= 0 && state.Radar.ModeOrdinal != want) {
      IntCmdMode_ = !EmconSilent_;   /* der Suchmodus gilt als gesetzt, sobald wir ihn selbst posten */
      post(FBCommandTarget::RadarMode, want);
      return true;
    }
  }
  int searchMode = SearchRadarModeOrdinal();
  if (!IntCmdMode_ && searchMode >= 0 && state.Radar.H.Readable() &&
      state.Radar.ModeOrdinal != searchMode) {
    IntCmdMode_ = true;
    post(FBCommandTarget::RadarMode, searchMode);
    return true;
  }
  return false;
}

/* Drei Instrumente, drei Gruende heimzufliegen, jeder vom BUS abgelesen statt gewusst. Die
 * Sprit-Beurteilung traegt der WARN-Block, weil BINGO eine Zahl ist, zu der sich der PILOT verpflichtet
 * hat — kein Bruchteil, den diese Klasse erfinden darf. */
bool FBPilot::CanPressOn(const FBState &state) const {
  bool weapons = state.Stores.H.Readable() && state.Stores.LoadedCount > 0;
  bool bingo = state.Warnings.H.Readable() && (state.Warnings.Active & FBWarnBingo) != 0;
  /* EIN BILD, kein Sender. Dass „Radar strahlt nicht" eine gebriefte Taktik ist, steht in dieser Datei
   * schon einmal — bei der laufenden Schleife, die deshalb ausdruecklich NICHT abbricht — und das
   * Eintrittstor hat ihr widersprochen. Eine lebende Quelle ist das eigene Geraet, WAEHREND es strahlt,
   * oder eine Meldung, die die Rotte oder der Leitoffizier geschickt hat; genau diese beiden setzt
   * FBFlightPicture ohnehin in jeder Phase zusammen. doc/duels.md D3a. */
  bool sensor = (state.Radar.H.Readable() && state.Radar.Radiating)
             || (state.Datalink.H.Readable() && state.Datalink.TrackCount > 0)
             || (state.NetLink.H.Readable() && state.NetLink.TrackCount > 0);
  return weapons && !bingo && sensor;
}

/* EIN Entscheidungstakt eines Abfangs, in der Reihenfolge, in der die Aufmerksamkeit eines Piloten
 * laeuft: was sehe ich, wer sieht mich, in welchen Zustand setzt mich das, wohin zeige ich, und erst
 * dann welchen Schalter fasse ich an. Alles Gesehene ist von simulierten Boxen geschrieben, nichts
 * davon Wahrheit. doc/pilot-ai.md, Abschnitt 7. */
/* DIE STATION, aus der eigenen Rottenposition und sonst nichts. Eine Rotte ist in ELEMENTE geteilt (je
 * zwei), das erste Element fliegt Line abreast um den Fuehrenden, jedes weitere seitlich versetzt und
 * zurueckgestaffelt — dieselbe Zerlegung, aus der ein Vierer zwei Paare macht. Jede Position bekommt
 * ihre eigene Hoehenstufe, damit keine zwei Maschinen ko-altitude sind. */
void FBPilot::FormationStation(const FBFlightMember &lead, double &latDeg, double &lonDeg,
                               double &altM) const {
  int k = Flight_.Flight().Position - 1;          /* 0 = der Fuehrende selbst */
  int element = k / 2, inElement = k % 2;
  /* Die drei Verhaeltnisse der gebrieften FORM (doc/formation.md F5a), jedes ein Vielfaches seines
   * eigenen Hakens; ohne Brief ist der Faktor 1,0 und die Station ist die des Flugzeugs. */
  double spreadM = TunedScale(FBPilotParam::FlightSpreadFrac, FormationSpreadM());
  double trailM  = TunedScale(FBPilotParam::FlightTrailFrac,  FormationTrailM());
  double stackM  = TunedScale(FBPilotParam::FlightStackFrac,  FormationStackM());
  double lateralM = (inElement ? 1.0 : 0.0) * spreadM - element * 2.0 * spreadM;
  double aftM = element * trailM;
  altM = lead.AltM + k * stackM;

  double h = lead.HeadingDeg * kDeg2Rad;
  double fwdE = std::sin(h), fwdN = std::cos(h);   /* Kurslinie des Fuehrenden, ENU */
  double rightE = fwdN, rightN = -fwdE;
  double eM = -aftM * fwdE + lateralM * rightE;
  double nM = -aftM * fwdN + lateralM * rightN;
  double coslat = std::cos(lead.LatDeg * kDeg2Rad);
  latDeg = lead.LatDeg + nM / kMPerDeg;
  lonDeg = lead.LonDeg + eM / (kMPerDeg * (std::fabs(coslat) > 1e-6 ? coslat : 1e-6));
}

/* POSITIONSHALTEN AUF EINEM BEWEGTEN PUNKT, in zwei getrennten Kanaelen — und die Trennung IST das
 * Gesetz. Ein Direct auf die Station waere reine Verfolgung eines Punktes, der sich mit
 * Kampfgeschwindigkeit bewegt: der Kursfehler wuerde nie null, und der Regelkreis, der gerade den
 * Merge-Rollfehler erzeugt hat, ist genau dieser.
 *   QUER + HOCH: die Bahnfuehrung, die es schon gibt — SetDirectLeg auf die KURSLINIE des Fuehrenden
 *     DURCH die Station (Ursprung = Station, Ziel = kInterceptAimM davor auf seinem Kurs). Damit regelt
 *     der Autopilot einen Querabstand zu einer LINIE statt eine Peilung zu einem Punkt, und die
 *     Herleitung dieses Gesetzes (doc/systems.md) gilt unveraendert.
 *   LAENGS: die Geschwindigkeit. Der Vorhalt ist die Schliessrate, die diese Zelle auch wieder abbauen
 *     kann — dv = sqrt(2*a*|e|) mit a = BfmBrakeMs2(), dieselbe Form wie der BFM-Closure-Fahrplan.
 *     Kein Regelknopf, keine Zeitkonstante, kein Integrator, und BEWUSST kein Deckel: die Wurzel
 *     begrenzt sich selbst (bei 100 m Fehler sind es 42 kt), und was oben herauskommt, deckelt die
 *     Zelle. Ein Deckel bei sqrt(2*a*Spread) war gemessen zu eng — nach einer Verteidigung stand der
 *     Rottenflieger 40 km hinter seiner Station und kam mit 94 m/s Vorhalt nicht mehr heran
 *     ([MESS] four-4v4-asym, Stationsfehler 40-48 km ueber 230 s ohne Tendenz).
 * OHNE FUEHRENDEN auf dem Netz gibt es keine Station: dann fliegt dieser Jet seinen eigenen Plan, denn
 * eine Station auf einer Vermutung ist keine. doc/formation.md, Abschnitt 4. */
FBPilotCommands FBPilot::FormationCommands(const Fdm::fb_fdm_state &st, const FBFlightPlan &plan) {
  FBPilotCommands c{};
  const FBFlightMember *lead = Flight_.Lead();
  /* EINE STATION HAT EINE REICHWEITE. Zwei Jets stuetzen sich, solange sie im Erfassungsraum des
   * jeweils anderen liegen; jenseits der eigenen Commit-Entfernung sind sie keine Rotte mehr, sondern
   * zwei Einzelne, und ein Rottenflieger, der einer 40 km entfernten Station nachfliegt, tut gar
   * nichts ([MESS] four-4v4-asym: 40-49 km Stationsfehler ueber 230 s ohne Tendenz, nachdem ihn eine
   * Verteidigung herausgerissen hatte). Ist der Fuehrende weiter weg, faellt dieser Jet auf seinen
   * eigenen Plan zurueck — die Rotte ist getrennt, und das ist eine Aussage, kein Fehler. */
  double rejoinM = Tuned(FBPilotParam::LockRangeNm, InterceptLockRangeNm()) * kNmToM;
  if (lead && !lead->Self &&
      FBPlanarDistM(st.lat, st.lon, lead->LatDeg, lead->LonDeg) > rejoinM) {
    if (!FlightSplit_) {
      FlightSplit_ = true;
      FBLog::Info("flight", "SPLIT", {{"t", TimeS_},
          {"leadNm", FBPlanarDistM(st.lat, st.lon, lead->LatDeg, lead->LonDeg) * kMToNm},
          {"rejoinNm", rejoinM * kMToNm}});
    }
    lead = nullptr;
  } else if (lead && !lead->Self) {
    FlightSplit_ = false;
  }
  if (!lead || lead->Self) {
    /* Der Fuehrende selbst fliegt die Route — die Rotte folgt IHM, er folgt der Mission. */
    Flight_.NoteStationErr(-1.0);
    const FBWaypoint *wp = plan.ActiveWaypoint();
    if (!wp) return c;
    c.Guidance = FBPilotGuidance::Direct;
    c.TargetLatDeg = wp->LatDeg; c.TargetLonDeg = wp->LonDeg;
    c.TargetAltM = wp->AltM; c.TargetSpeedKt = wp->SpeedKt;
    SetLegFromPlan(c, plan);
    return c;
  }

  double staLat, staLon, staAlt;
  FormationStation(*lead, staLat, staLon, staAlt);

  double eastM, northM;
  FBEnuOffsetM(st.lat, st.lon, staLat, staLon, eastM, northM);
  Flight_.NoteStationErr(std::sqrt(eastM * eastM + northM * northM));

  /* Der Laengsfehler ist die Projektion auf den Kurs des Fuehrenden; alles Uebrige erledigt die Bahn. */
  double h = lead->HeadingDeg * kDeg2Rad;
  double alongM = eastM * std::sin(h) + northM * std::cos(h);
  double aMs2 = std::fmax(0.1, BfmBrakeMs2());
  double dvMs = std::sqrt(2.0 * aMs2 * std::fabs(alongM));
  if (alongM < 0.0) dvMs = -dvMs;

  c.Guidance = FBPilotGuidance::Direct;
  c.HaveLeg = true;
  c.LegLatDeg = staLat; c.LegLonDeg = staLon;
  /* Der Zielpunkt liegt auf der Bahn VON DER STATION AUS, nicht von hier: sonst waere die Linie, die
   * der Autopilot haelt, nicht der Kurs des Fuehrenden, sondern die Verbindung Station-Eigenposition. */
  {
    double coslat = std::cos(staLat * kDeg2Rad);
    c.TargetLatDeg = staLat + kInterceptAimM * std::cos(h) / kMPerDeg;
    c.TargetLonDeg = staLon + kInterceptAimM * std::sin(h) /
                              (kMPerDeg * (std::fabs(coslat) > 1e-6 ? coslat : 1e-6));
  }
  c.TargetAltM = staAlt;
  c.TargetSpeedKt = std::fmax(0.0, (lead->SpeedMs + dvMs) * kMsToKt);
  return c;
}

FBPilotCommands FBPilot::InterceptCommands(const FBState &state, FBCommandBus &avionics,
                                           const Fdm::fb_fdm_state &st, const FBFlightPlan &plan, double dt) {
  /* Die Fusion des gelockten Kontakts, fuer das, was ein einzelnes Echo nicht hergibt: die
   * Zielgeschwindigkeit und daraus der Aspekt, unter dem die Schussentscheidung faellt. */
  Bfm_.Update(state, st, TimeS_);
  const FBBfmBlock &fused = Bfm_.Block();
  /* Ungueltig, bis wirklich einmal gelockt wurde — das haelt jeden Abfang ohne Kontakt auf seinem
   * gebrieften Vektor. */
  const FBTrackDatum datum = Bfm_.Datum(st, TimeS_, CornerTurnRateDegS());

  /* ---- 1. das Bild: welcher Rueckstrahler wird bearbeitet ---- */
  const FBRadarBlock &fcr = state.Radar;
  bool radarUp = fcr.H.Readable();
  bool locked = radarUp && fcr.LockIndex >= 0 && fcr.LockIndex < fcr.ContactCount;
  const FBRadarContact *tgt = nullptr;
  if (locked) {
    tgt = &fcr.Contacts[fcr.LockIndex];
  } else if (radarUp) {
    /* Naechster Rueckstrahler, der sich nicht als Freund ausgewiesen hat. Eine gueltige Mode-4-Antwort
     * BEWEIST freundlich und ist das Einzige, was einen Kontakt von der Liste nimmt; Schweigen beweist
     * nichts — das IST das Identifikationsproblem, keine Abkuerzung daran vorbei. */
    for (int i = 0; i < fcr.ContactCount; i++) {
      const FBRadarContact &c = fcr.Contacts[i];
      if (c.Iff == FBIffReply::Friendly) continue;
      if (!tgt || c.RangeM < tgt->RangeM) tgt = &c;
    }
  }
  /* DIE ZUTEILUNG SCHLAEGT „der naechste": in einer Rotte ist der naechste Rueckstrahler genau der,
   * den der Rottenkamerad auch nimmt. Sie schlaegt auch einen bestehenden Lock, denn ein Lock auf dem
   * falschen Ziel ist das Doppelbekaempfen selbst. Track 0 = keine Rotte oder nichts zu teilen, dann
   * bleibt alles daruber unveraendert. */
  int assignTrack = Flight_.AssignedTrack();
  if (radarUp && assignTrack != 0) {
    for (int i = 0; i < fcr.ContactCount; i++)
      if (fcr.Contacts[i].TrackNum == assignTrack) { tgt = &fcr.Contacts[i]; break; }
    locked = locked && tgt == &fcr.Contacts[fcr.LockIndex];
  }
  if (locked) { if (IntLockSinceS_ < 0.0) IntLockSinceS_ = TimeS_; } else { IntLockSinceS_ = -1.0; }
  bool haveTgt = tgt != nullptr && tgt->LookAgeS < kInterceptLostS;
  if (haveTgt) {
    if (IntTrack_ != tgt->TrackNum) IntTrack_ = tgt->TrackNum;
    Eng_.NoteContact(TimeS_);
    if (locked) Eng_.NoteLock(TimeS_);
  } else {
    IntTrack_ = 0;
    IntHaveLookPitch_ = false;   /* kein Kontakt, keine Lage, auf die sich ein alter Look bezieht */
  }

  double tgtRangeM = haveTgt ? tgt->RangeM : 0.0;
  double tgtAzDeg = haveTgt ? tgt->AzDeg : 0.0;
  double tgtElDeg = haveTgt ? tgt->ElDeg : 0.0;
  double tgtBrgDeg = haveTgt ? tgt->BearingDeg : st.yaw;
  double tgtAltM = haveTgt ? st.elev + tgtRangeM * std::sin(tgt->ElevAngleDeg * kDeg2Rad) : st.elev;
  double tgtClosMs = haveTgt ? tgt->ClosureMs : 0.0;
  double aspectDeg = (locked && fused.H.IsValid()) ? fused.AspectDeg : -1.0;

  /* ---- 2. wer mich sieht: der Warnempfaenger und was er verlangt ---- */
  const FBRwrBlock &rwr = state.Rwr;
  bool threatTrack = false, threatMissile = rwr.H.Readable() && rwr.Powered && rwr.MissileLaunch;
  double threatBrgDeg = 0.0;
  if (rwr.H.Readable() && rwr.Powered) {
    for (int i = 0; i < rwr.ThreatCount; i++) {
      const FBRwrThreat &t = rwr.Threats[i];
      if (t.Mode == FBRwrThreatMode::Search) continue;
      /* Das RAKETEN-Symbol schlaegt ein TRACK-Symbol, was auch immer die Rangfolge des Scopes sagt:
       * das eine ist ein Radar, das schiessen KOENNTE, das andere ein Sucher, der es schon hat. */
      bool better = !threatTrack || t.Mode == FBRwrThreatMode::Missile;
      if (!better) continue;
      threatTrack = true;
      threatBrgDeg = t.BearingDeg;
      if (t.Mode == FBRwrThreatMode::Missile) threatMissile = true;
    }
  }
  if (threatTrack) Eng_.NoteThreat(TimeS_);

  /* Wann eine Warnung eine Drehung wert ist: ein Sucher auf dem Flugzeug ist nie verhandelbar, ein
   * blosser Verfolgungs-Spike schon — wegzudrehen, bevor der eigene Schuss gefallen ist, verliert das
   * Gefecht. Er verlangt eine Antwort erst, wenn der eigene Angriff nichts mehr zu gewinnen hat. */
  bool weapons = state.Stores.H.Readable() && state.Stores.LoadedCount > 0;
  bool shotSelfSufficient = Eng_.HaveShot() && (Eng_.Pitbull() || !locked);
  bool mustDefend = threatMissile || (threatTrack && (!weapons || shotSelfSufficient));
  if (mustDefend) {
    if (IntDefendCueS_ < 0.0) IntDefendCueS_ = TimeS_;
    IntThreatLastS_ = TimeS_;
  } else {
    IntDefendCueS_ = -1.0;
  }
  bool defendDue = mustDefend &&
                   TimeS_ - IntDefendCueS_ >= Tuned(FBPilotParam::ReactionS, kInterceptReactionS);

  /* ---- 3. der Brief: der Vektor, der geflogen wird, solange nichts auf dem Scope ist ----
   * Der aktive Wegpunkt IST der Vektor — die Vektorierung eines Lotsen ist ein Punkt und eine Hoehe.
   * Ohne deklarierten hält der Pilot, worauf er gespawnt wurde, EINMAL verankert, damit die Suche nicht
   * ihrer eigenen Drehung hinterherlaeuft. */
  if (const FBWaypoint *wp = plan.ActiveWaypoint()) {
    IntBriefHdgDeg_ = FBBearingDeg(st.lat, st.lon, wp->LatDeg, wp->LonDeg);
    IntBriefAltM_ = wp->AltM;
    IntAnchored_ = true;
  } else if (!IntAnchored_) {
    IntBriefHdgDeg_ = st.yaw;
    IntBriefAltM_ = st.elev;
    IntAnchored_ = true;
  }

  /* ---- 4. der Schuss: ist einer weg, und ist einer moeglich ---- */
  const FBFireControlBlock &fc = state.FireControl;
  bool zone = fc.H.Readable() && fc.DlzValid;
  double shotRangeM = zone ? fc.TargetRangeM : 0.0;
  bool inParams = zone && fc.InZone && shotRangeM <= fc.RtrM * Tuned(FBPilotParam::ShotRtrFactor, InterceptShotRtrFactor()) &&
                  std::fabs(tgtAzDeg) <= Tuned(FBPilotParam::ShotAtaDeg, InterceptShotAtaDeg());
  int released = state.Stores.H.Readable() ? state.Stores.ReleasedCount : IntSeenReleases_;
  if (released > IntSeenReleases_) {
    IntSeenReleases_ = released;
    Eng_.NoteShot(TimeS_, shotRangeM, tgtAzDeg, aspectDeg, fc.RaeroM, fc.RtrM, fc.RminM, fc.TimeToActiveS,
                  fc.TimeToImpactS);
    /* Eine zweite Runde ist erst dann eine Frage, wenn die erste ihre Chance hatte. Ueber den Treffer
     * erfaehrt der Pilot weiterhin nichts ausser ueber die eigenen Sensoren: ein zerstoerter Jet hoert
     * auf, ein Radarkontakt zu sein, weil er faellt. */
    IntNextShotS_ = TimeS_ + std::fmax(Tuned(FBPilotParam::ShotSpacingS, InterceptShotSpacingS()),
                                       fc.TimeToImpactS > 0.0f ? (double)fc.TimeToImpactS : 0.0);
    IntHaveCrankSign_ = false;
    EngState_ = FBEngageState::Support;
  }
  int chaffOut = state.Cmds.H.Readable() ? state.Cmds.ChaffDispensed : IntSeenChaff_;
  if (chaffOut > IntSeenChaff_) { Eng_.NoteChaff(chaffOut - IntSeenChaff_); IntSeenChaff_ = chaffOut; }

  /* ---- 5. die Zustandsmaschine (doc/pilot-ai.md, Abschnitt 7.3) ----
   * DIE HAERTE EINER HALBAKTIVEN RUNDE: solange eine gestartete Rakete die eigene Beleuchtung braucht,
   * ist Wegdrehen kein Ausweichen, sondern das Aufgeben des Schusses. Ein Modul, dessen Waffe das
   * verlangt, sperrt den Uebergang (SupportInhibitsDefend); fuer jedes andere ist der Ausdruck
   * konstant false und die Maschine unveraendert. */
  bool supportBinding = EngState_ == FBEngageState::Support && SupportInhibitsDefend() &&
                        Eng_.HaveShot() && !Eng_.SupportComplete();
  if (defendDue && supportBinding) {
    if (!IntSupportBound_) {
      IntSupportBound_ = true;
      FBLog::Info("intercept", "SUPPORT_BINDING", {{"t", TimeS_}, {"threatBrgDeg", threatBrgDeg},
          {"sinceShotS", TimeS_ - Eng_.ShotS()}});
    }
    defendDue = false;
  } else if (!supportBinding) {
    IntSupportBound_ = false;
  }
  if (defendDue) {
    EngState_ = FBEngageState::Defend;
  } else if (EngState_ == FBEngageState::Defend) {
    /* DIE ABWEHRFRIST BESITZT DEN ZUSTAND, SOLANGE SIE LAEUFT. Vorher stand hier eine Bedingung statt
     * eines Zweigs, und der allgemeine Zweig unten nahm Defend im ERSTEN Takt nach dem Erloeschen des
     * Symbols weg: die Frist lief nie ab, der Beam wurde genau dann abgebrochen, wenn er wirkte, und
     * CanPressOn darunter war unerreichbarer Code (doc/campaigns/w3-desert-storm.md, Fund 1).
     * Ob das der Notch war oder der Schuetze weggeflogen ist, kann dieses Flugzeug nicht wissen. Was es
     * beantworten kann, ist, ob es noch etwas hat, WOMIT es zurueckkommt. */
    if (TimeS_ - IntThreatLastS_ >= Tuned(FBPilotParam::DefendHoldS, InterceptDefendHoldS()))
      EngState_ = CanPressOn(state) ? FBEngageState::Search : FBEngageState::Abort;
  } else if (EngState_ == FBEngageState::Support) {
    /* Der Crank wird NICHT „bis der Suchkopf uebernimmt" gehalten: das ist das Ende des UPLINK-Bedarfs,
     * nicht das Ende des Schusses. Der Deckel faengt einen Startbereich ab, der nie einen Countdown
     * produziert hat. Das andere Ende ist das Versagen: Lock weg UND kein Pitbull — dafuer lohnt die
     * Rueckkehr, denn Neu-Designieren nimmt den Uplink wieder auf. */
    double sinceShotS = TimeS_ - Eng_.ShotS();
    double holdS = std::fmin(kInterceptSupportMaxS,
                             std::fmax(Eng_.ShotTtiS(), std::fmax(Eng_.ShotTtaS(), 0.0)));
    if (sinceShotS >= holdS || (!locked && !Eng_.Pitbull()))
      EngState_ = weapons && haveTgt ? FBEngageState::Attack : FBEngageState::Abort;
  } else if (EngState_ != FBEngageState::Abort) {
    /* DER SPRIT SCHLAEGT DAS BILD. Er steht ueber allem hier, weil er als Einziges eine Tatsache ueber
     * das FLUGZEUG ist und nicht ueber das, was auf dem Scope steht; unter ihm stehen nur noch Saetze
     * darueber, wer gerade wo ist. Ueber ihm stehen genau zwei: die Abwehr (defendDue, oben) und die
     * eigene Runde, die noch den Uplink braucht (Support, oben) — man hoert nicht wegen Sprit auf, sich
     * zu wehren, und man wirft keinen bereits bezahlten Schuss weg. Von den drei Instrumenten in
     * CanPressOn wird hier NUR dieses gehoben: „Radar strahlt nicht" ist eine gebriefte Taktik (EMCON)
     * und „Traeger leer" hat unten seinen eigenen Zweig mit eigener Ausnahme.
     * doc/pilot.md, Abschnitt 7.4a. */
    if (state.Warnings.H.Readable() && (state.Warnings.Active & FBWarnBingo) != 0) {
      if (EngState_ != FBEngageState::Abort && !IntBingoLogged_) {
        IntBingoLogged_ = true;
        FBLog::Info("intercept", "BINGO_ABORT",
                    {{"t", TimeS_}, {"fuelLbs", state.Airframe.H.Readable() ? state.Airframe.FuelLbs : 0.0f},
                     {"bingoLbs", state.Ufc.H.Readable() ? state.Ufc.BingoEffectiveLbs : 0.0f},
                     {"from", FBEngageStateStr(EngState_)}, {"haveTgt", haveTgt}});
      }
      EngState_ = FBEngageState::Abort;
    }
    /* „Nichts auf dem Scope" schlaegt alles andere: ein Jet ohne Ziel fliegt seinen Brief und sucht,
     * ganz gleich, was auf den Traegern ist — ein Abbruch wegen leerer Traeger, bevor je etwas gesehen
     * wurde, waere schlicht ein Jet, der geht. */
    else if (!haveTgt) EngState_ = FBEngageState::Search;
    else if (!weapons) EngState_ = FBEngageState::Abort;
    else if (tgtRangeM * kMToNm < Tuned(FBPilotParam::AbortRangeNm, InterceptAbortRangeNm()) && !Eng_.HaveShot())
      EngState_ = FBEngageState::Abort;   /* in Sichtweite und nie geschossen: das ist kein Abfang mehr */
    else if (tgtRangeM * kMToNm <= Tuned(FBPilotParam::LockRangeNm, InterceptLockRangeNm())) EngState_ = FBEngageState::Attack;
    else EngState_ = FBEngageState::Closing;
  }

  /* ---- 6. wohin der Jet zeigt ----
   * Hier statt im case gefragt, damit das Verlassen von Search das Muster BEENDET, statt es halb
   * gesweept fuer die naechste Suche stehenzulassen. */
  bool searching = EngState_ == FBEngageState::Search || EngState_ == FBEngageState::Idle;
  double searchWeaveDeg = SearchWeaveDeg(datum, searching);

  FBPilotCommands c{};
  c.Guidance = FBPilotGuidance::Direct;
  c.TargetSpeedKt = Tuned(FBPilotParam::InterceptSpeedKt, InterceptSpeedKt());
  c.TargetAltM = IntBriefAltM_;
  double aimHdgDeg = IntBriefHdgDeg_;
  /* DIE ANTENNENHOEHE IST KOERPERFEST, und der Typ sagt das jetzt: jeder Zweig unten muss benennen,
   * WORAUS er sie gewinnt — aus einem Weltwinkel gegen die eigene Nicklage, oder aus einem
   * Rueckkehrwinkel, der schon koerperfest gemessen wurde. core/FBBodyAngle.h. */
  FBBodyAngle wantEl;
  int designate = 0;
  bool wantShot = false, wantChaff = false;

  switch (EngState_) {
    case FBEngageState::Idle:
    case FBEngageState::Search: {
      /* Der EIGENE NICK macht daraus ein Kommando statt einer Konstante: das Muster ist an die Nase
       * geschraubt, ein steigender Jet schaut also aus dem Band heraus, das er absuchen soll. */
      double bandAltM = IntBriefAltM_;
      double distM = std::fmax(kInterceptAimM * 0.25, 1000.0);
      /* ...ES SEI DENN, dieses Flugzeug hat ihn schon gesehen. Dann ist der Brief veraltet und das Datum
       * nicht. Ohne je gesehenen Gegner ist das Datum ungueltig und jede Zahl unten bleibt die gebriefte,
       * Byte fuer Byte. */
      if (datum.Valid) {
        bandAltM = datum.AltM;
        distM = std::fmax(std::sqrt(datum.EastM * datum.EastM + datum.NorthM * datum.NorthM), 1000.0);
        aimHdgDeg = datum.BearingDeg + searchWeaveDeg;
        c.TargetAltM = datum.AltM;
      }
      wantEl = FBBodyAngle::FromWorldElevation(std::atan2(bandAltM - st.elev, distM) * kRad2Deg,
                                               st.pitch);
      break;
    }
    case FBEngageState::Closing:
    case FBEngageState::Attack: {
      /* Verfolgungskurs auf den Kontakt, co-altitude. Bewusst KEIN Vorhaltekurs: vor dem Lock gibt es
       * keine Geschwindigkeitsschaetzung, nach ihm braucht die RUNDE den Vorhalt, nicht der Schuetze. */
      aimHdgDeg = tgtBrgDeg;
      c.TargetAltM = tgtAltM;
      /* Den Rueckstrahler ZENTRIEREN, nicht relativ nachfuehren: Kontaktelevation und Volumenmitte sind
       * beide koerperbezogen, die gewuenschte Antennenstellung IST also der Rueckkehrwinkel. Aufs
       * aktuelle Kommando zu addieren wanderte die Keule Look fuer Look vom Ziel weg (gemessen).
       *
       * WAEHREND EINES COASTS ist genau dieser Rueckkehrwinkel VERALTET, und zwar um den einzigen
       * Betrag, den dieser Pilot selbst kennt: seine EIGENE Lageaenderung seit dem Look. Die gemeldete
       * Elevation ist koerperbezogen und wurde in der Lage des Looks gemessen; wer seither die Nase
       * bewegt hat, zeigt die Keule woanders hin, ohne dass sich am Ziel etwas geaendert haette. Bei
       * einem FRISCHEN Look ist die Korrektur exakt null — nichts, was nie gecoastet hat, bewegt sich
       * dadurch. [MESS, duel-fulcrum-high] ohne sie stand die N019-Keule (+-6 deg Elevationsfenster)
       * nach EINEM Look fest, waehrend der Jet 6.000 m auf Ziel-Hoehe sank und dabei 7 deg Nick
       * abgab: der Kontakt kam nie wieder, coastete 6 s und fiel — ein Track je Anflug, kein Schuss. */
      if (haveTgt && tgt->LookAgeS <= 0.0f) { IntLookPitchDeg_ = st.pitch; IntHaveLookPitch_ = true; }
      wantEl = FBBodyAngle::Measured(tgtElDeg -
                                     (IntHaveLookPitch_ ? st.pitch - IntLookPitchDeg_ : 0.0));
      if (EngState_ == FBEngageState::Attack) {
        if (!locked) designate = IntTrack_;
        wantShot = locked && inParams && TimeS_ - IntLockSinceS_ >= kInterceptTrackSettleS &&
                   TimeS_ - IntLastShotS_ >= Tuned(FBPilotParam::ShotSpacingS, InterceptShotSpacingS()) && TimeS_ >= IntNextShotS_;
        /* DECKUNG: die Rotte haelt EINEN frei. Ein Schuetze, dessen Runde noch seine Beleuchtung
         * braucht, fliegt an seiner eigenen Antenne — sind alle gebunden, hat die Rotte niemanden mehr,
         * der eine Startwarnung beantworten kann. Die Regel ist fuer jedes Muster dieselbe; ihr PREIS
         * ist die Laenge der Bindung, und die steht in der Waffe (aktiv: bis zur Suchereinschaltung,
         * halbaktiv: bis zum Einschlag). Zurueckgehalten wird hoechstens so lange, wie der eigene
         * Schuss selbst binden wuerde — danach sind zwei Runden in der Luft besser als eine.
         * doc/formation.md, Abschnitt 6. */
        if (wantShot && Flight_.MateBound()) {
          if (IntCoverSinceS_ < 0.0) IntCoverSinceS_ = TimeS_;
          /* G2: das Vielfache der EIGENEN Bindungszeit, das ein Mitglied den Abzug haelt. 0 = Regel aus,
           * 1,0 = genau eine Bindung. Die Bindungszeit selbst bleibt die der Waffe. */
          double capS = TunedScale(FBPilotParam::CoverFrac,
                                   std::fmax(Tuned(FBPilotParam::ShotSpacingS, InterceptShotSpacingS()),
                                             fc.TimeToImpactS > 0.0f ? (double)fc.TimeToImpactS : 0.0));
          if (TimeS_ - IntCoverSinceS_ < capS) {
            wantShot = false;
            Flight_.NoteDeferred(dt);
            if (!IntCoverLogged_) {
              IntCoverLogged_ = true;
              FBLog::Info("flight", "COVER_DEFER", {{"t", TimeS_}, {"track", IntTrack_},
                  {"rangeNm", tgtRangeM * kMToNm}, {"capS", capS}});
            }
          }
        }
      }
      break;
    }
    case FBEngageState::Support: {
      /* DER CRANK: wegdrehen bis an den Rand dessen, was die Antenne noch fuehren kann — jedes Grad
       * davon ist geschenkte Trennung, weil der Uplink den LOCK braucht und nicht die Nase. Die Seite
       * wird EINMAL je Schuss festgelegt, sonst fliegt der Jet eine S, waehrend seine Runde ungestuetzt
       * bleibt. */
      if (!IntHaveCrankSign_) { IntCrankSign_ = tgtAzDeg >= 0.0 ? 1.0 : -1.0; IntHaveCrankSign_ = true; }
      if (haveTgt) {
        double wantAz = IntCrankSign_ * Tuned(FBPilotParam::CrankAtaDeg, InterceptCrankAtaDeg());
        aimHdgDeg = st.yaw + FBWrap180(tgtAzDeg - wantAz);
        c.TargetAltM = tgtAltM;
        if (tgt->LookAgeS <= 0.0f) { IntLookPitchDeg_ = st.pitch; IntHaveLookPitch_ = true; }
        wantEl = FBBodyAngle::Measured(tgtElDeg -
                                       (IntHaveLookPitch_ ? st.pitch - IntLookPitchDeg_ : 0.0));
      } else {
        /* Nichts mehr, wogegen der Crank-Winkel gehalten werden koennte: den erreichten Kurs behalten,
         * statt auf einen gebrieften Vektor zurueckzuschnappen, der dahin zeigt, wo der Kampf WAR. */
        aimHdgDeg = st.yaw;
        c.TargetAltM = st.elev;
      }
      break;
    }
    case FBEngageState::Defend: {
      /* DER BEAM: den Sender auf die 3/9-Linie legen — dort hat die Eigengeschwindigkeit keine
       * Komponente auf seine Sichtlinie, also genau das Clutter-Filter, das ein Puls-Doppler-Geraet
       * verwirft. Von beiden Wegen der kuerzere: die Drehung selbst ist Zeit in seinem besten Fall. */
      double left = FBWrap180(threatBrgDeg + Tuned(FBPilotParam::BeamOffsetDeg, InterceptBeamOffsetDeg()));
      double right = FBWrap180(threatBrgDeg - Tuned(FBPilotParam::BeamOffsetDeg, InterceptBeamOffsetDeg()));
      double turn = std::fabs(right) <= std::fabs(left) ? right : left;
      aimHdgDeg = st.yaw + turn;
      wantChaff = TimeS_ - IntLastChaffS_ >= Tuned(FBPilotParam::ChaffIntervalS, InterceptChaffIntervalS());
      break;
    }
    case FBEngageState::Abort: {
      /* Kalt abdrehen und kalt bleiben: 180° weg vom Letzten, was auf dieses Flugzeug gezeigt hat. */
      double fromDeg = threatTrack ? st.yaw + threatBrgDeg : (haveTgt ? tgtBrgDeg : IntBriefHdgDeg_);
      aimHdgDeg = fromDeg + 180.0;
      break;
    }
  }

  if (!Flight_.MateBound()) { IntCoverSinceS_ = -1.0; IntCoverLogged_ = false; }

  /* WAS DIESER JET SEINER ROTTE MELDET: ein PUNKT und ein Zustand, nie eine Identitaet — dieses Radar
   * weiss nicht, wen es sieht, also kann es das auch niemandem sagen. Die Meldung reist in der eigenen
   * PPLI und erreicht damit nur, wer ohnehin auf diesem Netz ist. */
  {
    bool engaging = haveTgt && (EngState_ == FBEngageState::Closing || EngState_ == FBEngageState::Attack ||
                                EngState_ == FBEngageState::Support);
    double tLat = st.lat, tLon = st.lon;
    if (engaging) {
      double hb = tgtBrgDeg * kDeg2Rad;
      double coslat = std::cos(st.lat * kDeg2Rad);
      double gnd = tgtRangeM * std::cos(tgt->ElevAngleDeg * kDeg2Rad);
      tLat = st.lat + gnd * std::cos(hb) / kMPerDeg;
      tLon = st.lon + gnd * std::sin(hb) / (kMPerDeg * (std::fabs(coslat) > 1e-6 ? coslat : 1e-6));
    }
    Flight_.SetOwnEngagement(engaging, tLat, tLon, tgtAltM);
  }

  AimAlongHeading(st, aimHdgDeg, c.TargetLatDeg, c.TargetLonDeg);

  /* DER ROTTENFLIEGER SUCHT NICHT AUF EIGENE FAUST. Solange ihm nichts zugeteilt ist, ist seine
   * Aufgabe die Station: zwei Jets, die unabhaengig denselben gebrieften Vektor absuchen, sind keine
   * Rotte, sondern zwei Einzelkaempfer mit demselben Auftrag. Er faellt von selbst heraus, sobald sein
   * EIGENES Radar etwas hat, das die Zuteilung ihm gibt — und die ANTENNE bleibt in jedem Fall seine,
   * denn die Haende unten laufen unveraendert weiter. doc/formation.md, Abschnitt 4.3. */
  if (searching && Flight_.Declared() && !Flight_.Flight().IsLead()) {
    const FBFlightMember *lead = Flight_.Lead();
    if (lead && !lead->Self) {
      FBPilotCommands f = FormationCommands(st, plan);
      if (f.Guidance == FBPilotGuidance::Direct) {
        c.HaveLeg = f.HaveLeg;
        c.LegLatDeg = f.LegLatDeg; c.LegLonDeg = f.LegLonDeg;
        c.TargetLatDeg = f.TargetLatDeg; c.TargetLonDeg = f.TargetLonDeg;
        c.TargetAltM = f.TargetAltM; c.TargetSpeedKt = f.TargetSpeedKt;
      }
    }
  }

  /* ---- 6a. EMISSIONSDISZIPLIN (doc/duels.md D3c) ----------------------------------------------
   * Still sein darf nur, wer noch ein BILD hat: ein einzelner Jaeger ohne Netz kann nicht schweigen,
   * denn dann schweigt er blind — genau der Satz, an dem D3 haengt. Der Naechste, den das Bild haelt,
   * entscheidet: das eigene Echo, solange gestrahlt wird, sonst der vom Rottenkameraden gemeldete
   * PUNKT (FBNetReport, ohne Kennung, ohne Typ, mit eigenem Blickalter). Liegt er innerhalb des
   * gebrieften Bruchteils der eigenen Erfassungsreichweite, wird gestrahlt. */
  EmconSilent_ = false;
  if (SilentRadarModeOrdinal() >= 0 && EmconRadiateNm() > 0.0) {
    double radiateM = TunedScale(FBPilotParam::EmconFrac, EmconRadiateNm()) * kNmToM;
    double nearestM = 1e12;
    if (radarUp)
      for (int i = 0; i < fcr.ContactCount; i++)
        nearestM = std::fmin(nearestM, (double)fcr.Contacts[i].RangeM);
    /* BEIDE Meldeblöcke, und dass es zwei sind, ist eine Entscheidung dieses Baums und kein Versehen:
     * FBDatalinkBlock traegt die kooperative Rotte, FBNetLinkBlock den Leitoffizier — derselbe Typ,
     * getrennter Block, weil ein Block genau einen Schreiber hat (core/FBAvionicsBlocks.h). Ein Pilot,
     * der nur einen von beiden liest, ist auf der Haelfte der Sprossen blind. */
    bool other = false;
    const FBDatalinkBlock *srcs[2] = {&state.Datalink, &state.NetLink};
    for (const FBDatalinkBlock *b : srcs) {
      if (!b->H.Readable()) continue;
      for (int i = 0; i < b->TrackCount; i++) {
        /* ZWEI Meldearten, und beide sind ein PUNKT ohne Kennung: die VERBANDS-Haelfte, die ein
         * Rottenkamerad fuellt, waehrend er ein Ziel bearbeitet (FBFlightReport), und die
         * LUFTVERTEIDIGUNGS-Haelfte, die ein Netzknoten meldet (FBNetReport). Ein Jaeger fliegt in
         * diesem Baum fast immer auf der ersten — die zweite gehoert den Netzen, und in den Kampagnen
         * gehoeren die dem Gegner. */
        const FBFlightReport &r = b->Tracks[i].Report;
        if (r.Engaging) {
          other = true;
          nearestM = std::fmin(nearestM, FBPlanarDistM(st.lat, st.lon, r.TgtLatDeg, r.TgtLonDeg));
        }
        const FBNetReport &n = b->Tracks[i].Net;
        if (!n.Reporting) continue;
        other = true;
        nearestM = std::fmin(nearestM, FBPlanarDistM(st.lat, st.lon, n.LatDeg, n.LonDeg));
      }
    }
    EmconSilent_ = other && nearestM > radiateM;
  }

  /* ---- 7. die Haende ---- */
  bool acted = InterceptCockpit(state, avionics, designate, wantShot, wantChaff, wantEl);
  if (acted && (wantChaff || EngState_ == FBEngageState::Defend))
    Eng_.NoteDefensiveAction(TimeS_, IntDefendCueS_ >= 0.0 ? IntDefendCueS_ : IntThreatLastS_);

  Eng_.NoteSupport(locked, TimeS_, dt);
  double esFt = (st.elev + st.speed * st.speed / (2.0 * kBfmG0)) * kMToFt;
  Eng_.Report(EngState_, haveTgt, locked, tgtRangeM, tgtAzDeg, aspectDeg, tgtClosMs, esFt, dt);
  return c;
}

/* Der LUFT-BODEN-PASS: drei Teile und EINE Entscheidung. Diese Klasse rechnet keine Ballistik und haelt
 * keine Zielposition — sie liest ein Instrument (die Luft-Boden-Felder des FireControl-Blocks), genau
 * wie sie vor einem Flare den Radarhoehenmesser liest. doc/pilot-ai.md, Abschnitt 4. */
FBPilotCommands FBPilot::AttackCommands(const FBState &state, FBCommandBus &avionics,
                                        const Fdm::fb_fdm_state &st, const FBFlightPlan &plan) {
  FBPilotCommands c{};

  /* ---- der Weg hinaus, sobald der Store weg ist ---- */
  if (AtkReleased_) {
    if (TimeS_ >= AtkEgressUntilS_) { Transition(Phase::Route); return c; }
    c.Guidance = FBPilotGuidance::Direct;
    c.TargetLatDeg = AtkEgressLatDeg_; c.TargetLonDeg = AtkEgressLonDeg_;
    c.TargetAltM = AtkEgressAltM_;
    c.TargetSpeedKt = st.cas * kMsToKt;   /* halten, was der Anflug hinterliess: die Kurve ist das Manoever */
    return c;
  }

  const FBWaypoint *wp = plan.ActiveWaypoint();
  if (!wp) { Transition(Phase::Route); return c; }   /* nichts gebrieft, was anzugreifen waere */

  /* Der ANFLUG ist eine BAHN, keine Peilung, und sie wird beim Beginn des Passes verankert: ein
   * Angriffsanflug wird ausgerollt und gehalten, und genau das macht den Abwurfmoment zur einzigen
   * Variablen. Der Peilung hinterherzufliegen liess den Jet 31 m querab abtreiben (gemessen). */
  if (!AtkHaveRunIn_) {
    AtkHaveRunIn_ = true;
    AtkRunInLatDeg_ = st.lat; AtkRunInLonDeg_ = st.lon;
  }
  c.Guidance = FBPilotGuidance::Direct;
  c.TargetLatDeg = wp->LatDeg; c.TargetLonDeg = wp->LonDeg;
  c.TargetAltM = wp->AltM;
  c.TargetSpeedKt = wp->SpeedKt;
  c.HaveLeg = true;
  c.LegLatDeg = AtkRunInLatDeg_; c.LegLonDeg = AtkRunInLonDeg_;

  /* Die Ausweichkurve beginnt, wenn der Store WEG ist, nicht wenn der Daumen unten war: dazwischen liegt
   * die Betaetigungslatenz, und wer in ihr schon rollt, wirft aus der Kurve ab (gemessen: 32 deg
   * Querlage und -0,6 m/s im Ausloesemoment). Gesehen wird der Abwurf am SMS-Zaehler wie jeder andere
   * Instrumentenwert — bis dahin faellt oben die Anflugbahn heraus, unveraendert. */
  if (AtkPickled_) {
    const FBStoresBlock &sms = state.Stores;
    /* DIE BINDUNG AN DIE EIGENE LENKBOMBE, und sie ist EIN Instrumentenwert: solange der eigene SMS
     * meldet, dass er einen Punkt beleuchtet, wird der Anflug GEHALTEN — die Ausweichkurve naehme dem
     * Sucher den Fleck, was die reale Einsatzbedingung dieser Waffe ist. Ein Jet, der keine
     * halbaktive Laserrunde geworfen hat, liest hier false und dreht wie bisher.
     * doc/air-to-ground.md §3.2. */
    if (sms.H.Readable() && sms.Designating) return c;
    if (!sms.H.Readable() || sms.ReleasedCount > AtkReleasedSeen_) {
      AtkReleased_ = true;                        /* kein Zaehler mehr = der Pass ist trotzdem vorbei */
      StartAttackEgress(state, st);
    }
    return c;
  }

  /* DER ANTIRADIATIONS-CUE, und er ist die einzige Zeile, die dieser Pass fuer die neue Waffe braucht:
   * kein Countdown, weil es keine Entfernung gibt, sondern eine PEILUNG im Sucherkegel. Gelesen wird
   * der RWR-BLOCK wie jedes andere Instrument — der Pilot erfaehrt keine Entfernung, weil es keine zu
   * erfahren gibt. doc/air-to-ground.md §7. */
  if (AtkMode_ == FBDeliveryMode::Arm) {
    const FBRwrBlock &rwr = state.Rwr;
    if (!rwr.H.Readable()) return c;
    const FBRwrThreat *cue = nullptr;
    for (int i = 0; i < rwr.ThreatCount && !cue; i++) {
      FBEmitterKind k = rwr.Threats[i].Kind;
      bool ok = AtkArmClass_ == FBArTargetClass::AnySurface
                    ? (k == FBEmitterKind::SurfaceFireControl || k == FBEmitterKind::SurfaceEarlyWarning)
                : AtkArmClass_ == FBArTargetClass::SurfaceFireControl
                    ? k == FBEmitterKind::SurfaceFireControl
                    : k == FBEmitterKind::SurfaceEarlyWarning;
      if (ok) cue = &rwr.Threats[i];
    }
    if (!cue) return c;
    /* Der Sucherkegel der Runde, gefragt am SCHUETZEN: ein Schuss weiter von der Nase weg als das
     * erfasst nach der Trennung nichts. Der Wert ist die des gewaehlten Stores. */
    const FBStoreSpec *sel = state.Stores.H.Readable() && state.Stores.SelectedStation > 0
                                 ? FBStoreSpecOf((FBStoreKind)state.Stores
                                        .Station[state.Stores.SelectedStation - 1])
                                 : nullptr;
    double fovDeg = sel && sel->SeekerFovHalfDeg > 0.0 ? sel->SeekerFovHalfDeg : 0.0;
    if (fovDeg > 0.0 && std::fabs((double)cue->BearingDeg) > fovDeg) return c;
    FBCommandAck ra = avionics.Post(FBCommandTarget::WeaponRelease, 1.0, TimeS_);
    FBLog::Info("pilot", "ATTACK_RELEASE", {{"mode", FBDeliveryModeStr(AtkMode_)},
        {"accepted", ra.Outcome != FBCommandOutcome::Rejected},
        {"class", FBArTargetClassStr(AtkArmClass_)}, {"kind", FBEmitterKindStr(cue->Kind)},
        {"brgDeg", (double)cue->BearingDeg}, {"elDeg", (double)cue->ElDeg},
        {"signal", (double)cue->SignalNorm}, {"fovDeg", fovDeg},
        {"altM", st.elev}, {"gsMs", st.gs}});
    AtkPickled_ = true;
    AtkReleasedSeen_ = state.Stores.H.Readable() ? state.Stores.ReleasedCount : -1;
    if (ra.Outcome == FBCommandOutcome::Rejected) { AtkReleased_ = true; StartAttackEgress(state, st); }
    return c;
  }

  const FBFireControlBlock &fc = state.FireControl;
  if (!fc.H.Readable() || !fc.AgValid) return c;
  if (fc.AgArmMarginS <= 0.0) return c;   /* zu tief zum Schaerfen — von hier wird der Pass nicht geflogen */

  /* ERST IN RANGE, DANN der Cue: ein Freigabe-Countdown zaehlt DURCH null, „<= 0" allein feuerte also
   * auch auf eine Loesung, die nie positiv war. Das Verriegeln des In-Range-Bits ist ohnehin, was ein
   * Pilot tut — man sieht den Cue die Steuerlinie herunterkommen, bevor man etwas drueckt. */
  if (fc.AgInRange) AtkInRangeSeen_ = true;
  if (!AtkInRangeSeen_) return c;

  /* Der Pickle wird um die EIGENE Betaetigungslatenz VORGEHALTEN: genau auf dem Cue zu druecken liesse
   * den Store eine halbe Sekunde zu spaet los, bei 230 m/s also 115 m. Der echte Jet loest dasselbe
   * andersherum — der Pilot HAELT, und das FLUGZEUG loest aus. Das ist Wissen des Piloten ueber seine
   * eigenen Haende, kein Blick auf irgendetwas. */
  double bias = Tuned(FBPilotParam::AttackBiasS, AttackReleaseBiasS());
  /* Zwischen der ZAHL und ihrer WIRKUNG liegt mehr als die Bus-Latenz, und beides ist dem Piloten
   * bekannt: sein eigener Entscheidungstakt (er liest den Cue und drueckt einen Takt spaeter — die
   * Betaetigung erreicht den Bus also erst dann) und das ALTER des Cues, das dessen Gueltigkeitskopf
   * traegt. Ohne den ersten Term loest er systematisch einen Takt zu spaet aus: gemessen 63,7 m
   * Laengsfehler gegen 42,8 m Rechnerfehler, also 21 m allein aus dem Takt bei 211 m/s. */
  double leadS = FBCommandBus::LatencyS(FBCommandTarget::WeaponRelease) + DecisionDtS_;
  double solAgeS = TimeS_ - fc.H.StampS;
  if (solAgeS < 0.0 || solAgeS > 1.0) solAgeS = 0.0;
  bool cue = fc.AgTimeToReleaseS - solAgeS <= leadS - bias;
  /* CCIPs Zusatzbedingung ist die QUER-Haelfte des Zielfehlers und nur sie: die Laengshaelfte ist das,
   * wofuer der Cue da ist, und im Moment des Druckens absichtlich ungleich null. */
  if (AtkMode_ == FBDeliveryMode::Ccip)
    cue = cue && std::fabs(fc.AgCrossErrM) <= Tuned(FBPilotParam::AttackCcipTolM, AttackCcipTolM());
  if (!cue) return c;

  /* Abgelehnt ist ENDGUELTIG und der Pass vorbei — dieselbe Regel wie fuer jede andere Waffenhandlung. */
  FBCommandAck r = avionics.Post(FBCommandTarget::WeaponRelease, 1.0, TimeS_);
  FBLog::Info("pilot", "ATTACK_RELEASE", {{"mode", FBDeliveryModeStr(AtkMode_)},
      {"accepted", r.Outcome != FBCommandOutcome::Rejected},
      {"ttrS", (double)fc.AgTimeToReleaseS}, {"leadS", leadS}, {"biasS", bias},
      {"alongErrM", (double)fc.AgAlongErrM}, {"crossErrM", (double)fc.AgCrossErrM},
      {"missM", (double)fc.AgMissM}, {"bombRangeM", (double)fc.AgRangeM},
      {"tofS", (double)fc.AgTofS}, {"armMarginS", (double)fc.AgArmMarginS},
      {"altM", st.elev}, {"gsMs", st.gs}});
  AtkPickled_ = true;
  AtkReleasedSeen_ = state.Stores.H.Readable() ? state.Stores.ReleasedCount : -1;
  if (r.Outcome == FBCommandOutcome::Rejected) { AtkReleased_ = true; StartAttackEgress(state, st); }
  return c;
}

/* EINMAL gerechnet, damit die Ausweichkurve ein fester Ort in der Welt ist statt eines Kurses, den die
 * Guidance jeden Tick neu herleiten muesste. Immer nach rechts: eine Ausweichkurve muss irgendwohin, und
 * die Seite aus der Geometrie zu waehlen waere eine Entscheidung ohne Quelle. */
void FBPilot::StartAttackEgress(const FBState &state, const Fdm::fb_fdm_state &st) {
  AtkEgressUntilS_ = TimeS_ + AttackEgressS();
  double trackDeg = state.AirData.H.Readable() ? state.AirData.TrackDeg : st.yaw;
  double hdg = (trackDeg + AttackEgressTurnDeg()) * kDeg2Rad;
  double coslat = std::cos(st.lat * kDeg2Rad);
  double rng = AttackEgressRangeM();
  AtkEgressLatDeg_ = st.lat + rng * std::cos(hdg) / kMPerDeg;
  AtkEgressLonDeg_ = st.lon + (coslat > 1e-6 ? rng * std::sin(hdg) / (kMPerDeg * coslat) : 0.0);
  AtkEgressAltM_ = st.elev + AttackEgressClimbM();
}

FBPilotCommands FBPilot::Run(const FBState &state, FBCommandBus &avionics,
                             const Systems::FBAirframeControls &airframe, const Fdm::fb_fdm_state &st,
                             const FBFlightPlan &plan, const FBRunway *runway, double dt) {
  PhaseElapsedS += dt;
  TimeS_ += dt;
  DecisionDtS_ = dt;
  FBPilotCommands c{};

  /* DIE ROTTE, vor allem anderen und in JEDER Phase: sie ist ein BILD, kein Manoever, und die Phase
   * darunter liest es (Formation die Station, Intercept die Zuteilung). Ohne deklarierte Rotte kehrt
   * das hier sofort zurueck. */
  {
    FBFlightPicture::FBSortParams sp;
    sp.TurnRateDegS = CornerTurnRateDegS();
    sp.CommitRangeM = Tuned(FBPilotParam::LockRangeNm, InterceptLockRangeNm()) * kNmToM;
    sp.SwitchMarginS = kInterceptTrackSettleS;
    const FBWaypoint *awp0 = plan.ActiveWaypoint();
    sp.AxisDeg = awp0 ? FBBearingDeg(st.lat, st.lon, awp0->LatDeg, awp0->LonDeg) : st.yaw;
    /* GEBUNDEN heisst: eine gestartete Runde braucht diesen Jet noch. WELCHE Runde, weiss FBEngagement
     * (aktiv bis zur Suchereinschaltung, halbaktiv bis zum Einschlag) — die Regel ist also fuer beide
     * Muster dieselbe und nur ihr PREIS unterscheidet sich. */
    bool bound = Eng_.HaveShot() && !Eng_.SupportComplete();
    bool threatened = false;
    if (state.Rwr.H.Readable() && state.Rwr.Powered) {
      for (int i = 0; i < state.Rwr.ThreatCount; i++)
        if (state.Rwr.Threats[i].Mode != FBRwrThreatMode::Search) threatened = true;
      threatened = threatened || state.Rwr.MissileLaunch;
    }
    Flight_.Update(state, st, TimeS_, dt, bound, threatened, sp);
  }

  /* Cockpitarbeit erst, wenn der Jet sich selbst fliegt: nicht in Idle (niemand sitzt drin) und nicht
   * mit Gewicht auf dem Fahrwerk, wo die echte Checkliste diese Eingaben vor den Triebwerksstart legt. */
  if (CurPhase != Phase::Idle && !airframe.GetWeightOnWheels()) {
    EnterBriefedItems(avionics);
    ReleaseBriefedStores(avionics);
    DispenseBriefedCm(avionics);
    FireBriefedGun(avionics);
  }

  /* Telemetriecache: dieselbe Wegpunktdistanz, die der Missions-Runner frueher von aussen rechnete. */
  ActiveWpCache = plan.ActiveIndex();
  DistToWpCache = -1.0;
  if (const FBWaypoint *awp = plan.ActiveWaypoint()) {
    DistToWpCache = FBPlanarDistM(st.lat, st.lon, awp->LatDeg, awp->LonDeg);
  }

  switch (CurPhase) {
    case Phase::Idle:
      return c;   /* neutral: der AP bleibt unangetastet */

    case Phase::Preflight: {
      /* Nicht am Boden = ein Preflight hat nichts Sinnvolles zu tun; neutral bleiben statt zu raten. */
      if (!airframe.GetWeightOnWheels()) return c;
      c.Guidance = FBPilotGuidance::Manual;   /* Leerlauf, Fluegel waagerecht, Bremsen fest */
      c.GearDown = true;
      c.WheelBrakeLeft = 1.0; c.WheelBrakeRight = 1.0;
      bool engineOk = airframe.GetEngineRunning(0);
      if (engineOk && PhaseElapsedS >= kPreflightHoldS) Transition(Phase::Takeoff);
      return c;
    }

    case Phase::Takeoff: {
      c.Guidance = FBPilotGuidance::Manual;
      c.GearDown = true;
      c.WheelBrakeLeft = 0.0; c.WheelBrakeRight = 0.0;   /* Bremsen los */
      c.ManualThr = TakeoffThrottleNorm();
      c.ManualRoll = 0.0; c.ManualYaw = 0.0;

      if (runway) c.NosewheelSteer = NosewheelSteerCmd(*runway, st.lat, st.lon, st.yaw);

      double vr = RotationSpeedKt(airframe.GetGrossWeightLbs());
      double casKt = st.cas * kMsToKt;
      if (casKt >= vr - RotationLeadKt())
        c.ManualPitch = PitchHoldStick(RotationPitchDeg(), st.pitch, st.q, kRotateStickMax);
      else
        c.ManualPitch = 0.0;   /* Stick neutral bis zum Rotationsruf */

      if (!airframe.GetWeightOnWheels()) Transition(Phase::Climb);
      return c;
    }

    case Phase::Climb: {
      const FBWaypoint *wp = plan.ActiveWaypoint();
      if (!wp) { Transition(Phase::Shutdown); return c; }
      c.Guidance = FBPilotGuidance::Direct;
      c.TargetLatDeg = wp->LatDeg; c.TargetLonDeg = wp->LonDeg;
      c.TargetAltM = wp->AltM;
      c.TargetSpeedKt = ClimbSpeedKt();
      SetLegFromPlan(c, plan);
      /* JEDES AGL-Gate fragt zuerst den KOPF des Radarhoehen-Blocks: ohne gueltige Hoehe handelt der
       * Pilot NICHT — Fahrwerk bleibt unten, der Flare loest nicht aus, der BFM-Boden zieht nicht
       * (doc/modules/f16/controls-commands.md §6.4: der Sensor sperrt die Wirkung, nicht das Kommando). */
      if (st.vy > kPositiveRateMs && state.RadarAlt.H.Readable() &&
          state.RadarAlt.AglFt > kGearUpAglFt && st.cas * kMsToKt < GearUpLimitKt())
        c.GearDown = false;
      if (airframe.GetGearPosition() <= 0.02) Transition(Phase::Route);
      return c;
    }

    case Phase::Route: {
      const FBWaypoint *wp = plan.ActiveWaypoint();
      if (!wp) { Transition(Phase::Shutdown); return c; }   /* kein Wegpunkt mehr -> das SUCCESS-Tor der Mission */
      if (wp->Type == FBWaypointType::Land) { Transition(Phase::Approach); return c; }
      c.Guidance = FBPilotGuidance::Direct;
      c.TargetLatDeg = wp->LatDeg; c.TargetLonDeg = wp->LonDeg;
      c.TargetAltM = wp->AltM;
      c.TargetSpeedKt = wp->SpeedKt;
      SetLegFromPlan(c, plan);
      return c;
    }

    case Phase::Approach: {
      if (!runway) { Transition(Phase::Shutdown); return c; }
      /* Aufgesetzt, bevor FlareStartAglFt ausgeloest hat: Rollout behandelt das korrekt, weil
       * Aerobrake/Derotate ein reiner CAS-Fahrplan ist und keinen Flare voraussetzt. */
      if (airframe.GetWeightOnWheels()) { Transition(Phase::Rollout); return c; }

      c.Guidance = FBPilotGuidance::Course;
      c.TargetLatDeg = runway->ThresholdLatDeg; c.TargetLonDeg = runway->ThresholdLonDeg;
      c.CourseDeg = runway->TrueHeadingDeg;
      c.TargetAltM = runway->ThresholdElevM;
      c.GlidepathDeg = GlidepathAngleDeg();
      c.TargetSpeedKt = ApproachSpeedKt();
      c.Speedbrake = ApproachSpeedbrakeNorm();
      if (st.cas * kMsToKt < GearUpLimitKt()) c.GearDown = true;

      if (state.RadarAlt.H.Readable() && state.RadarAlt.AglFt <= FlareStartAglFt())
        Transition(Phase::Flare);
      return c;
    }

    case Phase::Flare: {
      if (airframe.GetWeightOnWheels()) { Transition(Phase::Rollout); return c; }
      c.Guidance = FBPilotGuidance::Manual;
      c.ManualThr = 0.0;      /* Leerlauf (doc/modules/f16/procedures-landing.md, Short Final) */
      c.ManualRoll = 0.0; c.ManualYaw = 0.0;
      c.ManualPitch = PitchHoldStick(FlareTargetPitchDeg(), st.pitch, st.q, kFlareStickMax);
      c.Speedbrake = ApproachSpeedbrakeNorm();
      return c;
    }

    case Phase::Rollout: {
      c.Guidance = FBPilotGuidance::Manual;
      c.ManualThr = 0.0;
      c.ManualRoll = 0.0; c.ManualYaw = 0.0;
      c.Speedbrake = 1.0;   /* voll offen (doc/modules/f16/procedures-landing.md, Roll-Out) */

      /* Zwei-Punkt-Aerobrake oberhalb AerobrakeSpeedKt, darunter proportionales Absenken der Nase —
       * dasselbe PD wie Rotation und Flare, nur mit geschwindigkeitsgeplantem Ziel. */
      double casKt = st.cas * kMsToKt;
      double brakeKt = AerobrakeSpeedKt();
      double targetPitch = casKt > brakeKt ? AerobrakePitchDeg()
                                            : AerobrakePitchDeg() * Clamp(casKt / brakeKt, 0.0, 1.0);
      c.ManualPitch = PitchHoldStick(targetPitch, st.pitch, st.q, kRotateStickMax);

      if (runway) c.NosewheelSteer = NosewheelSteerCmd(*runway, st.lat, st.lon, st.yaw);

      /* Radbremsen erst nach dem Absenken (doc/modules/f16/procedures-landing.md, Roll-Out) — und das ist die
       * NASE AM BODEN, nicht AerobrakeSpeedKt: in der Zwei-Punkt-Lage tragen die Fluegel das Flugzeug,
       * die Haupraeder sind also unbelastet und eine Bremse hat gar nichts zu greifen. Die Prozedur
       * nennt die ~100 kt als ERWARTUNG, wann die Nase faellt; faellt sie frueher, weil das
       * Hoehenruder die Lage nicht mehr haelt, ist die Aerobrake dort zu Ende und die Bremsen gehoeren
       * dorthin. Gelatcht, damit ein Aufsetz-Huepfer sie nicht wieder loest. */
      if (airframe.GetNoseWheelOnGround()) RolloutNoseDown_ = true;
      double brake = RolloutNoseDown_ ? RolloutBrakeNorm() : 0.0;
      c.WheelBrakeLeft = brake; c.WheelBrakeRight = brake;
      return c;
    }

    case Phase::Bfm:
      return BfmCommands(state, avionics, st, dt);

    case Phase::Intercept:
      return InterceptCommands(state, avionics, st, plan, dt);

    case Phase::Attack:
      return AttackCommands(state, avionics, st, plan);

    case Phase::Formation:
      return FormationCommands(st, plan);

    case Phase::Shutdown:
    default:
      return c;
  }
}

void FBPilot::DeclareTelemetry(FBTelemetrySchema &schema) const {
  schema.Add("phase");
  schema.Add("activeWp");
  schema.Add("distToWpM", "m");
}

void FBPilot::SampleTelemetry(FBTelemetryRow &row) const {
  row.Push(std::string(PhaseName(CurPhase)));
  row.Push(ActiveWpCache);
  row.Push(DistToWpCache);
}

} // namespace FlightBox::Pilot
