#include "FBAutopilot.h"
#include "FBGeodesy.h"
#include <cmath>

namespace FlightBox {

static double Clamp(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }

/* Das Bahnfolge-Gesetz, eine Definition, zwei Nutzer (Localizer und Route-/Run-in-Leg). Kaskadiert
 * statt summiert, damit der Cap den Anfangswinkel begrenzt, den ein grosser Versatz fordern darf;
 * fuer kleine Fehler IST die Kaskade die Zweizustands-Rueckfuehrung. doc/flightbox/systems.md 2.4. */
static double TrackBankCmd(double courseDeg, double acrossM, double dirDeg, double kXtDegPerM,
                           double interceptMaxDeg, double kDir, double bankMaxDeg) {
  double intercept = Clamp(-kXtDegPerM * acrossM, -interceptMaxDeg, interceptMaxDeg);
  double dirErr = FBWrap180(courseDeg + intercept - dirDeg);
  return Clamp(kDir * dirErr, -bankMaxDeg, bankMaxDeg);
}

/* Die beiden Verstaerkungen einer Bahn sind HERGELEITET, nicht gewaehlt: zeta = 1/sqrt(2) plus die
 * Autoritaets-Aussage „Querablage-Term allein saettigt bei einem Kurvenradius Versatz" legen sie fest,
 * und k_chi faellt dabei unabhaengig von V und g heraus. doc/flightbox/systems.md, Abschnitt 2.4. */
const double kLegZeta = 0.70710678118654752;   /* 1/sqrt(2) */
const double kLegG0 = 9.80665;
/* Darunter hat der Geschwindigkeitsvektor keine regelungswuerdige Richtung -> Nase statt Bahnwinkel. */
const double kLegMinSpeedMs = 30.0;

FBAutopilot::FBAutopilot()
  : BankMaxDeg(60), KHdg(0.8), KAlt(0.08), KXt(0.08), CourseInterceptMaxDeg(45.0), ApproachVsCapMs(8.0),
    Mode(FBMode::Manual), LatDeg(0), LonDeg(0), AltM(0), SpeedMs(220),
    MRoll(0), MPitch(0), MYaw(0), MThr(0.85), CourseDeg(0), RefElevM(0), GlidepathDeg(3.0),
    HaveLeg(false), LegLatDeg(0), LegLonDeg(0), LegCourseDeg(0) {}

void FBAutopilot::SetManual(double roll, double pitch, double yaw, double thr) {
  Mode = FBMode::Manual;
  MRoll = roll; MPitch = pitch; MYaw = yaw; MThr = thr;
}

void FBAutopilot::SetDirect(double lat, double lon, double altM, double speedMs) {
  Mode = FBMode::Direct;
  LatDeg = lat; LonDeg = lon; AltM = altM; SpeedMs = speedMs;
  HaveLeg = false;
}

void FBAutopilot::SetDirectLeg(double fromLat, double fromLon, double lat, double lon, double altM,
                               double speedMs) {
  SetDirect(lat, lon, altM, speedMs);
  if (FBPlanarDistM(fromLat, fromLon, lat, lon) < kMinLegM) return;   /* keine Richtung, s. Header */
  HaveLeg = true;
  LegLatDeg = fromLat; LegLonDeg = fromLon;
  LegCourseDeg = FBBearingDeg(fromLat, fromLon, lat, lon);
}

void FBAutopilot::SetCourse(double refLat, double refLon, double courseDeg, double refElevM,
                            double glidepathDeg, double speedMs) {
  Mode = FBMode::Course;
  LatDeg = refLat; LonDeg = refLon; CourseDeg = courseDeg; RefElevM = refElevM;
  GlidepathDeg = glidepathDeg; SpeedMs = speedMs;
}

FBGuidance FBAutopilot::Run(const fb_fdm_state &s) {
  FBGuidance g{};
  g.Mode = Mode;
  g.TargetSpeedMs = SpeedMs;
  if (Mode == FBMode::Manual) {
    g.ManualRoll = MRoll; g.ManualPitch = MPitch; g.ManualYaw = MYaw; g.ManualThr = MThr;
    return g;
  }
  if (Mode == FBMode::Course) {
    /* Projektionskonvention (along=0 am Referenzpunkt, +across = rechts vom Kurs) ist dieselbe wie in
     * FBPilots Centerline-Gesetz und FBMissionMonitor::OnRunway. */
    double along, across;
    FBTrackProjectM(LatDeg, LonDeg, CourseDeg, s.lat, s.lon, along, across);
    double distToGoM = -along;   /* positiv VOR dem Referenzpunkt */
    g.BankCmdDeg = TrackBankCmd(CourseDeg, across, s.yaw, KXt, CourseInterceptMaxDeg, KHdg, BankMaxDeg);
    /* Das VS-Feedforward nimmt dem P-Term den Schleppfehler gegen das RAMPENDE Ziel (gemessen ~56 m zu
     * hoch an der Schwelle auf 9 nm Final). doc/flightbox/systems.md, Abschnitt 2.6. */
    double tanGp = std::tan(GlidepathDeg * kDeg2Rad);
    double targetAlt = RefElevM + tanGp * std::fmax(distToGoM, 0.0);
    g.AltErrM = targetAlt - s.elev;
    double vsFeedforward = -tanGp * s.gs;
    g.TargetVsMs = Clamp(vsFeedforward + KAlt * g.AltErrM, -ApproachVsCapMs, ApproachVsCapMs);
    g.RingDistM = distToGoM;
    return g;
  }
  /* Der VS-Cap unten (25 m/s) ist enger als eine Reiseflug-Korrektur braucht, weil DIRECT auch den
   * Steigflug nach dem Abheben treibt — ungecappt waeren das ~30 Grad Anstellung. */
  double n, e;
  FBEnuOffsetM(s.lat, s.lon, LatDeg, LonDeg, e, n);
  if (HaveLeg) {
    /* Gegen den BODENKURS, nicht gegen die Nase: die Querablage ist das Integral davon, wo das Flugzeug
     * tatsaechlich hinfliegt — jeder Schiebe-/Vorhaltewinkel bliebe sonst als Driftrate stehen. */
    double along, across;
    FBTrackProjectM(LegLatDeg, LegLonDeg, LegCourseDeg, s.lat, s.lon, along, across);
    double bankMaxRad = BankMaxDeg * kDeg2Rad;
    double kDir = 2.0 * kLegZeta * std::sqrt(bankMaxRad * std::tan(bankMaxRad));
    double v = std::fmax(s.gs, kLegMinSpeedMs);
    double turnRadiusM = v * v / (kLegG0 * std::tan(bankMaxRad));
    double trackDeg = s.gs > kLegMinSpeedMs ? std::atan2(s.vx, -s.vz) * kRad2Deg : s.yaw;
    g.BankCmdDeg = TrackBankCmd(LegCourseDeg, across, trackDeg, BankMaxDeg / (kDir * turnRadiusM),
                                BankMaxDeg / kDir, kDir, BankMaxDeg);
    (void)along;   /* die Laengskoordinate ist Sache der SEQUENZIERUNG (FBNavSystem), nicht des Fliegens */
  } else {
    double brg = std::atan2(e, n) * kRad2Deg;
    double hdgErr = FBWrap180(brg - s.yaw);
    g.BankCmdDeg = Clamp(KHdg * hdgErr, -BankMaxDeg, BankMaxDeg);
  }
  g.AltErrM = AltM - s.elev;
  g.TargetVsMs = Clamp(KAlt * g.AltErrM, -25.0, 25.0);
  g.RingDistM = std::hypot(n, e);   /* Diagnose, kein Regelanteil */
  return g;
}

} // namespace FlightBox
