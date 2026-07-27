#include "FBRadarSystem.h"
#include "FBGeodesy.h"
#include "FBLog.h"
#include "FBUnit.h"
#include "FBUnitRegistry.h"
#include <cmath>

namespace FlightBox {

/* ENU offset -> slant range + true bearing + world-referenced elevation angle + body-frame az/el. The
 * body rotation is core/FBGeodesy.h's FBEnuToBodyLos (the one definition, shared with the pilot's BFM
 * steering, which needs its exact inverse).
 *
 * The world-referenced pair (bearing, elevation angle) is what a set can publish about WHERE a return
 * is without its consumer having to un-rotate a look-old body vector through a now-current attitude: the
 * radar knows its own attitude at the instant of the look, the consumer does not. Body az/el stays the
 * antenna's own quantity (the scan volume is body-referenced); the world pair is the reported position. */
void FBRadarSystem::RelativeLos(const fb_fdm_state &own, double tgtLatDeg, double tgtLonDeg,
                                double tgtAltM, double &rangeM, double &bearingDeg, double &elevAngleDeg,
                                double &azDeg, double &elDeg) {
  double e = 0.0, n = 0.0;
  FBEnuOffsetM(own.lat, own.lon, tgtLatDeg, tgtLonDeg, e, n);
  double u = tgtAltM - own.elev;
  rangeM = std::sqrt(e * e + n * n + u * u);
  bearingDeg = std::atan2(e, n) * kRad2Deg;
  if (bearingDeg < 0.0) bearingDeg += 360.0;
  elevAngleDeg = std::atan2(u, std::sqrt(e * e + n * n)) * kRad2Deg;
  FBEnuToBodyLos(own.roll, own.pitch, own.yaw, e, n, u, azDeg, elDeg);
}

/* Own velocity resolved along the line of sight. The unit vector comes from the WORLD-referenced pair
 * the look already produced, and the velocity from the FDM state's ENU components (fb_fdm_state's
 * X-Plane axes: +x east, +y up, +z south). This is the CLUTTER Doppler — what a stationary point in
 * that direction would close at — and therefore the reference the notch test is taken against. */
double FBRadarSystem::OwnClosureOn(const fb_fdm_state &st, double bearingDeg, double elevAngleDeg) {
  double cb = std::cos(bearingDeg * kDeg2Rad), sb = std::sin(bearingDeg * kDeg2Rad);
  double ce = std::cos(elevAngleDeg * kDeg2Rad), se = std::sin(elevAngleDeg * kDeg2Rad);
  double ue = sb * ce, un = cb * ce, uu = se;
  return st.vx * ue + (-st.vz) * un + st.vy * uu;
}

/* Which chaff cloud this set ends up measuring (class banner). Every cloud in the unit's published
 * signature is tested against the SAME volume the aircraft was tested against, and the strongest return
 * wins: the two-way radar equation is RCS/r^4, with the cloud's own age curve supplying the RCS
 * (core/FBCountermeasure.h). Deterministic by construction — no draw, no probability. */
const FBChaffCloud *FBRadarSystem::SelectDecoy(const FBChaffCloud *clouds, const fb_fdm_state &st,
                                               const FBRadarScanVolume &v, double simTimeS) const {
  const FBChaffCloud *best = nullptr;
  double bestPower = 0.0;
  for (int i = 0; i < kMaxChaffClouds; i++) {
    const FBChaffCloud &c = clouds[i];
    if (!c.Active) continue;
    double rcs = FBChaffRcsNorm(simTimeS - c.BloomS);
    if (rcs <= 0.0) continue;
    double r = 0.0, brg = 0.0, ev = 0.0, az = 0.0, el = 0.0;
    RelativeLos(st, c.LatDeg, c.LonDeg, c.AltM, r, brg, ev, az, el);
    if (r > v.RangeM || r < 1.0) continue;
    if (std::fabs(FBWrap180(az - v.AzCenterDeg)) > v.AzHalfDeg) continue;
    if (std::fabs(el - v.ElCenterDeg) > v.ElHalfDeg) continue;
    double power = rcs / (r * r * r * r);
    if (power > bestPower) { bestPower = power; best = &c; }
  }
  return best;
}

void FBRadarSystem::SetPowered(bool on) {
  Powered_ = on;
  if (!on) NextScanS_ = 0.0;   /* a set that comes back up starts a fresh frame grid, not a stale one */
}

FBIffReply FBRadarSystem::Interrogate(const FBUnit &u) const {
  if (!IffInterrogator_) return FBIffReply::NotInterrogated;
  bool validReply = u.GetSignature().IffXpdr && u.GetTeam() == SelfTeam_;
  return validReply ? FBIffReply::Friendly : FBIffReply::NoReply;
}

/* One antenna frame: the volume is swept once, every unit is tested against it, hits build tracks and
 * misses coast them. The registry is walked IN ORDER, so a fresh acquisition's track number depends on
 * the mission's declaration order and never on who happened to be seen first. */
void FBRadarSystem::ScanFrame(const fb_fdm_state &st, const FBUnitRegistry &net, double simTimeS) {
  const FBRadarScanVolume &v = ActiveVolume();
  bool seen[kMaxRadarContacts] = {};

  /* STT: the beam is on the locked track and nowhere else, so every other unit is not even looked at
   * this frame and its trackfile ages out below (FBRadarScanVolume::SingleTarget). */
  bool sttOnly = v.SingleTarget && LockedNum_ != 0;

  if (v.Active) {
    for (const FBUnit *u : net.Units()) {
      if (!u || u->GetId() == SelfId_) continue;   /* the set does not paint itself */
      /* An air-to-air set searches for AIRCRAFT. A store in free flight is a unit of the world like any
       * other, but it is not what this radar's detection model describes, and painting one as a contact
       * would be an invention rather than a simulation. */
      if (u->GetKind() != FBUnitKind::Aircraft) continue;

      int slot = -1;
      for (int i = 0; i < TrackCount_; i++)
        if (Tracks_[i].UnitId == u->GetId()) { slot = i; break; }
      if (sttOnly && (slot < 0 || Tracks_[slot].TrackNum != LockedNum_)) continue;

      FBUnitPose p = u->GetPose();
      FBUnitSignature sig = u->GetSignature();
      double rangeM = 0.0, bearingDeg = 0.0, elevAngleDeg = 0.0, azDeg = 0.0, elDeg = 0.0;
      RelativeLos(st, p.LatDeg, p.LonDeg, p.ElevM, rangeM, bearingDeg, elevAngleDeg, azDeg, elDeg);

      /* THE DOPPLER DECISION (class banner + kDopplerNotchMs), taken BEFORE the volume test, because a
       * seduced look measures the cloud and it is the CLOUD that then has to be in the beam. The
       * target's radial velocity is what the set measures, not what the target publishes: own velocity
       * along the line of sight minus the closure differenced from the last two looks. Only a track
       * that already has a look pair can be fooled — a first detection has no closure to test. */
      const FBChaffCloud *decoy = nullptr;
      double tgtRadialMs = 0.0, ownClosureMs = 0.0, measuredClosureMs = 0.0;
      double dtDwell = slot >= 0 ? simTimeS - Tracks_[slot].DopplerRefS : 0.0;
      bool dwellDone = slot >= 0 && Tracks_[slot].DopplerRefS > 0.0 && dtDwell >= kDopplerDwellS;
      if (slot >= 0 && (dwellDone || Tracks_[slot].Seduced)) {
        const Track &prev = Tracks_[slot];
        ownClosureMs = OwnClosureOn(st, bearingDeg, elevAngleDeg);
        measuredClosureMs = dwellDone ? (prev.DopplerRefRangeM - rangeM) / dtDwell : prev.ClosureMs;
        tgtRadialMs = ownClosureMs - measuredClosureMs;
        /* Once the tracking gate has settled into the clutter filter it STAYS there while a return
         * exists in it: a cloud is stationary, so it is inside the notch by construction, and a
         * processor tracking that bin has no reason to leave it. Without the stickiness the test would
         * flip every look — the substituted measurement makes the NEXT look's closure a jump, which
         * reads as a target well outside the notch (measured: seduce/resolve alternating at the
         * seeker's 20 Hz frame). The seduction ends when the clouds do. */
        if (prev.Seduced || std::fabs(tgtRadialMs) < kDopplerNotchMs)
          decoy = SelectDecoy(sig.Chaff, st, v, simTimeS);
      }
      if (decoy)
        RelativeLos(st, decoy->LatDeg, decoy->LonDeg, decoy->AltM, rangeM, bearingDeg, elevAngleDeg,
                    azDeg, elDeg);

      bool inVolume = rangeM <= v.RangeM &&
                      std::fabs(FBWrap180(azDeg - v.AzCenterDeg)) <= v.AzHalfDeg &&
                      std::fabs(elDeg - v.ElCenterDeg) <= v.ElHalfDeg;

      if (!inVolume) {
        /* A hit streak that breaks before it firms is not a track at all — only a FIRM track earns the
         * coast below, which is why an un-firm entry is deleted here rather than aged. */
        if (slot >= 0 && !Tracks_[slot].Firm) {
          for (int j = slot; j < TrackCount_ - 1; j++) Tracks_[j] = Tracks_[j + 1];
          TrackCount_--;
          for (int j = slot; j < TrackCount_; j++) seen[j] = seen[j + 1];
        }
        continue;
      }

      if (slot < 0) {
        if (TrackCount_ >= kMaxRadarContacts) continue;   /* the file is full; nothing is displaced */
        slot = TrackCount_++;
        Tracks_[slot] = Track{};
        Tracks_[slot].UnitId = u->GetId();
        Tracks_[slot].PrevRangeM = rangeM;
        Tracks_[slot].PrevLookS = simTimeS;
      }

      Track &t = Tracks_[slot];
      /* Range rate from consecutive looks. A pulse-Doppler set measures it directly; differencing the
       * two looks the track actually stands on is the same quantity through the observable, and needs no
       * assumption about a velocity the target's snapshot does not publish (its vertical rate). */
      double dtLook = simTimeS - t.PrevLookS;
      if (dtLook > 1e-6) t.ClosureMs = (t.PrevRangeM - rangeM) / dtLook;
      t.PrevRangeM = rangeM;
      t.PrevLookS = simTimeS;

      if (decoy && !t.Seduced)
        FBLog::Info("radar", "CHAFF_SEDUCED", {{"track", t.TrackNum},
            {"tgtRadialMs", tgtRadialMs}, {"notchMs", kDopplerNotchMs},
            {"ownClosMs", ownClosureMs}, {"measClosMs", measuredClosureMs},
            {"cloudAgeS", simTimeS - decoy->BloomS}, {"rangeM", rangeM},
            {"offsetM", FBPlanarDistM(decoy->LatDeg, decoy->LonDeg, p.LatDeg, p.LonDeg)}});
      else if (!decoy && t.Seduced)
        FBLog::Info("radar", "CHAFF_RESOLVED", {{"track", t.TrackNum},
            {"tgtRadialMs", tgtRadialMs}, {"notchMs", kDopplerNotchMs}, {"rangeM", rangeM}});
      t.Seduced = decoy != nullptr;

      if (t.DopplerRefS <= 0.0 || dwellDone) { t.DopplerRefRangeM = rangeM; t.DopplerRefS = simTimeS; }

      t.RangeM = rangeM;
      t.BearingDeg = bearingDeg;
      t.ElevAngleDeg = elevAngleDeg;
      t.AzDeg = azDeg;
      t.ElDeg = elDeg;
      t.LastLookS = simTimeS;
      seen[slot] = true;
      if (t.Hits < kHitsToFirm) t.Hits++;

      if (!t.Firm && t.Hits >= kHitsToFirm) {
        t.Firm = true;
        t.TrackNum = NextTrackNum_++;
        FBLog::Info("radar", "RADAR_CONTACT", {{"track", t.TrackNum}, {"rangeNm", rangeM * kMToNm},
            {"azDeg", azDeg}, {"elDeg", elDeg}, {"closureKt", t.ClosureMs * kMsToKt}});
      }
      if (t.Firm && simTimeS - t.LastIffS >= kIffPeriodS) {
        FBIffReply reply = Interrogate(*u);
        if (reply != t.Iff)
          FBLog::Info("radar", "IFF_REPLY", {{"track", t.TrackNum},
              {"reply", reply == FBIffReply::Friendly ? "friendly"
                        : reply == FBIffReply::NoReply ? "none" : "not-interrogated"}});
        t.Iff = reply;
        t.LastIffS = simTimeS;
      }
    }
  }

  /* Coast + drop, in one pass over the file. `seen` is indexed by the slot as it stood during the walk
   * above; the compaction there kept both in step. */
  double coastS = std::fmax(kMinCoastS, kCoastFrames * v.FrameS);
  for (int i = 0; i < TrackCount_;) {
    Track &t = Tracks_[i];
    if (seen[i] || simTimeS - t.LastLookS < coastS) {
      if (!seen[i]) t.Hits = 0;   /* the streak is broken; re-firming needs kHitsToFirm fresh looks */
      i++;
      continue;
    }
    FBLog::Info("radar", "RADAR_DROP", {{"track", t.TrackNum}, {"lastRangeNm", t.RangeM * kMToNm},
        {"coastS", simTimeS - t.LastLookS}});
    for (int j = i; j < TrackCount_ - 1; j++) Tracks_[j] = Tracks_[j + 1];
    for (int j = i; j < TrackCount_ - 1; j++) seen[j] = seen[j + 1];
    TrackCount_--;
  }
}

/* The ACM property (doc/f16/radar-sensors.md: "auto-lock the first target in a close-range volume"):
 * nobody operates a radar in a turning fight, so a volume declared AutoAcquire locks by itself. "First"
 * is read as NEAREST — the beam's instantaneous position inside its sweep is not modelled, so the only
 * alternative would be an arbitrary registry-order tie-break. */
void FBRadarSystem::UpdateLock(double simTimeS, bool autoAcquire) {
  if (LockedNum_ != 0) {
    for (int i = 0; i < TrackCount_; i++)
      if (Tracks_[i].TrackNum == LockedNum_) {
        if (!autoAcquire) break;   /* the mode no longer holds locks — fall through to the drop below */
        return;
      }
    FBLog::Info("radar", "RADAR_LOST", {{"track", LockedNum_},
        {"reason", autoAcquire ? "track dropped" : "mode change"}});
    LockedNum_ = 0;
    /* A lock the PILOT designated does not re-acquire by itself (see Designate): losing it means the
     * set falls back to searching, and taking a new one is another decision with the same price. */
    if (Designated_) { Designated_ = false; return; }
  }
  if (!autoAcquire) return;

  const Track *best = nullptr;
  for (int i = 0; i < TrackCount_; i++) {
    const Track &t = Tracks_[i];
    if (!t.Firm) continue;
    if (!best || t.RangeM < best->RangeM) best = &t;
  }
  if (!best) return;
  LockedNum_ = best->TrackNum;
  FBLog::Info("radar", "RADAR_LOCK", {{"track", best->TrackNum}, {"rangeNm", best->RangeM * kMToNm},
      {"azDeg", best->AzDeg}, {"elDeg", best->ElDeg}, {"closureKt", best->ClosureMs * kMsToKt},
      {"iff", best->Iff == FBIffReply::Friendly ? "friendly"
              : best->Iff == FBIffReply::NoReply ? "unknown" : "not-interrogated"},
      {"t", simTimeS}});
}

/* The pilot's designation (see the header). Deliberately a separate event from the ACM auto-acquisition's
 * RADAR_LOCK: the two look identical on the scope and are opposite decisions in the debrief — one was
 * taken, the other happened. */
bool FBRadarSystem::Designate(int trackNum, double simTimeS) {
  /* THE ANTENNA MOVES WITH THE DESIGNATION, and the frame grid has to move with it. Locking replaces
   * the pattern (a 4-second search sweep becomes a 0.1-second stare, modules/f16/FBF16Fcr), but the
   * grid still holds the search pattern's next-sweep time — so without this the first single-target
   * look would arrive up to a whole search frame later and the track would stand on the stale echo the
   * designation was taken from. Measured: four seconds of a frozen lock, during which the fused target
   * VELOCITY stayed at zero and a shot taken there was programmed with a stationary target. */
  NextScanS_ = simTimeS;
  if (trackNum == 0) {
    if (LockedNum_ == 0) return false;
    FBLog::Info("radar", "RADAR_BREAK", {{"track", LockedNum_}, {"t", simTimeS}});
    LockedNum_ = 0;
    Designated_ = false;
    return true;
  }
  for (int i = 0; i < TrackCount_; i++) {
    const Track &t = Tracks_[i];
    if (!t.Firm || t.TrackNum != trackNum) continue;
    LockedNum_ = trackNum;
    Designated_ = true;
    FBLog::Info("radar", "RADAR_DESIGNATE", {{"track", trackNum}, {"rangeNm", t.RangeM * kMToNm},
        {"azDeg", t.AzDeg}, {"elDeg", t.ElDeg}, {"closureKt", t.ClosureMs * kMsToKt},
        {"iff", t.Iff == FBIffReply::Friendly ? "friendly"
                : t.Iff == FBIffReply::NoReply ? "unknown" : "not-interrogated"}, {"t", simTimeS}});
    return true;
  }
  return false;
}

void FBRadarSystem::DropAllTracks(const char *reason, double simTimeS) {
  if (LockedNum_ != 0)
    FBLog::Info("radar", "RADAR_LOST", {{"track", LockedNum_}, {"reason", reason}});
  for (int i = 0; i < TrackCount_; i++)
    if (Tracks_[i].Firm)
      FBLog::Info("radar", "RADAR_DROP", {{"track", Tracks_[i].TrackNum}, {"reason", reason},
          {"t", simTimeS}});
  TrackCount_ = 0;
  LockedNum_ = 0;
  Designated_ = false;
}

void FBRadarSystem::Run(FBState &state, const fb_fdm_state &st, const FBUnitRegistry *net,
                        double simTimeS) {
  const FBRadarScanVolume &v = ActiveVolume();
  FBRadarBlock &b = state.Radar;
  b.Radiating = Powered_ && v.Active;
  b.ModeOrdinal = ModeOrdinal();
  b.IffTransponder = IffXpdr_;

  if (!b.Radiating || !net) {
    if (TrackCount_ > 0 || LockedNum_ != 0) DropAllTracks(Powered_ ? "radar standby" : "radar off", simTimeS);
    b.ContactCount = 0;
    b.LockIndex = -1;
    /* A set that is not radiating has no picture — not an empty one. Invalid, so a consumer cannot
     * mistake "nothing found" for "not looking" (core/FBBlockStatus.h). */
    b.H.Invalidate();
    ContactCount_ = 0;
    LockNm_ = -1.0f; LockAzDeg_ = LockElDeg_ = LockClosureKt_ = LockAgeS_ = 0.0f; LockIff_ = 0;
    return;
  }

  /* The antenna's own frame grid. ActiveVolume() is re-read every iteration because acquiring a lock can
   * change the pattern (an ACM box becoming an STT envelope, modules/f16/FBF16Fcr) — and with it the
   * frame time this grid advances by. The guard bounds the catch-up a pathological FrameS could ask for
   * and RESYNCS the grid rather than falling further behind every call. */
  int guard = 0;
  bool swept = false;
  while (NextScanS_ <= simTimeS && guard++ < 64) {
    swept = true;
    ScanFrame(st, *net, simTimeS);
    UpdateLock(simTimeS, ActiveVolume().AutoAcquire);
    NextScanS_ += ActiveVolume().FrameS;
  }
  if (NextScanS_ <= simTimeS) NextScanS_ = simTimeS + ActiveVolume().FrameS;

  /* Between looks nothing about a contact's geometry moves — only its age. */
  int n = 0;
  b.LockIndex = -1;
  LockNm_ = -1.0f; LockAzDeg_ = LockElDeg_ = LockClosureKt_ = LockAgeS_ = 0.0f; LockIff_ = 0;
  for (int i = 0; i < TrackCount_ && n < kMaxRadarContacts; i++) {
    const Track &t = Tracks_[i];
    if (!t.Firm) continue;
    FBRadarContact &c = b.Contacts[n];
    c.TrackNum = t.TrackNum;
    c.RangeM = (float)t.RangeM;
    c.BearingDeg = (float)t.BearingDeg;
    c.ElevAngleDeg = (float)t.ElevAngleDeg;
    c.AzDeg = (float)t.AzDeg;
    c.ElDeg = (float)t.ElDeg;
    c.ClosureMs = (float)t.ClosureMs;
    c.LookAgeS = (float)std::fmax(0.0, simTimeS - t.LastLookS);
    c.Coasting = c.LookAgeS > (float)ActiveVolume().FrameS;
    c.Iff = t.Iff;
    if (t.TrackNum == LockedNum_) {
      b.LockIndex = n;
      LockNm_ = (float)(t.RangeM * kMToNm);
      LockAzDeg_ = (float)t.AzDeg;
      LockElDeg_ = (float)t.ElDeg;
      LockClosureKt_ = (float)(t.ClosureMs * kMsToKt);
      LockAgeS_ = c.LookAgeS;
      LockIff_ = t.Iff == FBIffReply::Friendly ? 2 : t.Iff == FBIffReply::NoReply ? 1 : 0;
    }
    n++;
  }
  b.ContactCount = n;
  ContactCount_ = n;
  /* Between sweeps the geometry stands still and only the per-contact age moves (class banner) —
   * freeze-at-last-value, which is exactly what Held means. The stamp keeps naming the last completed
   * sweep, so a consumer can ask how old the picture is. */
  if (swept) b.H.Publish(simTimeS);
  else b.H.Hold();
}

/* What leaves the antenna (class banner). Two shapes, and the difference between them is the difference
 * between "somebody is looking" and "he has me": a search pattern's beam crosses everything inside the
 * volume it sweeps, so the volume IS the illuminated window; a single-target track puts the whole beam
 * on one contact, so the window collapses to a pencil pointed at it. Both are body-referenced, i.e. they
 * roll and pitch with the aircraft, which is what makes a warning receiver's picture change when the
 * EMITTER manoeuvres. */
FBEmitterSignature FBRadarSystem::Emission() const {
  FBEmitterSignature e;
  const FBRadarScanVolume &v = ActiveVolume();
  if (!Powered_ || !v.Active) return e;   /* Mode::None — nothing is radiating */
  e.Kind = EmitterKind();
  e.RangeM = (float)v.RangeM;
  if (v.SingleTarget && LockedNum_ != 0) {
    for (int i = 0; i < TrackCount_; i++) {
      if (Tracks_[i].TrackNum != LockedNum_) continue;
      e.Mode = FBEmitterMode::Track;
      e.AzCenterDeg = (float)Tracks_[i].AzDeg;
      e.ElCenterDeg = (float)Tracks_[i].ElDeg;
      e.AzHalfDeg = e.ElHalfDeg = (float)kTrackBeamHalfDeg;
      return e;
    }
  }
  e.Mode = FBEmitterMode::Search;
  e.AzCenterDeg = (float)v.AzCenterDeg;
  e.AzHalfDeg = (float)v.AzHalfDeg;
  e.ElCenterDeg = (float)v.ElCenterDeg;
  e.ElHalfDeg = (float)v.ElHalfDeg;
  return e;
}

void FBRadarSystem::DeclareTelemetry(FBTelemetrySchema &schema) const {
  schema.Add("fcr_on");
  schema.Add("fcr_mode");
  schema.Add("fcr_contacts");
  schema.Add("fcr_lock");
  schema.Add("fcr_lock_nm", "nm");
  schema.Add("fcr_lock_az", "deg");
  schema.Add("fcr_lock_el", "deg");
  schema.Add("fcr_lock_clos", "kt");
  schema.Add("fcr_lock_age", "s");
  schema.Add("fcr_iff");
  schema.Add("iff_xpdr");
}

void FBRadarSystem::SampleTelemetry(FBTelemetryRow &row) const {
  row.Push(Powered_ && ActiveVolume().Active);
  row.Push(ModeOrdinal());
  row.Push(ContactCount_);
  row.Push(LockedNum_);
  row.Push((double)LockNm_);
  row.Push((double)LockAzDeg_);
  row.Push((double)LockElDeg_);
  row.Push((double)LockClosureKt_);
  row.Push((double)LockAgeS_);
  row.Push(LockIff_);
  row.Push(IffXpdr_);
}

} // namespace FlightBox
