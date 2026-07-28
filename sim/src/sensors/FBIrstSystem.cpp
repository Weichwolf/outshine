#include "FBIrstSystem.h"
#include "FBGeodesy.h"
#include "FBLog.h"
#include "FBUnit.h"
#include "FBUnitRegistry.h"
#include <cmath>

namespace FlightBox::Sensors {

void FBIrstSystem::SetPowered(bool on) {
  if (Powered_ == on) return;
  Powered_ = on;
  if (!on) NextScanS_ = 0.0;
}

void FBIrstSystem::SetLaserArmed(bool on) {
  if (LaserArmed_ == on) return;
  LaserArmed_ = on;
  FBLog::Info("irst", "LASER", {{"armed", on}});
}

/* THE ASPECT LAW. The aspect angle is the standard one: the angle between the TARGET's heading and the
 * bearing on which this sensor sees it. A tail chase (bearing = his heading) is aspect 0 and looks
 * straight into the nozzle; head-on is 180. Everything comes off the published pose — no velocity
 * vector of the target is needed, and none would be available. */
double FBIrstSystem::DetectRangeM(const FBIrstFieldOfRegard &f, const Units::FBUnitPose &tgt,
                                  bool afterburner, double bearingDeg) const {
  double aspectDeg = std::fabs(FBWrap180(bearingDeg - tgt.HeadingDeg));
  double face = 0.5 * (1.0 + std::cos(aspectDeg * kDeg2Rad));   /* 1 = dead astern, 0 = head-on */
  double reach = f.RangeM * (kFrontFraction + (1.0 - kFrontFraction) * face);
  if (afterburner) reach *= kAfterburnerRangeFactor;
  return reach;
}

/* THE FIRST TACTICAL EFFECT WEATHER HAS ON A SENSOR IN THIS SIMULATOR, and it is deliberately the
 * crudest honest form: a deck whose cover is at or above "broken" is treated as a LID between the two
 * altitudes it spans. Two aircraft on the same side of every deck see each other; one above and one
 * below a solid deck do not.
 *
 * WHAT IS NOT MODELLED, and it is the next step rather than a defect: the deck's HORIZONTAL structure.
 * core/FBCloudDensity is a closed-form field and could be marched along the line of sight for a real
 * transmittance, which would let a 60 %-cover deck be seen through where the holes are. That march
 * costs a per-contact, per-look integration — the same price terrain masking costs the radar, and it
 * is declined here for the same stated reason. */
bool FBIrstSystem::CloudMasked(double ownAltM, double tgtAltM) const {
  double lo = ownAltM < tgtAltM ? ownAltM : tgtAltM;
  double hi = ownAltM < tgtAltM ? tgtAltM : ownAltM;
  for (int i = 0; i < 3; i++) {
    const FBCloudDeckParams &d = Sky_.Deck[i];
    if (d.Cover < kCloudMaskCover) continue;
    if (hi >= (double)d.BaseM && lo <= (double)d.TopM) return true;
  }
  return false;
}

/* One head frame. Registry IN ORDER, so a track number hangs on the mission declaration and never on
 * who was seen first — the same determinism rule as the radar's. */
void FBIrstSystem::ScanFrame(const Fdm::fb_fdm_state &st, const Units::FBUnitRegistry &net,
                             double simTimeS) {
  const FBIrstFieldOfRegard &f = ActiveField();
  bool seen[kMaxIrstContacts] = {};
  int masked = 0;

  bool sttOnly = f.SingleTarget && LockedNum_ != 0;

  if (f.Active) {
    for (const Units::FBUnit *u : net.Units()) {
      if (!u || u->GetId() == SelfId_) continue;
      /* An air-to-air head looks for AIRCRAFT. A store in free flight is cold and a ground target is
       * not what this field of regard is pointed at; painting either would be invention. */
      if (u->GetKind() != Units::FBUnitKind::Aircraft) continue;

      int slot = -1;
      for (int i = 0; i < TrackCount_; i++)
        if (Tracks_[i].UnitId == u->GetId()) { slot = i; break; }
      if (sttOnly && (slot < 0 || Tracks_[slot].TrackNum != LockedNum_)) continue;

      Units::FBUnitPose p = u->GetPose();
      Units::FBUnitSignature sig = u->GetSignature();
      double rangeM = 0.0, bearingDeg = 0.0, elevAngleDeg = 0.0, azDeg = 0.0, elDeg = 0.0;
      FBRadarSystem::RelativeLos(st, p.LatDeg, p.LonDeg, p.ElevM, rangeM, bearingDeg, elevAngleDeg,
                                 azDeg, elDeg);

      bool inField = std::fabs(FBWrap180(azDeg - f.AzCenterDeg)) <= f.AzHalfDeg &&
                     std::fabs(elDeg - f.ElCenterDeg) <= f.ElHalfDeg &&
                     rangeM <= DetectRangeM(f, p, sig.Afterburner, bearingDeg);
      /* Counted even when the source would have been out of the field anyway is WRONG for a diagnostic,
       * so the mask is only tallied where it actually decided the outcome. */
      if (inField && CloudMasked(st.elev, p.ElevM)) { inField = false; masked++; }

      if (!inField) {
        if (slot >= 0 && !Tracks_[slot].Firm) {
          for (int j = slot; j < TrackCount_ - 1; j++) Tracks_[j] = Tracks_[j + 1];
          TrackCount_--;
          for (int j = slot; j < TrackCount_; j++) seen[j] = seen[j + 1];
        }
        continue;
      }

      if (slot < 0) {
        if (TrackCount_ >= kMaxIrstContacts) continue;
        slot = TrackCount_++;
        Tracks_[slot] = Track{};
        Tracks_[slot].UnitId = u->GetId();
      }

      Track &t = Tracks_[slot];
      t.BearingDeg = bearingDeg;
      t.ElevAngleDeg = elevAngleDeg;
      t.AzDeg = azDeg;
      t.ElDeg = elDeg;
      t.RangeM = rangeM;
      t.LastLookS = simTimeS;
      seen[slot] = true;
      if (t.Hits < kHitsToFirm) t.Hits++;

      if (!t.Firm && t.Hits >= kHitsToFirm) {
        t.Firm = true;
        t.TrackNum = NextTrackNum_++;
        /* NO range in the event either: what is not published is not logged, or a debriefing would
         * carry knowledge the pilot never had. */
        FBLog::Info("irst", "IRST_CONTACT", {{"track", t.TrackNum}, {"azDeg", azDeg}, {"elDeg", elDeg},
            {"brgDeg", bearingDeg}, {"ab", sig.Afterburner}});
      }

      /* THE LASER. Only the tracked source, only when armed, only inside its own reach — and it is the
       * one line in this class that produces metres. */
      bool wasRanged = t.HasLaserRange;
      t.HasLaserRange = LaserArmed_ && t.Firm && t.TrackNum == LockedNum_ &&
                        LaserRangeM() > 0.0 && rangeM <= LaserRangeM();
      t.LaserRangeM = t.HasLaserRange ? rangeM : 0.0;
      if (t.HasLaserRange && !wasRanged)
        FBLog::Info("irst", "LASER_RANGE", {{"track", t.TrackNum}, {"rangeM", rangeM},
            {"maxM", LaserRangeM()}});
    }
  }

  double coastS = kCoastFrames * f.FrameS > kMinCoastS ? kCoastFrames * f.FrameS : kMinCoastS;
  for (int i = 0; i < TrackCount_;) {
    Track &t = Tracks_[i];
    if (seen[i] || simTimeS - t.LastLookS < coastS) {
      if (!seen[i]) t.Hits = 0;
      i++;
      continue;
    }
    FBLog::Info("irst", "IRST_DROP", {{"track", t.TrackNum}, {"coastS", simTimeS - t.LastLookS}});
    for (int j = i; j < TrackCount_ - 1; j++) Tracks_[j] = Tracks_[j + 1];
    for (int j = i; j < TrackCount_ - 1; j++) seen[j] = seen[j + 1];
    TrackCount_--;
  }
  MaskedCount_ = masked;
}

/* Without range there is no "nearest": the only ordering an angle-only sensor has is ANGLE, so the
 * auto-acquire takes the source closest to the centre of the field. Stated here because it is the one
 * place where the missing range changes a decision rather than a number. */
void FBIrstSystem::UpdateLock(double simTimeS, bool autoAcquire) {
  if (LockedNum_ != 0) {
    for (int i = 0; i < TrackCount_; i++)
      if (Tracks_[i].TrackNum == LockedNum_) {
        if (!autoAcquire) break;
        return;
      }
    FBLog::Info("irst", "IRST_LOST", {{"track", LockedNum_},
        {"reason", autoAcquire ? "track dropped" : "mode change"}});
    LockedNum_ = 0;
    if (Designated_) { Designated_ = false; return; }
  }
  if (!autoAcquire) return;

  const FBIrstFieldOfRegard &f = ActiveField();
  const Track *best = nullptr;
  double bestOff = 0.0;
  for (int i = 0; i < TrackCount_; i++) {
    const Track &t = Tracks_[i];
    if (!t.Firm) continue;
    double dAz = FBWrap180(t.AzDeg - f.AzCenterDeg), dEl = t.ElDeg - f.ElCenterDeg;
    double off = dAz * dAz + dEl * dEl;
    if (!best || off < bestOff) { best = &t; bestOff = off; }
  }
  if (!best) return;
  LockedNum_ = best->TrackNum;
  FBLog::Info("irst", "IRST_LOCK", {{"track", best->TrackNum}, {"azDeg", best->AzDeg},
      {"elDeg", best->ElDeg}, {"t", simTimeS}});
}

bool FBIrstSystem::Designate(int trackNum, double simTimeS) {
  NextScanS_ = simTimeS;   /* the pattern changes with the lock, so the raster has to go with it */
  if (trackNum == 0) {
    if (LockedNum_ == 0) return false;
    FBLog::Info("irst", "IRST_BREAK", {{"track", LockedNum_}, {"t", simTimeS}});
    LockedNum_ = 0;
    Designated_ = false;
    return true;
  }
  for (int i = 0; i < TrackCount_; i++) {
    const Track &t = Tracks_[i];
    if (!t.Firm || t.TrackNum != trackNum) continue;
    LockedNum_ = trackNum;
    Designated_ = true;
    FBLog::Info("irst", "IRST_DESIGNATE", {{"track", trackNum}, {"azDeg", t.AzDeg},
        {"elDeg", t.ElDeg}, {"t", simTimeS}});
    return true;
  }
  return false;
}

void FBIrstSystem::DropAllTracks(const char *reason, double simTimeS) {
  if (LockedNum_ != 0) FBLog::Info("irst", "IRST_LOST", {{"track", LockedNum_}, {"reason", reason}});
  for (int i = 0; i < TrackCount_; i++)
    if (Tracks_[i].Firm)
      FBLog::Info("irst", "IRST_DROP", {{"track", Tracks_[i].TrackNum}, {"reason", reason},
          {"t", simTimeS}});
  TrackCount_ = 0;
  LockedNum_ = 0;
  Designated_ = false;
}

void FBIrstSystem::Run(FBState &state, const Fdm::fb_fdm_state &st, const Units::FBUnitRegistry *net,
                       double simTimeS) {
  const FBIrstFieldOfRegard &f = ActiveField();
  FBIrstBlock &b = state.Irst;
  b.Powered = Powered_;
  b.Searching = Powered_ && f.Active;
  b.ModeOrdinal = ModeOrdinal();
  b.LaserArmed = LaserArmed_;

  if (!b.Searching || !net) {
    if (TrackCount_ > 0 || LockedNum_ != 0) DropAllTracks(Powered_ ? "head caged" : "irst off", simTimeS);
    b.ContactCount = 0;
    b.LockIndex = -1;
    b.CloudMaskedCount = 0;
    /* A caged head has NO picture, not an empty one — the same distinction the radar makes. */
    b.H.Invalidate();
    ContactCount_ = MaskedCount_ = 0;
    LockAzDeg_ = LockElDeg_ = LockAgeS_ = 0.0f;
    LockNm_ = -1.0f;
    BlockStatus_ = (int)b.H.Status;
    return;
  }

  int guard = 0;
  bool swept = false;
  while (NextScanS_ <= simTimeS && guard++ < 64) {
    swept = true;
    ScanFrame(st, *net, simTimeS);
    UpdateLock(simTimeS, ActiveField().AutoAcquire);
    NextScanS_ += ActiveField().FrameS;
  }
  if (NextScanS_ <= simTimeS) NextScanS_ = simTimeS + ActiveField().FrameS;

  int n = 0;
  b.LockIndex = -1;
  LockAzDeg_ = LockElDeg_ = LockAgeS_ = 0.0f;
  LockNm_ = -1.0f;
  for (int i = 0; i < TrackCount_ && n < kMaxIrstContacts; i++) {
    const Track &t = Tracks_[i];
    if (!t.Firm) continue;
    FBIrstContact &c = b.Contacts[n];
    c.TrackNum = t.TrackNum;
    c.BearingDeg = (float)t.BearingDeg;
    c.ElevAngleDeg = (float)t.ElevAngleDeg;
    c.AzDeg = (float)t.AzDeg;
    c.ElDeg = (float)t.ElDeg;
    c.LookAgeS = (float)std::fmax(0.0, simTimeS - t.LastLookS);
    c.Coasting = c.LookAgeS > (float)ActiveField().FrameS;
    c.HasRange = t.HasLaserRange;
    c.RangeM = (float)t.LaserRangeM;
    if (t.TrackNum == LockedNum_) {
      b.LockIndex = n;
      LockAzDeg_ = (float)t.AzDeg;
      LockElDeg_ = (float)t.ElDeg;
      LockAgeS_ = c.LookAgeS;
      /* -1 when the laser did not measure: the telemetry column must not be able to leak a range the
       * cockpit never had. */
      LockNm_ = t.HasLaserRange ? (float)(t.LaserRangeM * kMToNm) : -1.0f;
    }
    n++;
  }
  b.ContactCount = n;
  b.CloudMaskedCount = MaskedCount_;
  ContactCount_ = n;
  if (swept) b.H.Publish(simTimeS);
  else b.H.Hold();
  BlockStatus_ = (int)b.H.Status;
}

void FBIrstSystem::DeclareTelemetry(FBTelemetrySchema &schema) const {
  /* FIRST column is this block's validity, declared HERE and not in FBStateBusTelemetry's list: that
   * list sits in the middle of every telemetry.csv ever measured, and one more name in it would shift
   * every column to its right. The same rule FBRwrSystem states. */
  schema.Add("blk_irst");
  schema.Add("irst_on");
  schema.Add("irst_mode");
  schema.Add("irst_contacts");
  schema.Add("irst_lock");
  schema.Add("irst_lock_az", "deg");
  schema.Add("irst_lock_el", "deg");
  schema.Add("irst_lock_age", "s");
  schema.Add("irst_lock_nm", "nm");   /* LASER only; -1 = nobody measured a range */
  schema.Add("irst_masked");          /* sources rejected by a cloud deck this frame */
  schema.Add("irst_laser");
}

void FBIrstSystem::SampleTelemetry(FBTelemetryRow &row) const {
  row.Push(BlockStatus_);
  row.Push(Powered_);
  row.Push(ModeOrdinal());
  row.Push(ContactCount_);
  row.Push(LockedNum_);
  row.Push((double)LockAzDeg_);
  row.Push((double)LockElDeg_);
  row.Push((double)LockAgeS_);
  row.Push((double)LockNm_);
  row.Push(MaskedCount_);
  row.Push(LaserArmed_);
}

} // namespace FlightBox::Sensors
