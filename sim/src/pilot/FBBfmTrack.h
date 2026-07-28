/* FlightBox — FBBfmTrack: das Zielbild des Piloten, gebaut aus RADARKONTAKTEN UND SONST NICHTS (kein
 * FBWorld, keine Registry, kein Datalink-Track im Include-Baum), plus das BFM-Scoreboard und
 * FBTrackDatum. doc/pilot-ai.md, Abschnitt 6. */
#ifndef FBBFMTRACK_H
#define FBBFMTRACK_H

#include "FBFdm.h"
#include "FBState.h"
#include "FBTelemetry.h"

namespace FlightBox::Pilot {

/* Was der Pilot diesen Tick mit dem Bild gemacht hat. Telemetrie-Ordinal — anhaengen, nie umsortieren. */
enum class FBBfmPursuit { None, Search, Lead, Pure, Lag };
const char *FBBfmPursuitStr(FBBfmPursuit p);

/* DAS DATUM: wo ein nicht mehr gesehenes Ziel jetzt sein KANN, und wie gross „kann" geworden ist. Der
 * Block beantwortet „wo ist er" und friert jenseits des Fensters ein — richtig fuer die VERFOLGUNG,
 * nutzlos fuer die SUCHE, die einen Punkt UND eine Breite braucht. Radius = min(0,5·V·ω·t², V·t), die
 * Haelften kreuzen bei t = 2/ω. Vollstaendige Herleitung: doc/pilot-ai.md, Abschnitt 6.2. */
struct FBTrackDatum {
  bool   Valid = false;         /* irgendwann wurde etwas gemessen; sonst gibt es nichts zurueckzugehen */
  double AgeS = 0.0;            /* seit dem LOOK, auf dem die Schaetzung steht */
  double EastM = 0.0, NorthM = 0.0, UpM = 0.0;   /* eigenrelativer Versatz des Gebietsmittelpunkts */
  double RangeM = 0.0;
  double BearingDeg = 0.0;
  double AltM = 0.0;            /* Hoehe des Mittelpunkts, m ASL */
  double RadiusM = 0.0;         /* wie gross das Gebiet geworden ist */
  double HalfWidthDeg = 0.0;    /* ...als Winkel von hier aus */
};

class FBBfmTrack : public FBTelemetrySource {
public:
  /* Geschwindigkeit wird zwischen AUFEINANDERFOLGENDEN Looks differenziert und gefiltert, nicht ueber
   * eine laengere Basislinie: gegen ein kurvendes Ziel schlaegt Nachlauf das Rauschen (gemessen: 1,8°
   * Richtungsfehler gegen 5,4° bei halbsekuendiger Basislinie). */
  static constexpr double kVelAlpha = 0.25;
  static constexpr double kMinLookDtS = 0.05;      /* darunter sind zwei Looks numerisch derselbe Look */
  /* Danach faellt die Schaetzung auf die zuletzt GEMESSENE Position zurueck: nach acht Sekunden
   * Geradeaus-Vorhersage liegt ein kurvender Jaeger einen Grossteil eines Kurvendurchmessers daneben. */
  static constexpr double kMaxExtrapolateS = 8.0;
  static constexpr double kMinTrackSpeedMs = 20.0; /* darunter hat ein Geschwindigkeitsvektor keinen Kurs */

  /* Liest NUR den GELOCKTEN Kontakt — ein ungelockter Suchrueckstrahler ist eine Detektion, kein Ziel,
   * auf das sich der Pilot festgelegt hat; beides zu mischen liesse die Verfolgung springen. */
  void Update(const FBState &state, const Fdm::fb_fdm_state &own, double nowS);

  /* Das Urteil des Piloten ueber diesen Tick, fuer das Scoreboard zurueckgemeldet (die Schwellen gehoeren
   * dem Piloten — sie sind zellenspezifisch, nicht Sache dieses Containers). */
  void Report(FBBfmPursuit pursuit, bool inControl, double gCmd, double dt);

  void Reset();

  /* `turnRateDegS` ist die Annahme, wie hart das Ziel kurven kann — die Zahl der EIGENEN Zelle, denn
   * diese Klasse kennt keine. */
  FBTrackDatum Datum(const Fdm::fb_fdm_state &own, double nowS, double turnRateDegS) const;

  const FBBfmBlock &Block() const { return Blk_; }
  bool Engaged() const { return EngagedS_ > 0.0; }
  double LockHeldFrac() const { return EngagedS_ > 0.0 ? LockS_ / EngagedS_ : 0.0; }
  double ControlS() const { return ControlS_; }

  const char *TelemetryName() const override { return "bfm"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

private:
  void Predict(const Fdm::fb_fdm_state &own, double nowS);

  FBBfmBlock Blk_{};
  bool   Have_ = false;          /* mindestens ein Look ist eingeflossen */
  double LastLookS_ = -1e9;      /* Simzeit des Looks, auf dem die Schaetzung steht */
  double PosLatDeg_ = 0.0, PosLonDeg_ = 0.0, PosAltM_ = 0.0;   /* geschaetzte Zielposition */
  double VelE_ = 0.0, VelN_ = 0.0, VelU_ = 0.0;

  FBBfmPursuit Pursuit_ = FBBfmPursuit::None;
  bool   InControl_ = false;
  double GCmd_ = 0.0;
  double AgeS_ = 0.0;            /* Telemetriekopie des Kopfalters (SampleTelemetry hat keine Uhr) */
  double EsFt_ = 0.0;
  double EngagedS_ = 0.0, LockS_ = 0.0, ControlS_ = 0.0;
};

} // namespace FlightBox::Pilot
#endif
