/* FlightBox — FBAutopilot: die aeussere Guidance (MANUAL / DIRECT auf Punkt oder Bahn / COURSE) ->
 * ein Guidance-Kommando, dem die innere FBW folgt. Herleitung des Bahngesetzes, die Begruendung „ein
 * Punkt ist keine Bahn" und die Gain-Tabelle: doc/flightbox/systems.md, Abschnitt 2. */
#ifndef FBAUTOPILOT_H
#define FBAUTOPILOT_H

#include "FBFdm.h"
#include "FBMode.h"

namespace FlightBox {

struct FBGuidance {
  FBMode Mode;
  double BankCmdDeg;     /* kommandierte Schraeglage, deg */
  double AltErrM;        /* Zielhoehe - Isthoehe, m, UNGECLAMPT */
  double TargetVsMs;     /* gewuenschte Vertikalgeschwindigkeit, m/s */
  double TargetSpeedMs;  /* zu haltende Fahrt, m/s */
  double ManualRoll, ManualPitch, ManualYaw, ManualThr;   /* Durchgriff in Manual */
  double RingDistM;      /* Diagnose: Entfernung zum Direct-Zielpunkt */
};

class FBAutopilot {
public:
  FBAutopilot();
  virtual ~FBAutopilot() = default;

  void SetManual(double roll, double pitch, double yaw, double thr);

  /* DIRECT: Peilung auf (lat, lon) bei Hoehen-/Speedhaltung. */
  void SetDirect(double lat, double lon, double altM, double speedMs);

  /* DIRECT AUF EINER BAHN: dasselbe Ziel, als TRACK von (fromLat,fromLon) geflogen statt als Peilung.
   * Die Verstaerkungen sind hier keine Config, sie sind aus BankMaxDeg HERGELEITET (s. Run()). */
  void SetDirectLeg(double fromLat, double fromLon, double lat, double lon, double altM, double speedMs);

  /* COURSE: die unendliche Linie durch (refLat,refLon) auf courseDeg, auf geradem Gleitpfad nach
   * refElevM AM Referenzpunkt. Generisch darueber, was der Referenzpunkt BEDEUTET. */
  void SetCourse(double refLat, double refLon, double courseDeg, double refElevM, double glidepathDeg,
                 double speedMs);

  /* Der EINE Override-Punkt; alles andere hier ist Config, die jeder Override verbatim teilt. */
  virtual FBGuidance Run(const fb_fdm_state &s);

  FBMode GetMode(void) const { return Mode; }
  double GetTargetAlt(void) const { return AltM; }

  /* Gains — public wie ein Config-Block; Defaults = das geflogene F-16-Preset. */
  double BankMaxDeg, KHdg, KAlt;

  /* Nur COURSE: Querablage (m) -> Intercept-Winkel (deg), gecappt; VS-Cap enger als DIRECTs. */
  double KXt, CourseInterceptMaxDeg, ApproachVsCapMs;

  /* Kuerzer ist eine „Bahn" ein Koordinatenpaar und keine Richtung — faengt nur degenerierte
   * Deklarationen, zwei Missionsfixe liegen Kilometer auseinander. */
  static constexpr double kMinLegM = 100.0;

private:
  FBMode Mode;
  double LatDeg, LonDeg, AltM, SpeedMs;
  double MRoll, MPitch, MYaw, MThr;
  double CourseDeg, RefElevM, GlidepathDeg;
  bool   HaveLeg;                       /* Direct traegt einen Bahnursprung (SetDirectLeg) */
  double LegLatDeg, LegLonDeg, LegCourseDeg;
};

} // namespace FlightBox
#endif
