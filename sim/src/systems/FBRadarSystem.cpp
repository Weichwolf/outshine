#include "FBRadarSystem.h"
#include "FBGeodesy.h"
#include "FBLog.h"
#include "FBUnit.h"
#include "FBUnitRegistry.h"
#include <cmath>

namespace FlightBox {

/* ENU offset -> slant range + true bearing + body-frame az/el. The body rotation is the standard
 * NED->body Euler sequence Rx(roll)*Ry(pitch)*Rz(yaw); writing it out beats building a matrix type for
 * one call site, and it is the only place in the simulator that needs the aircraft's full attitude
 * applied to a line of sight. */
void FBRadarSystem::RelativeLos(const fb_fdm_state &own, double tgtLatDeg, double tgtLonDeg,
                                double tgtAltM, double &rangeM, double &bearingDeg, double &azDeg,
                                double &elDeg) {
  double e = 0.0, n = 0.0;
  FBEnuOffsetM(own.lat, own.lon, tgtLatDeg, tgtLonDeg, e, n);
  double u = tgtAltM - own.elev;
  rangeM = std::sqrt(e * e + n * n + u * u);
  bearingDeg = std::atan2(e, n) * kRad2Deg;
  if (bearingDeg < 0.0) bearingDeg += 360.0;

  double N = n, E = e, D = -u;
  double ph = own.roll * kDeg2Rad, th = own.pitch * kDeg2Rad, ps = own.yaw * kDeg2Rad;
  double cph = std::cos(ph), sph = std::sin(ph);
  double cth = std::cos(th), sth = std::sin(th);
  double cps = std::cos(ps), sps = std::sin(ps);
  double xb = cth * cps * N + cth * sps * E - sth * D;
  double yb = (sph * sth * cps - cph * sps) * N + (sph * sth * sps + cph * cps) * E + sph * cth * D;
  double zb = (cph * sth * cps + sph * sps) * N + (cph * sth * sps - sph * cps) * E + cph * cth * D;
  azDeg = std::atan2(yb, xb) * kRad2Deg;
  elDeg = -std::atan2(zb, std::sqrt(xb * xb + yb * yb)) * kRad2Deg;
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

      int slot = -1;
      for (int i = 0; i < TrackCount_; i++)
        if (Tracks_[i].UnitId == u->GetId()) { slot = i; break; }
      if (sttOnly && (slot < 0 || Tracks_[slot].TrackNum != LockedNum_)) continue;

      FBUnitPose p = u->GetPose();
      double rangeM = 0.0, bearingDeg = 0.0, azDeg = 0.0, elDeg = 0.0;
      RelativeLos(st, p.LatDeg, p.LonDeg, p.ElevM, rangeM, bearingDeg, azDeg, elDeg);

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

      t.RangeM = rangeM;
      t.BearingDeg = bearingDeg;
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

void FBRadarSystem::DropAllTracks(const char *reason, double simTimeS) {
  if (LockedNum_ != 0)
    FBLog::Info("radar", "RADAR_LOST", {{"track", LockedNum_}, {"reason", reason}});
  for (int i = 0; i < TrackCount_; i++)
    if (Tracks_[i].Firm)
      FBLog::Info("radar", "RADAR_DROP", {{"track", Tracks_[i].TrackNum}, {"reason", reason},
          {"t", simTimeS}});
  TrackCount_ = 0;
  LockedNum_ = 0;
}

void FBRadarSystem::Run(FBState &state, const fb_fdm_state &st, const FBUnitRegistry *net,
                        double simTimeS) {
  const FBRadarScanVolume &v = ActiveVolume();
  state.fcrOn = Powered_ && v.Active;
  state.fcrMode = ModeOrdinal();
  state.iffXpdr = IffXpdr_;

  if (!state.fcrOn || !net) {
    if (TrackCount_ > 0 || LockedNum_ != 0) DropAllTracks(Powered_ ? "radar standby" : "radar off", simTimeS);
    state.fcrContactCount = 0;
    state.fcrLockIndex = -1;
    ContactCount_ = 0;
    LockNm_ = -1.0f; LockAzDeg_ = LockElDeg_ = LockClosureKt_ = LockAgeS_ = 0.0f; LockIff_ = 0;
    return;
  }

  /* The antenna's own frame grid. ActiveVolume() is re-read every iteration because acquiring a lock can
   * change the pattern (an ACM box becoming an STT envelope, modules/f16/FBF16Fcr) — and with it the
   * frame time this grid advances by. The guard bounds the catch-up a pathological FrameS could ask for
   * and RESYNCS the grid rather than falling further behind every call. */
  int guard = 0;
  while (NextScanS_ <= simTimeS && guard++ < 64) {
    ScanFrame(st, *net, simTimeS);
    UpdateLock(simTimeS, ActiveVolume().AutoAcquire);
    NextScanS_ += ActiveVolume().FrameS;
  }
  if (NextScanS_ <= simTimeS) NextScanS_ = simTimeS + ActiveVolume().FrameS;

  /* Between looks nothing about a contact's geometry moves — only its age. */
  int n = 0;
  state.fcrLockIndex = -1;
  LockNm_ = -1.0f; LockAzDeg_ = LockElDeg_ = LockClosureKt_ = LockAgeS_ = 0.0f; LockIff_ = 0;
  for (int i = 0; i < TrackCount_ && n < kMaxRadarContacts; i++) {
    const Track &t = Tracks_[i];
    if (!t.Firm) continue;
    FBRadarContact &c = state.fcrContacts[n];
    c.TrackNum = t.TrackNum;
    c.RangeM = (float)t.RangeM;
    c.BearingDeg = (float)t.BearingDeg;
    c.AzDeg = (float)t.AzDeg;
    c.ElDeg = (float)t.ElDeg;
    c.ClosureMs = (float)t.ClosureMs;
    c.LookAgeS = (float)std::fmax(0.0, simTimeS - t.LastLookS);
    c.Coasting = c.LookAgeS > (float)ActiveVolume().FrameS;
    c.Iff = t.Iff;
    if (t.TrackNum == LockedNum_) {
      state.fcrLockIndex = n;
      LockNm_ = (float)(t.RangeM * kMToNm);
      LockAzDeg_ = (float)t.AzDeg;
      LockElDeg_ = (float)t.ElDeg;
      LockClosureKt_ = (float)(t.ClosureMs * kMsToKt);
      LockAgeS_ = c.LookAgeS;
      LockIff_ = t.Iff == FBIffReply::Friendly ? 2 : t.Iff == FBIffReply::NoReply ? 1 : 0;
    }
    n++;
  }
  state.fcrContactCount = n;
  ContactCount_ = n;
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
