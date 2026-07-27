#include "FBBfmTrack.h"
#include "FBGeodesy.h"
#include "FBUnits.h"
#include <cmath>

namespace FlightBox {

namespace {
const double kG0 = 9.80665;   /* the g in the energy height, not a load factor */
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

void FBBfmTrack::Update(const FBState &state, const fb_fdm_state &own, double nowS) {
  const FBRadarBlock &fcr = state.Radar;
  const FBRadarContact *c = nullptr;
  /* Head first: an Invalid radar block is a set that is not looking, and its contact array means
   * nothing — reading it would be exactly the stale-picture bug the head exists to prevent. */
  if (fcr.H.Readable() && fcr.LockIndex >= 0 && fcr.LockIndex < fcr.ContactCount)
    c = &fcr.Contacts[fcr.LockIndex];

  if (c) {
    /* The look this contact stands on. A contact re-read between looks carries the SAME frozen geometry,
     * so differencing it would inject a zero into the velocity filter — the age field is what separates
     * a new measurement from a re-read (systems/FBRadarSystem: geometry freezes, only LookAgeS moves). */
    double lookS = nowS - c->LookAgeS;
    if (lookS > LastLookS_ + kMinLookDtS || !Have_) {
      /* Place the echo: own position + slant range along the WORLD-referenced direction the set
       * reports (bearing + elevation angle). Deliberately not the body az/el — those were measured
       * against the attitude at look time, and applying them to the attitude NOW would smear a rolling
       * jet's own motion into the target's estimated velocity. */
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
  /* While the set is actually looking at him, the CLOSURE is the radar's own measurement (a pulse-Doppler
   * quantity, and the one FBRadarContact carries). While it is not, the frozen last measurement is worse
   * than useless — it is actively misleading: a merge ends with several hundred knots of closure on
   * record, and a pilot still reading that number would keep flying an overshoot that is long over
   * instead of turning back in. Coasting therefore takes the range rate from the estimate itself. */
  if (c) Blk_.ClosureMs = c->ClosureMs;
  Blk_.Locked = c != nullptr;

  /* Own specific energy — energy height, the one energy number a pilot can read off his own instruments
   * (altitude + true airspeed). It is what makes "did he bleed himself dry winning those angles?"
   * answerable without knowing anything about the other jet. */
  EsFt_ = (own.elev + own.speed * own.speed / (2.0 * kG0)) * kMToFt;
}

/* The estimate, propagated to NOW and expressed as the geometry the pilot steers on. Runs every tick,
 * fresh look or not: while the lock is held the prediction interval is one radar frame and this is
 * simply the measurement; while it is lost the same code keeps the target moving on its last known
 * vector, which IS the extrapolation the sensor limitation demands. */
void FBBfmTrack::Predict(const fb_fdm_state &own, double nowS) {
  if (!Have_) { Blk_ = FBBfmBlock{}; AgeS_ = 0.0; return; }
  double age = nowS - LastLookS_;
  /* Past kMaxExtrapolateS the prediction has outlived its own credibility: propagating a velocity that
   * old would send the pilot chasing a point in empty sky, and the honest datum is the last position
   * actually MEASURED. The geometry keeps being reported (Have) — a pilot who has lost the lock still
   * knows where he last saw him, and that is where a search starts — but it stops being Valid, which is
   * what tells the pilot to search rather than pursue. */
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
  /* The stamp is the LOOK, not now: age asked of the head is age since the sensor saw him. Past the
   * credible extrapolation window the block freezes rather than blanking — Held (class banner). */
  Blk_.H.StampS = LastLookS_;
  Blk_.H.Status = extrapolate ? FBBlockStatus::Valid : FBBlockStatus::Held;

  /* Range rate from the estimate + own velocity (fb_fdm_state's X-Plane-local axes: +x east, +y up,
   * +z south). Overwritten by the radar's own measurement while locked — see Update(). */
  double ownE = own.vx, ownN = -own.vz, ownU = own.vy;
  double relE = VelE_ - ownE, relN = VelN_ - ownN, relU = VelU_ - ownU;
  Blk_.ClosureMs = -(e * relE + n * relN + u * relU) / std::fmax(Blk_.RangeM, 1.0);

  /* Aspect: the angle AT THE TARGET between its tail and the line of sight to us — 0 means the pursuer
   * sits directly behind him. With L the unit vector own->target and T his unit velocity, the vector
   * target->us is -L and his tail points along -T, so cos(aspect) = (-T).(-L) = T.L. Undefined while his
   * estimated speed is too small to define a direction; the last value then simply stands. */
  double vh = std::sqrt(VelE_ * VelE_ + VelN_ * VelN_ + VelU_ * VelU_);
  if (vh > kMinTrackSpeedMs && Blk_.RangeM > 1.0) {
    double dot = (VelE_ * e + VelN_ * n + VelU_ * u) / (vh * Blk_.RangeM);
    dot = dot < -1.0 ? -1.0 : dot > 1.0 ? 1.0 : dot;
    Blk_.AspectDeg = std::acos(dot) * kRad2Deg;
    double tgtHdg = std::atan2(VelE_, VelN_) * kRad2Deg;
    Blk_.HcaDeg = FBWrap180(own.yaw - tgtHdg);
  }
}

/* The last known state, turned into a place to go and a width to sweep — the whole derivation is in the
 * header's FBTrackDatum banner. Deliberately NOT derived from Block(): the block reverts to the last
 * MEASURED position past the credible window and freezes, which is the right answer for a pursuit and
 * the wrong one for a search, so this recomputes from the same stored estimate with its own propagation
 * rule. Const and allocation-free — it is asked for once per decision tick. */
FBTrackDatum FBBfmTrack::Datum(const fb_fdm_state &own, double nowS, double turnRateDegS) const {
  FBTrackDatum d;
  if (!Have_) return d;
  d.Valid = true;
  d.AgeS = std::fmax(nowS - LastLookS_, 0.0);

  double v = std::sqrt(VelE_ * VelE_ + VelN_ * VelN_ + VelU_ * VelU_);
  double w = std::fmax(turnRateDegS, 1.0) * kDeg2Rad;
  double tProp = std::fmin(d.AgeS, 2.0 / w);   /* past 2/w the turn could have gone anywhere: stop */

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

} // namespace FlightBox
