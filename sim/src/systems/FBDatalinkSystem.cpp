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

/* Ein Netzzyklus: das GANZE Bild wird neu gebaut, Registry IN REIHENFOLGE — die Trackliste haengt damit
 * nie daran, wer zuerst gehoert wurde. */
void FBDatalinkSystem::Cycle(const fb_fdm_state &st, const FBUnitRegistry &net, double simTimeS) {
  FBDatalinkTrack fresh[kMaxDatalinkTracks];
  int n = 0;
  int flightIndex = -1;
  const double dropAgeS = kDropAfterCycles * kNetPeriodS;

  for (const FBUnit *u : net.Units()) {
    if (!u || u->GetTeam() != SelfTeam_) continue;   /* ein kooperatives Netz traegt nur die eigene Fraktion */
    /* Ein abgeworfener Store gehoert zur selben Fraktion, traegt aber kein Terminal — und der Test muss
     * VOR das Ordinal, weil der FR/FL-Filter genau darauf selektiert. */
    if (u->GetKind() != FBUnitKind::Aircraft) continue;
    flightIndex++;                                   /* Ordinal in der Rotte, eigene Einheit eingeschlossen */
    if (u->GetId() == SelfId_) continue;             /* die eigene PPLI ist kein Track auf dem eigenen Display */
    if (!AcceptContact(*u, flightIndex)) continue;
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
      fresh[n++] = *held;   /* letzte Nachricht halten, Position und Zeitstempel unveraendert */
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
    if (TrackCount_ > 0)
      for (int i = 0; i < TrackCount_; i++)
        FBLog::Info("datalink", "TRACK_LOST", {{"id", Tracks_[i].UnitId},
            {"callsign", std::string(Tracks_[i].Callsign)}, {"reason", "terminal off"}});
    TrackCount_ = 0;
    b.TrackCount = 0;
    /* Ein stromloses Terminal empfaengt nichts — das Netzbild ist nicht „leer", es ist ABWESEND. */
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

  /* Zwischen den Zyklen bewegt sich nur das ALTER — Entfernung/Peilung gegen die EIGENE neue Position,
   * nie gegen eine neuere Absenderposition. */
  NearestNm_ = NearestAgeS_ = -1.0f;
  for (int i = 0; i < TrackCount_; i++) {
    FBDatalinkTrack &t = Tracks_[i];
    /* max(0): ReportTimeS ist float, simTimeS double — eine im selben Zyklus gestempelte Nachricht
     * laege sonst um Nanosekunden „in der Zukunft". */
    t.AgeS = std::max(0.0f, (float)(simTimeS - t.ReportTimeS));
    t.RangeM = (float)FBPlanarDistM(st.lat, st.lon, t.LatDeg, t.LonDeg);
    t.BearingDeg = (float)FBBearingDeg(st.lat, st.lon, t.LatDeg, t.LonDeg);
    b.Tracks[i] = t;
    float nm = t.RangeM * (float)kMToNm;
    if (NearestNm_ < 0.0f || nm < NearestNm_) { NearestNm_ = nm; NearestAgeS_ = t.AgeS; }
  }
  b.TrackCount = TrackCount_;
  /* Held statt Publish: der Zeitstempel nennt weiter den letzten Zyklus, der wirklich lieferte. */
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
