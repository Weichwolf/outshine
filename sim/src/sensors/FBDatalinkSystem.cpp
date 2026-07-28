#include "FBDatalinkSystem.h"
#include "FBGeodesy.h"
#include "FBLog.h"
#include "FBUnit.h"
#include "FBUnitRegistry.h"
#include "FBUnits.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace FlightBox::Sensors {

double FBDatalinkSystem::RadioHorizonM(double aglAM, double aglBM) {
  double a = std::max(0.0, aglAM) * kMToFt, b = std::max(0.0, aglBM) * kMToFt;
  return 1.23 * (std::sqrt(a) + std::sqrt(b)) * kNmToM;
}

namespace {
void CopyName(char *dst, const std::string &src, size_t cap) {
  size_t n = std::min(src.size(), cap - 1);
  std::memcpy(dst, src.data(), n);
  dst[n] = '\0';
}
} // namespace

void FBDatalinkSystem::SetControlNode(const std::string &callsign) {
  CopyName(ControlNode_, callsign, kDatalinkCallsignLen);
}

/* Ein Netzzyklus: das GANZE Bild wird neu gebaut, Registry IN REIHENFOLGE — die Trackliste haengt damit
 * nie daran, wer zuerst gehoert wurde. */
void FBDatalinkSystem::Cycle(const Fdm::fb_fdm_state &st, const Units::FBUnitRegistry &net, double simTimeS) {
  FBDatalinkTrack fresh[kMaxDatalinkTracks];
  int n = 0;
  int flightIndex = -1;
  const double dropAgeS = HoldCycles_ * NetPeriodS_;
  const double ownAglM = std::max(0.0, st.elev - OwnGroundAslM_) + MastM_;

  /* THE JAM TEST, in the function that already walks the registry and reading one more field off a
   * published signature — like DatalinkXmt and RcsM2 beside it. It is about the RECEIVER: barrage
   * jamming swamps a front end, and denying the transmitter would model a different physical thing.
   * A WIRE cannot be jammed, which is the whole reason the mode exists. */
  Jammed_ = false;
  if (Mode_ == LinkMode::Radio) {
    for (const Units::FBUnit *u : net.Units()) {
      if (!u || u->GetTeam() == SelfTeam_) continue;
      float rM = u->GetSignature().CommJamM;
      if (rM <= 0.0f) continue;
      Units::FBUnitPose jp = u->GetPose();
      if (FBPlanarDistM(st.lat, st.lon, jp.LatDeg, jp.LonDeg) <= rM) { Jammed_ = true; break; }
    }
  }

  for (const Units::FBUnit *u : net.Units()) {
    if (!u || u->GetTeam() != SelfTeam_) continue;   /* ein kooperatives Netz traegt nur die eigene Fraktion */
    /* Ein abgeworfener Store gehoert zur selben Fraktion, traegt aber kein Terminal — und der Test muss
     * VOR das Ordinal, weil der FR/FL-Filter genau darauf selektiert. Welche KINDS ein Terminal tragen
     * ist seit dem Netz eine Einstellung; der Default ist Aircraft allein, also genau wie zuvor. */
    const bool isAir = u->GetKind() == Units::FBUnitKind::Aircraft;
    if (!(isAir ? CarryAir_ : (CarryGround_ && u->GetKind() == Units::FBUnitKind::Ground))) continue;
    if (isAir) flightIndex++;                        /* Ordinal in der Rotte, eigene Einheit eingeschlossen */
    if (u->GetId() == SelfId_) continue;             /* die eigene PPLI ist kein Track auf dem eigenen Display */
    /* DER FILTER-INDEX ist die DEKLARIERTE Position, sobald die Mission eine erklaert — vorher war er
     * die Missionsreihenfolge, also ein Platzhalter fuer eine Fuehrung, die es nicht gab. Ohne
     * `flight`-Zeile bleibt es exakt der alte Ordinal (doc/formation.md 1). */
    int filterIndex = u->GetFlight().Declared() ? u->GetFlight().Position - 1 : flightIndex;
    if (!AcceptContact(*u, filterIndex)) continue;
    if (n >= kMaxDatalinkTracks) break;

    Units::FBUnitPose p = u->GetPose();
    double rangeM = FBPlanarDistM(st.lat, st.lon, p.LatDeg, p.LonDeg);
    /* Der Horizont gilt gegen die OBERFLAECHE unter beiden Antennen; ein vergrabenes Kabel kennt keinen.
     * Die Masthoehe ist die des NETZES (eine Deklaration, beide Enden), nicht die des Absenders — der
     * Empfaenger kann die fremde nicht messen und wuerde sie sonst erfinden. */
    double reachM = MaxRangeM_;
    if (Mode_ == LinkMode::Radio) {
      double theirAglM = std::max(0.0, p.ElevM - p.GroundAslM) + MastM_;
      reachM = std::min(MaxRangeM_, RadioHorizonM(ownAglM, theirAglM));
    }
    bool heard = !Jammed_ && u->GetSignature().DatalinkXmt && rangeM <= reachM;

    /* WHY THE NODE WENT QUIET, for the analyst and for nobody else: it is logged here and never enters
     * FBState, so the member goes on being unable to tell its Silent causes apart. The set is what this
     * RECEIVER measured — a node killed outright and a node whose terminal was shot away are the same
     * silence on the air, and claiming to distinguish them would mean reading the sender's health.
     * doc/air-defence-network.md §5. */
    if (ControlNode_[0] && std::strcmp(ControlNode_, u->GetName().c_str()) == 0) {
      if (heard != NodeHeard_) {
        if (heard) FBLog::Info("net", "JOIN", {{"node", u->GetName()}, {"distM", rangeM}});
        else FBLog::Warn("net", "LOST", {{"node", u->GetName()},
            {"reason", Jammed_ ? "jammed" : !u->GetSignature().DatalinkXmt ? "terminal" : "horizon"},
            {"distM", rangeM}, {"reachM", reachM}});
        NodeHeard_ = heard;
      }
    }

    FBDatalinkTrack *held = nullptr;
    for (int i = 0; i < TrackCount_; i++)
      if (Tracks_[i].UnitId == u->GetId()) { held = &Tracks_[i]; break; }

    if (heard) {
      FBDatalinkTrack &t = fresh[n++];
      t.UnitId = u->GetId();
      CopyName(t.Callsign, u->GetName(), kDatalinkCallsignLen);
      t.Team = u->GetTeam();
      t.LatDeg = p.LatDeg; t.LonDeg = p.LonDeg;
      t.AltM = (float)p.ElevM;
      t.HeadingDeg = (float)p.HeadingDeg;
      t.SpeedMs = (float)p.SpeedMs;
      t.ReportTimeS = (float)simTimeS;
      /* Die ROTTEN-Haelfte derselben Nachricht: Identitaet aus der Registry (wie Callsign und
       * Fraktion), Absicht aus der publizierten Signatur — beides nur, weil dies ein KOOPERATIVES
       * Netz ist. Ein Ziel wird als PUNKT gemeldet, nie als Einheit. */
      CopyName(t.FlightName, u->GetFlight().Name, kFlightNameLen);
      t.FlightPos = u->GetFlight().Position;
      t.Report = u->GetSignature().Flight;
      /* Die LUFTVERTEIDIGUNGS-Haelfte, unter derselben Regel und mit derselben Grenze: ein PUNKT, das
       * eigene Blickalter des Absenders und seine Waffenfreigabe. Kein Id-Feld, kein Team-Feld. */
      t.Net = u->GetSignature().Net;
    } else if (!Jammed_ && held && simTimeS - held->ReportTimeS < dropAgeS) {
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

void FBDatalinkSystem::Run(FBState &state, const Fdm::fb_fdm_state &st, const Units::FBUnitRegistry *net,
                           double simTimeS) {
  FBDatalinkBlock &b = Block(state);
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
    NextCycleS_ += NetPeriodS_;
    cycled = true;
  }

  /* EIN GESTOERTER EMPFAENGER hat kein Bild, nicht ein leeres — dieselbe Unterscheidung, die das
   * stromlose Terminal oben macht, und der Grund, warum der Halte-Zweig unter Stoerung nicht greift. */
  if (Jammed_) {
    b.TrackCount = 0;
    b.H.Invalidate();
    NearestNm_ = NearestAgeS_ = -1.0f;
    return;
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

} // namespace FlightBox::Sensors
