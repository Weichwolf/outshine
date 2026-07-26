#include "FBDatalinkSystem.h"
#include "FBGeodesy.h"
#include "FBLog.h"
#include "FBUnit.h"
#include "FBUnitRegistry.h"
#include "FBUnits.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace FlightBox {

double FBDatalinkSystem::RadioHorizonM(double altAM, double altBM) {
  double a = std::max(0.0, altAM) * kMToFt, b = std::max(0.0, altBM) * kMToFt;
  return 1.23 * (std::sqrt(a) + std::sqrt(b)) * kNmToM;
}

namespace {
void CopyCallsign(char *dst, const std::string &src) {
  size_t n = std::min(src.size(), (size_t)kDatalinkCallsignLen - 1);
  std::memcpy(dst, src.data(), n);
  dst[n] = '\0';
}
} // namespace

/* One network cycle: the whole picture is rebuilt by walking the registry IN ORDER, so the track list
 * is always in mission-declaration order and never depends on who was heard first. A participant that
 * cannot be received right now is not deleted immediately — its previous message is carried over until
 * it ages past the drop threshold. */
void FBDatalinkSystem::Cycle(const fb_fdm_state &st, const FBUnitRegistry &net, double simTimeS) {
  FBDatalinkTrack fresh[kMaxDatalinkTracks];
  int n = 0;
  int flightIndex = -1;
  const double dropAgeS = kDropAfterCycles * kNetPeriodS;

  for (const FBUnit *u : net.Units()) {
    if (!u || u->GetTeam() != SelfTeam_) continue;   /* a cooperative net carries its own faction only */
    /* The net is a net of PARTICIPANTS: a released store belongs to the same faction but carries no
     * terminal, so it is not a member — and skipping it BEFORE the ordinal below matters, because the
     * flight index is what the FR/FL contact filter selects on. */
    if (u->GetKind() != FBUnitKind::Aircraft) continue;
    flightIndex++;                                   /* ordinal within the flight, self included */
    if (u->GetId() == SelfId_) continue;             /* own PPLI is not a track on own display */
    if (!AcceptContact(*u, flightIndex)) continue;   /* receiver-side contact filter (TNDL FR/FL) */
    if (n >= kMaxDatalinkTracks) break;

    FBUnitPose p = u->GetPose();
    double rangeM = FBPlanarDistM(st.lat, st.lon, p.LatDeg, p.LonDeg);
    bool heard = u->GetSignature().DatalinkXmt &&
                 rangeM <= std::min(MaxRangeM_, RadioHorizonM(st.elev, p.ElevM));

    FBDatalinkTrack *held = nullptr;
    for (int i = 0; i < TrackCount_; i++)
      if (Tracks_[i].UnitId == u->GetId()) { held = &Tracks_[i]; break; }

    if (heard) {
      FBDatalinkTrack &t = fresh[n++];
      t.UnitId = u->GetId();
      CopyCallsign(t.Callsign, u->GetName());
      t.Team = u->GetTeam();
      t.LatDeg = p.LatDeg; t.LonDeg = p.LonDeg;
      t.AltM = (float)p.ElevM;
      t.HeadingDeg = (float)p.HeadingDeg;
      t.SpeedMs = (float)p.SpeedMs;
      t.ReportTimeS = (float)simTimeS;
    } else if (held && simTimeS - held->ReportTimeS < dropAgeS) {
      fresh[n++] = *held;   /* hold the last message, position and timestamp unchanged */
    }
  }

  for (int i = 0; i < TrackCount_; i++) {
    bool kept = false;
    for (int j = 0; j < n; j++) if (fresh[j].UnitId == Tracks_[i].UnitId) { kept = true; break; }
    if (!kept)
      FBLog::Info("datalink", "TRACK_LOST", {{"id", Tracks_[i].UnitId},
          {"callsign", std::string(Tracks_[i].Callsign)},
          {"lastAgeS", simTimeS - Tracks_[i].ReportTimeS}});
  }
  for (int j = 0; j < n; j++) {
    bool had = false;
    for (int i = 0; i < TrackCount_; i++)
      if (Tracks_[i].UnitId == fresh[j].UnitId) { had = true; break; }
    if (!had)
      FBLog::Info("datalink", "TRACK_GAINED", {{"id", fresh[j].UnitId},
          {"callsign", std::string(fresh[j].Callsign)},
          {"distNm", FBPlanarDistM(st.lat, st.lon, fresh[j].LatDeg, fresh[j].LonDeg) * kMToNm}});
  }

  for (int i = 0; i < n; i++) Tracks_[i] = fresh[i];
  TrackCount_ = n;
}

void FBDatalinkSystem::Run(FBState &state, const fb_fdm_state &st, const FBUnitRegistry *net,
                           double simTimeS) {
  FBDatalinkBlock &b = state.Datalink;
  b.Powered = Powered_;
  b.Transmitting = Transmitting();

  if (!Powered_ || !net) {
    /* Terminal off (or no net at all): the receiver goes blind — see the header on XMT vs. POWER. */
    if (TrackCount_ > 0)
      for (int i = 0; i < TrackCount_; i++)
        FBLog::Info("datalink", "TRACK_LOST", {{"id", Tracks_[i].UnitId},
            {"callsign", std::string(Tracks_[i].Callsign)}, {"reason", "terminal off"}});
    TrackCount_ = 0;
    b.TrackCount = 0;
    /* A powered-down terminal receives nothing — the net picture is not "empty", it is absent. */
    b.H.Invalidate();
    NearestNm_ = NearestAgeS_ = -1.0f;
    return;
  }

  bool cycled = false;
  while (NextCycleS_ <= simTimeS) {
    Cycle(st, *net, simTimeS);
    NextCycleS_ += kNetPeriodS;
    cycled = true;
  }

  /* Between cycles the picture stands still and only its age moves — range/bearing are recomputed
   * against OWN new position (the display's own geometry), never against a newer sender position. */
  NearestNm_ = NearestAgeS_ = -1.0f;
  for (int i = 0; i < TrackCount_; i++) {
    FBDatalinkTrack &t = Tracks_[i];
    /* max(0): ReportTimeS is stored as float and simTimeS accumulates in double, so a message stamped
     * in this very cycle can read back a few hundred nanoseconds "in the future" — a negative age is
     * nonsense to a consumer, and clamping is cheaper than widening the track. */
    t.AgeS = std::max(0.0f, (float)(simTimeS - t.ReportTimeS));
    t.RangeM = (float)FBPlanarDistM(st.lat, st.lon, t.LatDeg, t.LonDeg);
    t.BearingDeg = (float)FBBearingDeg(st.lat, st.lon, t.LatDeg, t.LonDeg);
    b.Tracks[i] = t;
    float nm = t.RangeM * (float)kMToNm;
    if (NearestNm_ < 0.0f || nm < NearestNm_) { NearestNm_ = nm; NearestAgeS_ = t.AgeS; }
  }
  b.TrackCount = TrackCount_;
  /* Between net cycles the picture stands still and only its age moves (class banner) — Held, with the
   * stamp still naming the last cycle that actually delivered messages. */
  if (cycled) b.H.Publish(simTimeS);
  else b.H.Hold();
}

void FBDatalinkSystem::DeclareTelemetry(FBTelemetrySchema &schema) const {
  schema.Add("dl_on");
  schema.Add("dl_xmt");
  schema.Add("dl_tracks");
  schema.Add("dl_near", "nm");
  schema.Add("dl_age", "s");
}

void FBDatalinkSystem::SampleTelemetry(FBTelemetryRow &row) const {
  row.Push(Powered_);
  row.Push(Transmitting());
  row.Push(TrackCount_);
  row.Push((double)NearestNm_);
  row.Push((double)NearestAgeS_);
}

} // namespace FlightBox
