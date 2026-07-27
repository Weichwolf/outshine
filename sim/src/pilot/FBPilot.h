/* FlightBox — FBPilot: die Missions-Ebene UEBER Guidance und FBW. Sie entscheidet WOHIN und gibt das
 * im ~10-Hz-Entscheidungstakt als FBPilotCommands aus; die Phasen-Zustandsmaschine ist das
 * Prozedur-Geruest, Run() der EINE Override-Punkt. Die zellenspezifischen ZAHLEN sind die virtuellen
 * Hooks unten (Defaults = generische Platzhalter, nicht die Zahlen eines echten Flugzeugs).
 * Phasen, Regelgesetze, Herleitungen und die vollstaendige Hook-Tabelle: doc/flightbox/pilot-ai.md. */
#ifndef FBPILOT_H
#define FBPILOT_H

#include <optional>
#include "FBAirframeControls.h"
#include "FBBfmTrack.h"
#include "FBCommandBus.h"
#include "FBEngagement.h"
#include "FBFlightPlan.h"
#include "FBPilotTuning.h"
#include "FBRunway.h"
#include "FBState.h"
#include "FBStore.h"
#include "FBTelemetry.h"
#include "FBFdm.h"

namespace FlightBox::Pilot {

/* None = „den AP unangetastet lassen" — das Modul ruft den passenden Setter nur bei konkretem Modus. */
enum class FBPilotGuidance { None, Manual, Direct, Course };

/* Die Ausgabe EINES Entscheidungstakts. Jedes Zellen-Kommando ist std::optional: nicht gesetzt heisst
 * „der Pilot fasst diese Bedienung gerade nicht an", was die meisten Ticks fuer die meisten zutrifft. */
struct FBPilotCommands {
  FBPilotGuidance Guidance = FBPilotGuidance::None;

  double TargetAltM = 0.0, TargetSpeedKt = 0.0;
  double TargetLatDeg = 0.0, TargetLonDeg = 0.0;   /* Direct-Zielpunkt / Course-Referenzpunkt */
  /* DIE BAHN, an deren Ende der Direct-Zielpunkt sitzt. Nicht gesetzt ist kein fehlender Wert, sondern
   * die Aussage „hier gibt es keine Linie" — Abfang, Kampf, Suche und Break steuern auf einen PUNKT und
   * sollen das. Kein std::optional, weil es keine Bedienung ist, sondern WISSEN des Piloten. */
  bool   HaveLeg = false;
  double LegLatDeg = 0.0, LegLonDeg = 0.0;         /* der Ursprungsfix der Bahn */
  double CourseDeg = 0.0, GlidepathDeg = 0.0;      /* nur Course; TargetAltM ist dort die Schwellenhoehe */
  double ManualRoll = 0.0, ManualPitch = 0.0, ManualYaw = 0.0, ManualThr = 0.0;   /* Manual-Durchgriff */

  std::optional<bool>   GearDown;
  std::optional<double> Speedbrake;                       /* 0..1 */
  std::optional<double> WheelBrakeLeft, WheelBrakeRight;   /* 0..1 */
  std::optional<double> NosewheelSteer;                    /* -1..1 */
  std::optional<bool>   EngineStart;                       /* true = Start, false = Cutoff */
};

class FBPilot : public FBTelemetrySource {
public:
  /* Die Reihenfolge ist telemetrie-sichtbar: nur ANHAENGEN, nie umsortieren. */
  enum class Phase { Idle, Preflight, Takeoff, Climb, Route, Approach, Flare, Rollout, Shutdown, Bfm,
                     Intercept, Attack };
  static const char *PhaseName(Phase p);

  FBPilot() = default;
  virtual ~FBPilot() = default;

  Phase GetPhase() const { return CurPhase; }
  void  SetPhase(Phase p) { CurPhase = p; PhaseElapsedS = 0.0; }

  /* Der EINE Override-Punkt. `airframe` ist die Zelle, CONST geborgt: nur Instrumentenwerte, ueber
   * DIESELBE Schnittstelle, aus der die Kommandos zurueckkommen — der Pilot fasst nie ein FDM an.
   * MEHR STEHT NICHT IN DIESER SIGNATUR, und das ist der Punkt: ein Pilot fliegt auf Instrumenten und
   * hat keinen Pfad zur Welt oder zur Einheiten-Registry. */
  virtual FBPilotCommands Run(const FBState &state, FBCommandBus &avionics,
                              const Systems::FBAirframeControls &airframe, const Fdm::fb_fdm_state &st,
                              const FBFlightPlan &plan, const FBRunway *runway, double dt);

  /* DER BRIEF: was dieser Pilot im Jet einstellen SOLL, und das Einzige, wofuer er je eine Avionikbox
   * bedient. Ein Wert hier wird nicht angewandt, sondern spaeter ueber den Kommandobus EINGEGEBEN — in
   * der Latenzklasse der jeweiligen Bedienung und ablehnbar. Ohne Brief bedient er nichts.
   * kBriefRetryS: ein abgelehnter DED-Eintrag ist eine Hand und ein Kopf, keine 10-Hz-Schleife. */
  static constexpr double kBriefRetryS = 2.0;

  void BriefAlowFt(double ft) { BriefAlowFt_ = ft; BriefAlowPending_ = true; }
  void BriefBingoLbs(double lbs) { BriefBingoLbs_ = lbs; BriefBingoPending_ = true; }
  void BriefMasterArm(bool arm) { BriefArm_ = arm; BriefArmPending_ = true; }
  void BriefWeapon(double sel) { BriefWeapon_ = sel; BriefWeaponPending_ = true; }
  /* DER ABWURF-BRIEF: pickeln bei `atS` Missionssekunden, ein Eintrag je Store, in Brief-Reihenfolge.
   * Keine Eingabe, sondern eine HANDLUNG zu einem Moment — deshalb traegt sie ihre eigene Zeit. Ein
   * abgelehnter Abwurf wird NICHT wiederholt: der Jet hat nein gesagt. */
  static constexpr int kMaxBriefedReleases = 8;
  bool BriefRelease(double atS);

  /* DER ANGRIFFS-BRIEF: in welchem Verfahren der Pass geflogen wird. Wird nicht in eine Box eingegeben —
   * die Feuerleitung publiziert beide Cues ohnehin, dies ist, worauf der PILOT handelt. */
  void BriefAttack(FBDeliveryMode m) { AtkMode_ = m; }
  FBDeliveryMode AttackMode() const { return AtkMode_; }

  /* DER GEGENMASSNAHMEN-BRIEF, in Form und Begruendung identisch zu BriefRelease. Eine Mission in
   * SEMI/AUTO brieft hier nichts: dort beantwortet das Flugzeug seinen eigenen Warnempfaenger. */
  static constexpr int kMaxBriefedDispenses = 8;
  bool BriefChaff(double atS);

  const char *TelemetryName() const override { return "pilot"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

  /* Zweite bzw. dritte Telemetriequelle, die der Client selbst registriert: bewusst NICHT in die
   * eigenen Kanaele dieser Klasse gefaltet, die in der Mitte jeder gemessenen telemetry.csv sitzen. */
  FBBfmTrack &BfmTrack() { return Bfm_; }
  const FBBfmTrack &BfmTrack() const { return Bfm_; }

  FBEngagement &Engagement() { return Eng_; }
  const FBEngagement &Engagement() const { return Eng_; }

  /* DIE VARIANTE: von aussen schreibgeschuetzt-einseitig — gelesen wird die Tabelle ausschliesslich
   * ueber Tuned(), im Entscheidungspfad selbst. */
  bool ApplyTuning(const std::string &key, double value) { return Tune_.Set(key, value); }

protected:
  /* g·sqrt(n²−1)/V an der Eckgeschwindigkeit. EINMAL hergeleitet, ZWEIMAL benutzt — deshalb eine
   * Methode statt zweier Konstanten: die schnellste Nasenbewegung dieses Jets UND die Annahme darueber,
   * wie hart der andere kurvt (FBTrackDatum). doc/flightbox/pilot-ai.md, Abschnitt 6.2. */
  double CornerTurnRateDegS() const;

  /* Der EINE Leser des Entscheidungspfads: der Wert der Variante, sonst die eigene Zahl des Aufrufers. */
  double Tuned(FBPilotParam p, double own) const { return Tune_.Or(p, own); }

  /* Die Zahlen der ZELLE — generische Platzhalter, FBF16Pilot ueberschreibt sie. NICHT der
   * Override-Punkt: das ist Config wie FBFlightControls Gain-Preset, nur als virtual ausgedrueckt, weil
   * RotationSpeedKt das Live-Abfluggewicht braucht. */
  virtual double RotationSpeedKt(double grossWeightLbs) const { (void)grossWeightLbs; return 65.0; }
  virtual double RotationLeadKt() const { return 10.0; }      /* so weit unter Vr beginnt der Zug */
  virtual double RotationPitchDeg() const { return 8.0; }
  virtual double GearUpLimitKt() const { return 150.0; }
  virtual double ClimbSpeedKt() const { return 100.0; }
  virtual double TakeoffThrottleNorm() const { return 1.0; }

  /* Die Landung. */
  virtual double ApproachSpeedKt() const { return 90.0; }
  virtual double GlidepathAngleDeg() const { return 3.0; }
  virtual double ApproachSpeedbrakeNorm() const { return 0.5; }
  virtual double FlareStartAglFt() const { return 50.0; }
  virtual double FlareTargetPitchDeg() const { return 8.0; }
  virtual double AerobrakePitchDeg() const { return 10.0; }     /* mit Marge unter dem 15°-Lagen-K.O.
                                                                   des Flugmonitors */
  virtual double AerobrakeSpeedKt() const { return 100.0; }     /* darunter Nase runter */
  virtual double RolloutBrakeNorm() const { return 0.8; }

  /* BFM-ENERGIE: CornerSpeedKt ist die Geschwindigkeit, auf die der Pilot HIN regelt; das verfuegbare g
   * ist CornerG · (V/Vcorner)² (Auftrieb waechst mit dem Staudruck), gedeckelt bei MaxG — der geforderte
   * Zug ist damit immer einer, den der Jet erzeugen kann. Unter MinSpeedKt ist er energielos: Zug auf
   * UnloadG, Auftriebsvektor unter das Ziel (Hoehe gegen Fahrt). */
  virtual double BfmCornerSpeedKt() const { return 300.0; }
  virtual double BfmCornerG() const { return 4.0; }
  virtual double BfmMaxG() const { return 6.0; }
  virtual double BfmMinSpeedKt() const { return 220.0; }
  virtual double BfmUnloadG() const { return 3.0; }
  /* GEOMETRIE. Die Kontrollposition: hinter ihm, nah, Nase drauf, Lock gehalten. */
  virtual double BfmControlMinNm() const { return 0.5; }
  virtual double BfmControlMaxNm() const { return 1.5; }
  virtual double BfmControlAspectDeg() const { return 30.0; }
  virtual double BfmControlAtaDeg() const { return 30.0; }
  /* VERFOLGUNGSART aus der Geometrie: Lag, wenn der Overshoot die Nase vorbeiwerfen wuerde; Lead,
   * solange die Winkel das Problem sind; Pure dazwischen. */
  virtual double BfmClosureGainKtPerNm() const { return 120.0; }   /* Steigung des Closure-FAHRPLANS */
  virtual double BfmMaxClosureKt() const { return 200.0; }
  /* Wie hart die Zelle mit allem, was sie hat, verzoegern kann. Das BEGRENZT den Fahrplan darueber: ein
   * Overshoot jenseits BrakeMs2/Steigung ist einer, den der Jet vor der Kontrollentfernung nicht mehr
   * abbaut. Eine GEMESSENE Zelleneigenschaft wie die Corner-Zahlen, kein Stellknopf. */
  virtual double BfmBrakeMs2() const { return 1.5; }
  virtual double BfmLeadAspectDeg() const { return 45.0; }
  virtual double BfmLeadRangeNm() const { return 3.0; }
  virtual double BfmLeadMaxS() const { return 4.0; }     /* Deckel auf die Kollisionskurs-Vorhaltezeit */
  virtual double BfmLagTimeS() const { return 2.5; }     /* wie weit hinter ihm der Lag-Punkt sitzt */
  virtual double BfmYoYoHeightM() const { return 400.0; }/* Hoehe des High Yo-Yo bei vollem Ueberschuss */
  /* SUCHE. Wie lange ein auslaufender Track mit der Nase allein verfolgt wird, bevor das Weben beginnt. */
  virtual double BfmScanAfterS() const { return 3.0; }
  /* Das Weben muss SANFT bleiben: seine eigene Kursrate 2π·A/T ist ein Lenkfehler wie jeder andere —
   * ein schnelles, weites Muster liesse den Piloten seiner eigenen Suche hinterherkurven statt schauen. */
  virtual double BfmScanAmplitudeDeg() const { return 8.0; }
  virtual double BfmScanPeriodS() const { return 30.0; }
  virtual double BfmFloorFt() const { return 2000.0; }   /* AGL, darunter wird der Zug nach oben gezogen */
  /* Die Laenge EINES Abzugsdrucks. Der Bus erzwingt 0,5 s zwischen zwei Handlungen an einem Schalter,
   * also ist genau diese Laenge Dauerfeuer, solange der Trichter haelt. */
  virtual double BfmGunBurstS() const { return 0.5; }
  /* Die Grenze zwischen Trichter fliegen und Verfolgungsgesetz fliegen. */
  virtual double BfmGunTrackMaxErrDeg() const { return 20.0; }
  /* Anteil der Trichtertoleranz, auf dem der Pilot vor dem Abdruecken besteht: 0,35 legt das Muster in
   * den Rumpf statt irgendwo ueber die Spannweite. */
  virtual double BfmGunFireTolFrac() const { return 0.35; }

  /* Die BVR-Zahlen. Der Suchmodus ist ein MODUL-Ordinal, die generische Schicht kann ihn also nicht
   * benennen, nur anfordern; -1 = „dieses Modul hat keinen eigenen nicht-lockenden Suchmodus". */
  virtual int    SearchRadarModeOrdinal() const { return -1; }
  virtual double InterceptSpeedKt() const { return 300.0; }
  /* WO DER LOCK AUSGEGEBEN WIRD: weit genug draussen, dass der Single-Target-Track steht und die
   * Feuerleitung eine DLZ hat; nah genug, dass die Warnung dem Ziel keine halbe Minute schenkt. */
  virtual double InterceptLockRangeNm() const { return 20.0; }
  /* Der Faktor ist ein Anteil einer Zahl, die die Feuerleitung JE SCHUSS rechnet, keine eigene
   * Entfernung — er bleibt damit richtig, wenn sich die Geometrie aendert. */
  virtual double InterceptShotRtrFactor() const { return 1.0; }
  virtual double InterceptShotAtaDeg() const { return 30.0; }
  virtual double InterceptShotSpacingS() const { return 12.0; }
  /* Der CRANK IST der Kardanwinkel der Antenne minus der Reserve, die ein manoevrierendes Ziel braucht:
   * weiter wegdrehen bricht den Track, und genau das darf die Support-Phase nicht. */
  virtual double InterceptCrankAtaDeg() const { return 45.0; }
  /* Darunter ist das Gefecht nicht mehr BVR: weiterdruecken ist ein Merge, kein Schuss. */
  virtual double InterceptAbortRangeNm() const { return 5.0; }

  /* Die Luft-Boden-Zahlen. WOHIN die Waffe geht, ist Sache der Feuerleitung; dies sind nur die Haende
   * des Piloten und sein Weg hinaus. Die VORSPANNUNG ist das Messinstrument der Phase: ein Abwurf `bias`
   * Sekunden nach dem Cue landet je Sekunde eine Grundgeschwindigkeit zu weit. */
  virtual double AttackReleaseBiasS() const { return 0.0; }
  /* Nur die QUER-Haelfte des Zielfehlers: die Laengshaelfte ist das, worum es im Cue selbst geht. Das
   * ist das Urteil, das ein CCRP-Countdown nicht faellen kann. */
  virtual double AttackCcipTolM() const { return 60.0; }
  /* Der Weg hinaus. */
  virtual double AttackEgressTurnDeg() const { return 120.0; }
  virtual double AttackEgressClimbM() const { return 500.0; }
  virtual double AttackEgressRangeM() const { return 12000.0; }
  virtual double AttackEgressS() const { return 25.0; }
  /* Die Verteidigung: 90° = reiner Beam, dort ist die eigene Radialgeschwindigkeit null und ein
   * Puls-Doppler-Set kann Flugzeug und Chaff-Wolke nicht trennen. */
  virtual double InterceptBeamOffsetDeg() const { return 90.0; }
  virtual double InterceptChaffIntervalS() const { return 3.0; }
  /* Eine Beam-Bewegung, die beim Erloeschen des Symbols abgebrochen wird, kam nie aus dem Notch heraus —
   * der Empfaenger verstummt ja GERADE WEIL das Manoever wirkte. */
  virtual double InterceptDefendHoldS() const { return 12.0; }

private:
  /* Ein Entscheidungstakt Cockpitarbeit: setzt den naechsten noch offenen Brief-Eintrag ab. Eine
   * BUS-Ablehnung (Kanal belegt, Manoever-Sperre) laesst ihn offen und wird wiederholt; was die Box
   * erreicht hat, ist endgueltig — ein Pilot tippt eine abgelehnte Eingabe nicht ewig neu. */
  void EnterBriefedItems(FBCommandBus &avionics);

  void ReleaseBriefedStores(FBCommandBus &avionics);
  void DispenseBriefedCm(FBCommandBus &avionics);

  /* Der Rumpf der Bfm-Phase: die einzige Phase mit EIGENEM Regelgesetz statt eines Autopilot-Ziels. */
  FBPilotCommands BfmCommands(const FBState &state, FBCommandBus &avionics, const Fdm::fb_fdm_state &st,
                              double dt);
  /* Der Abzugsfinger, getrennt vom Fliegen: eine Waffe zu bedienen ist kein Steuern. */
  void BfmGunfire(const FBState &state, FBCommandBus &avionics);
  FBPilotCommands AttackCommands(const FBState &state, FBCommandBus &avionics, const Fdm::fb_fdm_state &st,
                                 const FBFlightPlan &plan);
  FBPilotCommands InterceptCommands(const FBState &state, FBCommandBus &avionics,
                                    const Fdm::fb_fdm_state &st, const FBFlightPlan &plan, double dt);
  /* EINE Bedienhandlung je Entscheidungstakt, in Prioritaetsreihenfolge: die defensiven zuerst, denn
   * wer beschossen wird, editiert keinen Radarmodus. */
  bool InterceptCockpit(const FBState &state, FBCommandBus &avionics, int designateTrack, bool wantShot,
                        bool wantChaff, double wantElDeg);
  /* Steckt in diesem Jet noch ein Kampf? Waffen, Sprit, ein strahlendes Radar — fehlt eines davon, ist
   * Umdrehen kein Mut, sondern ein Jet ohne Auftrag im Startbereich eines anderen. Liest nur Bloecke. */
  bool CanPressOn(const FBState &state) const;
  /* Der Versatz des Such-Webens, verankert am Suchbeginn statt an der Missionsuhr, und auf die
   * Unsicherheitsbreite des Datums geweitet. */
  double SearchWeaveDeg(const FBTrackDatum &datum, bool searching);
  void Transition(Phase p) { CurPhase = p; PhaseElapsedS = 0.0; RolloutNoseDown_ = false; }

  /* DIE EINLAUFENDE BAHN des aktiven Wegpunkts, falls die Mission eine deklariert hat. Ein Bein sind
   * zwei DEKLARIERTE Fixe und sonst nichts: zum ERSTEN Wegpunkt gibt es keine Bahn, und eine aus der
   * Spawn-Position erfundene machte aus einem kreisenden Verteidiger einen, der einmal hinfliegt. */
  static void SetLegFromPlan(FBPilotCommands &c, const FBFlightPlan &plan);

  /* Runway-relatives along/across (m), DIESELBE Achsenkonvention wie FBMissionMonitor::OnRunway und
   * FBAutopilot::SetCourse — geteilt von Start und Rollout. */
  static void RunwayAxis(const FBRunway &rwy, double lat, double lon, double &alongM, double &acrossM);
  double NosewheelSteerCmd(const FBRunway &rwy, double lat, double lon, double yawDeg) const;
  double PitchHoldStick(double targetDeg, double pitchDeg, double qDegS, double stickMax) const;

  Phase CurPhase = Phase::Idle;
  double PhaseElapsedS = 0.0;
  bool RolloutNoseDown_ = false;  /* gelatcht: die Zwei-Punkt-Lage ist vorbei, Radbremsen frei */
  int ActiveWpCache = -1;         /* Telemetriecache, in Run() gesetzt */
  double DistToWpCache = -1.0;

  FBBfmTrack Bfm_;                /* Bild + Scoreboard des Kampfes; ausserhalb der Bfm-Phase inert */
  FBEngagement Eng_;              /* Zustand + Debriefing des Abfangs; ausserhalb inert */
  FBPilotTuning Tune_;            /* die Variante dieses Piloten — leer, wenn die Mission nichts setzte */

  /* Das eigene Gedaechtnis der Intercept-Phase (FBEngagement haelt das PROTOKOLL, dies ist der Zustand,
   * aus dem entschieden wird). */
  FBEngageState EngState_ = FBEngageState::Idle;
  int    IntTrack_ = 0;             /* bearbeitete Kontaktnummer, 0 = keine */
  double IntBriefAltM_ = 0.0;       /* das Hoehenband, das die Suche abdeckt */
  double IntBriefHdgDeg_ = 0.0;     /* der Vektor, solange nichts auf dem Scope ist */
  bool   IntAnchored_ = false;
  double IntLastActionS_ = -1e9;    /* eine Bedienhandlung nach der anderen */
  double IntLastShotS_ = -1e9;
  double IntNextShotS_ = -1e9;      /* nicht bevor die Runde in der Luft ihre Chance hatte */
  double IntLastChaffS_ = -1e9;
  double IntLockSinceS_ = -1.0;     /* seit wann der aktuelle Single-Target-Track steht */
  double IntCmdElDeg_ = 0.0;        /* die vom Piloten angeforderte Antennenhoehe */
  bool   IntCmdMode_ = false;
  int    IntSeenReleases_ = 0;      /* der Abwurfzaehler des Stores-Blocks, um einen Start zu bemerken */
  int    IntSeenChaff_ = 0;
  double IntThreatLastS_ = -1e9;    /* letzter Tick, in dem eine Warnung eine Antwort verlangte */
  double IntDefendCueS_ = -1.0;     /* seit wann sie das tut — die Null der Reaktionszeit */
  double IntCrankSign_ = 1.0;       /* Richtung der Crank-Drehung, je Support-Eintritt einmal entschieden */
  bool   IntHaveCrankSign_ = false;
  double TimeS_ = 0.0;            /* die eigene Uhr des Piloten; der Tracker stempelt Looks absolut */
  double BfmGIterm_ = 0.0;
  double BfmRollCmdPrev_ = 0.0;
  /* Der festgelegte Drehsinn, solange das Ziel hinter der Fluegellinie steht. */
  int    BfmTurnSense_ = 0;
  double BfmSearchHdgDeg_ = 0.0;  /* verankerter Kurs + Hoehe der kalten Suche */
  double BfmSearchAltM_ = 0.0;
  bool   BfmSearchAnchored_ = false;
  double ScanSinceS_ = 0.0;       /* Beginn des aktuellen Scans — die Phasennull des Webens */
  bool   ScanRunning_ = false;

  FBDeliveryMode AtkMode_ = FBDeliveryMode::Ccip;
  bool   AtkReleased_ = false;    /* der Pass ist verbraucht: ein Pickle je deklariertem Angriff */
  bool   AtkInRangeSeen_ = false;
  /* WOVON DER PASS BEGONNEN WURDE — der Bahnursprung des Anflugs. Die Mission deklariert keinen
   * Initialpunkt, also IST der Punkt, an dem der Anflug begann, der Initialpunkt. */
  bool   AtkHaveRunIn_ = false;
  double AtkRunInLatDeg_ = 0.0, AtkRunInLonDeg_ = 0.0;
  double AtkEgressUntilS_ = 0.0;  /* die eigene Uhr der Ausweichkurve, absolut */
  double AtkEgressLatDeg_ = 0.0, AtkEgressLonDeg_ = 0.0, AtkEgressAltM_ = 0.0;

  double GunNextS_ = -1e9;        /* nicht bevor der kommandierte Feuerstoss beendet ist */
  bool   GunTracking_ = false;    /* fliegt gerade den Trichter — beim Uebergang geloggt */
  double GunPrevErrDeg_ = 0.0, GunPrevS_ = 0.0;
  bool   GunHaveErr_ = false;
  /* Der Ratenanteil der Kanonen-Nachfuehrung: die geforderte Rohrrichtung als WELT-Richtung und deren
   * eigene Drehrate. Weltbezogen mit Absicht — koerperbezogen maesse man den eigenen Zug, nicht die
   * Bewegung der Loesung. */
  double GunLeadPrevE_ = 0.0, GunLeadPrevN_ = 0.0, GunLeadPrevU_ = 0.0, GunLeadPrevS_ = 0.0;
  double GunLeadRateE_ = 0.0, GunLeadRateN_ = 0.0, GunLeadRateU_ = 0.0;
  double GunTrackIAz_ = 0.0, GunTrackIEl_ = 0.0;   /* Integrator der Nachfuehrung, je Achse */
  bool   GunHaveLead_ = false;

  /* Die gebrieften Abwuerfe, in Brief-Reihenfolge von vorn verbraucht — das Array wird nie umgeschrieben,
   * die Folge ist exakt die deklarierte. */
  double ReleaseAtS_[kMaxBriefedReleases]{};
  int    ReleaseCount_ = 0, ReleaseNext_ = 0;
  double DispenseAtS_[kMaxBriefedDispenses]{};
  int    DispenseCount_ = 0, DispenseNext_ = 0;

  double BriefAlowFt_ = 0.0, BriefBingoLbs_ = 0.0, BriefWeapon_ = 0.0;
  bool   BriefArm_ = true;
  bool   BriefAlowPending_ = false, BriefBingoPending_ = false;
  bool   BriefArmPending_ = false, BriefWeaponPending_ = false;
  double BriefNextTryS_ = 0.0;
};

} // namespace FlightBox::Pilot
#endif
