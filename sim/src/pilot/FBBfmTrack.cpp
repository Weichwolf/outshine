#include "FBBfmTrack.h"
#include "FBGeodesy.h"
#include "FBUnits.h"
#include <cmath>

namespace FlightBox::Pilot {

namespace {
const double kG0 = 9.80665;   /* das g der Energiehoehe, kein Lastvielfaches */
} // namespace

const char *FBBfmPursuitStr(FBBfmPursuit p) {
  switch (p) {
    case FBBfmPursuit::None: return "none";
    case FBBfmPursuit::Search: return "search";
    case FBBfmPursuit::Lead: return "lead";
    case FBBfmPursuit::Pure: return "pure";
    case FBBfmPursuit::Lag: return "lag";
  }
  return "?";
}

void FBBfmTrack::Reset() {
  Blk_ = FBBfmBlock{};
  Have_ = false;
  LastLookS_ = -1e9;
  VelE_ = VelN_ = VelU_ = 0.0;
  Pursuit_ = FBBfmPursuit::None;
  InControl_ = false;
  GCmd_ = 0.0;
}

void FBBfmTrack::Update(const FBState &state, const Fdm::fb_fdm_state &own, double nowS) {
  const FBRadarBlock &fcr = state.Radar;
  const FBRadarContact *c = nullptr;
  /* Kopf zuerst: ein Invalid Radar-Block ist ein Geraet, das nicht schaut — sein Kontaktarray bedeutet
   * nichts. */
  if (fcr.H.Readable() && fcr.LockIndex >= 0 && fcr.LockIndex < fcr.ContactCount)
    c = &fcr.Contacts[fcr.LockIndex];

  if (c) {
    /* Ein zwischen zwei Looks erneut gelesener Kontakt traegt DIESELBE eingefrorene Geometrie — ihn zu
     * differenzieren schoebe eine Null in den Geschwindigkeitsfilter. */
    double lookS = nowS - c->LookAgeS;
    if (lookS > LastLookS_ + kMinLookDtS || !Have_) {
      /* Der Echo-Ort ist WELT-referenziert (Peilung + Elevationswinkel), bewusst nicht Koerper-Az/El:
       * letztere wurden gegen die Lage ZUM LOOKZEITPUNKT gemessen und schmierten die Eigenbewegung eines
       * rollenden Jets in die geschaetzte Zielgeschwindigkeit. */
      double brg = c->BearingDeg * kDeg2Rad, elw = c->ElevAngleDeg * kDeg2Rad;
      double horiz = c->RangeM * std::cos(elw);
      double e = horiz * std::sin(brg), n = horiz * std::cos(brg), u = c->RangeM * std::sin(elw);
      double coslat = std::cos(own.lat * kDeg2Rad);
      double lat = own.lat + n / kMPerDeg;
      double lon = own.lon + e / (kMPerDeg * (std::fabs(coslat) > 1e-6 ? coslat : 1e-6));
      double alt = own.elev + u;

      if (Have_) {
        double dtLook = lookS - LastLookS_;
        if (dtLook >= kMinLookDtS) {
          double de, dn;
          FBEnuOffsetM(PosLatDeg_, PosLonDeg_, lat, lon, de, dn);
          VelE_ += kVelAlpha * (de / dtLook - VelE_);
          VelN_ += kVelAlpha * (dn / dtLook - VelN_);
          VelU_ += kVelAlpha * ((alt - PosAltM_) / dtLook - VelU_);
        }
      }
      PosLatDeg_ = lat; PosLonDeg_ = lon; PosAltM_ = alt;
      LastLookS_ = lookS;
      Have_ = true;
    }
  }

  Predict(own, nowS);
  /* Annaeherungsrate vom Radar, SOLANGE es hinsieht; im Coast aus der Schaetzung selbst (Predict). Die
   * eingefrorene letzte Messung waere schlimmer als nutzlos: ein Merge endet mit mehreren hundert Knoten
   * im Protokoll, und wer die noch liest, fliegt einen laengst beendeten Overshoot weiter. */
  if (c) Blk_.ClosureMs = c->ClosureMs;
  Blk_.Locked = c != nullptr;

  /* Energiehoehe: die einzige Energiezahl, die ein Pilot von EIGENEN Instrumenten ablesen kann. */
  EsFt_ = (own.elev + own.speed * own.speed / (2.0 * kG0)) * kMToFt;
}

/* Laeuft JEDEN Tick, frischer Look oder nicht: mit Lock ist das Vorhersageintervall ein Radar-Frame und
 * dies schlicht die Messung, ohne Lock ist derselbe Code die Extrapolation. */
void FBBfmTrack::Predict(const Fdm::fb_fdm_state &own, double nowS) {
  if (!Have_) { Blk_ = FBBfmBlock{}; AgeS_ = 0.0; return; }
  double age = nowS - LastLookS_;
  /* Jenseits des Fensters faellt die Position auf die zuletzt GEMESSENE zurueck und der Block wird Held —
   * die Geometrie wird weiter gemeldet (dort startet die Suche), aber sie ist nicht mehr Valid. */
  bool extrapolate = age <= kMaxExtrapolateS;
  double prop = extrapolate ? age : 0.0;

  double coslat = std::cos(PosLatDeg_ * kDeg2Rad);
  double lat = PosLatDeg_ + VelN_ * prop / kMPerDeg;
  double lon = PosLonDeg_ + VelE_ * prop / (kMPerDeg * (std::fabs(coslat) > 1e-6 ? coslat : 1e-6));
  double alt = PosAltM_ + VelU_ * prop;

  double e, n;
  FBEnuOffsetM(own.lat, own.lon, lat, lon, e, n);
  double u = alt - own.elev;
  Blk_.EastM = e; Blk_.NorthM = n; Blk_.UpM = u;
  Blk_.VelE = VelE_; Blk_.VelN = VelN_; Blk_.VelU = VelU_;
  Blk_.RangeM = std::sqrt(e * e + n * n + u * u);
  FBEnuToBodyLos(own.roll, own.pitch, own.yaw, e, n, u, Blk_.AzDeg, Blk_.ElDeg);
  AgeS_ = age;
  /* Der Stempel ist der LOOK, nicht `now`: Alter am Kopf ist Alter, seit der Sensor ihn sah. */
  Blk_.H.StampS = LastLookS_;
  Blk_.H.Status = extrapolate ? FBBlockStatus::Valid : FBBlockStatus::Held;

  /* Annaeherungsrate aus der Schaetzung; wird bei gehaltenem Lock von der Radarmessung ueberschrieben. */
  double ownE = own.vx, ownN = -own.vz, ownU = own.vy;
  double relE = VelE_ - ownE, relN = VelN_ - ownN, relU = VelU_ - ownU;
  Blk_.ClosureMs = -(e * relE + n * relN + u * relU) / std::fmax(Blk_.RangeM, 1.0);

  /* Aspekt: der Winkel AM ZIEL zwischen seinem Heck und der Sichtlinie zu uns, also cos(aspect) = T·L.
   * Unterhalb kMinTrackSpeedMs undefiniert — dann bleibt der letzte Wert stehen. */
  double vh = std::sqrt(VelE_ * VelE_ + VelN_ * VelN_ + VelU_ * VelU_);
  if (vh > kMinTrackSpeedMs && Blk_.RangeM > 1.0) {
    double dot = (VelE_ * e + VelN_ * n + VelU_ * u) / (vh * Blk_.RangeM);
    dot = dot < -1.0 ? -1.0 : dot > 1.0 ? 1.0 : dot;
    Blk_.AspectDeg = std::acos(dot) * kRad2Deg;
    double tgtHdg = std::atan2(VelE_, VelN_) * kRad2Deg;
    Blk_.HcaDeg = FBWrap180(own.yaw - tgtHdg);
  }
}

/* Bewusst NICHT aus Block() abgeleitet: der Block friert jenseits des Fensters auf dem Messpunkt ein
 * (richtig fuer die Verfolgung, falsch fuer die Suche), also eigene Fortschreibungsregel. */
FBTrackDatum FBBfmTrack::Datum(const Fdm::fb_fdm_state &own, double nowS, double turnRateDegS) const {
  FBTrackDatum d;
  if (!Have_) return d;
  d.Valid = true;
  d.AgeS = std::fmax(nowS - LastLookS_, 0.0);

  double v = std::sqrt(VelE_ * VelE_ + VelN_ * VelN_ + VelU_ * VelU_);
  double w = std::fmax(turnRateDegS, 1.0) * kDeg2Rad;
  double tProp = std::fmin(d.AgeS, 2.0 / w);   /* jenseits 2/w koennte die Kurve ueberall hingegangen sein */

  double coslat = std::cos(PosLatDeg_ * kDeg2Rad);
  double lat = PosLatDeg_ + VelN_ * tProp / kMPerDeg;
  double lon = PosLonDeg_ + VelE_ * tProp / (kMPerDeg * (std::fabs(coslat) > 1e-6 ? coslat : 1e-6));
  d.AltM = PosAltM_ + VelU_ * tProp;

  double e, n;
  FBEnuOffsetM(own.lat, own.lon, lat, lon, e, n);
  d.EastM = e; d.NorthM = n; d.UpM = d.AltM - own.elev;
  double horiz = std::sqrt(e * e + n * n);
  d.RangeM = std::sqrt(horiz * horiz + d.UpM * d.UpM);
  d.BearingDeg = std::atan2(e, n) * kRad2Deg;
  d.RadiusM = std::fmin(0.5 * v * w * d.AgeS * d.AgeS, v * d.AgeS);
  d.HalfWidthDeg = std::atan2(d.RadiusM, std::fmax(horiz, 1.0)) * kRad2Deg;
  return d;
}

void FBBfmTrack::Report(FBBfmPursuit pursuit, bool inControl, double gCmd, double dt) {
  Pursuit_ = pursuit;
  InControl_ = inControl;
  GCmd_ = gCmd;
  EngagedS_ += dt;
  if (Blk_.Locked) LockS_ += dt;
  if (inControl) ControlS_ += dt;
}

void FBBfmTrack::DeclareTelemetry(FBTelemetrySchema &schema) const {
  schema.Add("bfm_pursuit");
  schema.Add("bfm_valid");
  schema.Add("bfm_locked");
  schema.Add("bfm_age", "s");
  schema.Add("bfm_rng", "nm");
  schema.Add("bfm_ata", "deg");
  schema.Add("bfm_aspect", "deg");
  schema.Add("bfm_hca", "deg");
  schema.Add("bfm_clos", "kt");
  schema.Add("bfm_es", "ft");
  schema.Add("bfm_gcmd", "g");
  schema.Add("bfm_ctrl");
  schema.Add("bfm_engaged", "s");
  schema.Add("bfm_lock_s", "s");
  schema.Add("bfm_ctrl_s", "s");
}

void FBBfmTrack::SampleTelemetry(FBTelemetryRow &row) const {
  bool valid = Blk_.H.IsValid();
  row.Push(std::string(FBBfmPursuitStr(Pursuit_)));
  row.Push(valid);
  row.Push(Blk_.Locked);
  row.Push(valid ? AgeS_ : -1.0);
  row.Push(valid ? Blk_.RangeM * kMToNm : -1.0);
  row.Push(valid ? Blk_.AzDeg : 0.0);
  row.Push(valid ? Blk_.AspectDeg : -1.0);
  row.Push(valid ? Blk_.HcaDeg : 0.0);
  row.Push(valid ? Blk_.ClosureMs * kMsToKt : 0.0);
  row.Push(EsFt_);
  row.Push(GCmd_);
  row.Push(InControl_);
  row.Push(EngagedS_);
  row.Push(LockS_);
  row.Push(ControlS_);
}

} // namespace FlightBox::Pilot
