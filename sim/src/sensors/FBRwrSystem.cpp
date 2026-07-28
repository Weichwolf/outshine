#include "FBRwrSystem.h"
#include "FBGeodesy.h"
#include "FBLog.h"
#include "FBUnit.h"
#include "FBUnitRegistry.h"
#include <cmath>

namespace FlightBox::Sensors {

void FBRwrSystem::SetPowered(bool on) {
  if (Powered_ == on) return;
  Powered_ = on;
  if (!on) Count_ = 0;   /* ein wieder hochgefahrener Empfaenger startet aus der Stille */
}

/* Der Antennentest des SENDERS, vom Empfangsende gerechnet: dieselbe Transformation, dieselbe
 * Konvention, dieselbe Datei, die das Radar fuer SEINE Erfassung benutzt — die zwei Seiten einer
 * Bestrahlung koennen sich ueber die Geometrie nie uneinig sein. */
bool FBRwrSystem::BeamCovers(const FBEmitterSignature &sig, double rollDeg, double pitchDeg,
                             double yawDeg, double eastM, double northM, double upM) {
  double az = 0.0, el = 0.0;
  FBEnuToBodyLos(rollDeg, pitchDeg, yawDeg, eastM, northM, upM, az, el);
  return std::fabs(FBWrap180(az - sig.AzCenterDeg)) <= sig.AzHalfDeg &&
         std::fabs(el - sig.ElCenterDeg) <= sig.ElHalfDeg;
}

/* Ein Raketensucher ist der Startfall, egal wie er gerade scannt — was ihn an die Spitze der Skala
 * setzt, ist, was hinter der Antenne sitzt. */
FBRwrThreatMode FBRwrSystem::ModeOf(const FBEmitterSignature &sig, FBEmitterKind kind) {
  if (kind == FBEmitterKind::MissileSeeker) return FBRwrThreatMode::Missile;
  switch (sig.Mode) {
    case FBEmitterMode::Guidance: return FBRwrThreatMode::Missile;
    case FBEmitterMode::Track: return FBRwrThreatMode::Track;
    default: return FBRwrThreatMode::Search;
  }
}

double FBRwrSystem::Lethality(FBRwrThreatMode mode, double signalNorm) const {
  double base = mode == FBRwrThreatMode::Missile ? kLethalityMissile
              : mode == FBRwrThreatMode::Track ? kLethalityTrack : kLethalitySearch;
  double v = base + kLethalitySignalWeight * signalNorm;
  return v > 1.0 ? 1.0 : v;
}

void FBRwrSystem::Run(FBState &state, const Fdm::fb_fdm_state &st, const Units::FBUnitRegistry *net,
                      double simTimeS) {
  FBRwrBlock &b = state.Rwr;
  b.Powered = Powered_;
  if (!Powered_ || !net) {
    b.ThreatCount = 0;
    b.PriorityIndex = -1;
    b.MissileLaunch = b.Activity = b.HiddenSearch = false;
    /* „Nicht zuhoeren" ist nicht „nichts da draussen": ruhiger Himmel gegen tote Box. */
    b.H.Invalidate();
    Count_ = 0;
    ThreatCount_ = 0; PriorityMode_ = 0;
    TopBearingDeg_ = TopElDeg_ = TopLethality_ = 0.0f;
    Launch_ = Activity_ = NewThreat_ = false;
    BlockStatus_ = (int)b.H.Status;
    return;
  }

  for (int i = 0; i < Count_; i++) Threats_[i].Heard = false;

  /* Registry IN REIHENFOLGE, damit die Symbolnummer an der Missionsdeklaration haengt und nie daran,
   * wer zuerst strahlte. */
  for (const Units::FBUnit *u : net->Units()) {
    if (!u || u->GetId() == SelfId_) continue;   /* das eigene Set ist keine Bedrohung fuer sich selbst */
    Units::FBUnitSignature sig = u->GetSignature();
    if (sig.Radar.Mode == FBEmitterMode::None) continue;   /* stumme Einheit: nichts zu hoeren */

    Units::FBUnitPose p = u->GetPose();
    double e = 0.0, n = 0.0;
    FBEnuOffsetM(p.LatDeg, p.LonDeg, st.lat, st.lon, e, n);   /* emitter -> self */
    double up = st.elev - p.ElevM;
    double rangeM = std::sqrt(e * e + n * n + up * up);

    /* Hoerweite = das Tor des Senders mal dem Einwegvorteil. Diese Zahl verlaesst die Klasse NIE —
     * publiziert wird die Leistung, die sie erzeugt, nie die Entfernung dahinter. */
    double hearM = sig.Radar.RangeM * kBeamRangeFactor;
    if (hearM <= 0.0 || rangeM > hearM) continue;

    int slot = -1;
    for (int i = 0; i < Count_; i++)
      if (Threats_[i].UnitId == u->GetId()) { slot = i; break; }

    if (!BeamCovers(sig.Radar, p.RollDeg, p.PitchDeg, p.YawDeg, e, n, up)) continue;   /* zeigt woanders hin */

    /* Die Abdeckung des EMPFAENGERS: 360° Azimut, begrenzte Elevation — eine echte BLINDZONE, die das
     * eigene Manoevrieren aufreisst, samt einer Warnung, die man eine Sekunde zuvor noch hatte. */
    double rxAz = 0.0, rxEl = 0.0;
    FBEnuToBodyLos(st.roll, st.pitch, st.yaw, -e, -n, -up, rxAz, rxEl);
    if (std::fabs(rxEl) > ElevCoverageDeg()) {
      /* Nur der UEBERGANG ist eine Zeile wert: der Moment, in dem eine bestehende Warnung unhoerbar wurde. */
      if (slot >= 0 && !Threats_[slot].Blind) {
        Threats_[slot].Blind = true;
        FBLog::Info("rwr", "THREAT_BLIND", {{"symbol", Threats_[slot].Id},
            {"rxElDeg", rxEl}, {"limitDeg", ElevCoverageDeg()},
            {"mode", FBRwrThreatModeStr(Threats_[slot].Mode)}});
      }
      continue;
    }

    /* Empfangsleistung, normiert auf die Hoerweite: Einwegausbreitung, Leistung ~ 1/r². Die EINE
     * Naeherungsandeutung, die die Box hat. */
    double ratio = rangeM / hearM;
    double signal = 1.0 - ratio * ratio;
    if (signal < 0.0) signal = 0.0;

    FBEmitterKind kind = Classify(sig.Radar);
    FBRwrThreatMode mode = ModeOf(sig.Radar, kind);

    if (slot < 0) {
      if (Count_ >= kMaxRwrThreats) continue;   /* Tabelle voll; nichts wird verdraengt */
      slot = Count_++;
      Threats_[slot] = Threat{};
      Threats_[slot].Id = NextId_++;
      Threats_[slot].UnitId = u->GetId();
      Threats_[slot].FirstS = simTimeS;
      Threats_[slot].Mode = mode;
      FBLog::Info("rwr", "THREAT_NEW", {{"symbol", Threats_[slot].Id}, {"mode", FBRwrThreatModeStr(mode)},
          {"kind", FBEmitterKindStr(kind)}, {"brgDeg", FBWrap180(rxAz)}, {"elDeg", rxEl},
          {"signal", signal}});
    } else if (Threats_[slot].Mode != mode) {
      FBLog::Info("rwr", "THREAT_MODE", {{"symbol", Threats_[slot].Id},
          {"from", FBRwrThreatModeStr(Threats_[slot].Mode)}, {"to", FBRwrThreatModeStr(mode)},
          {"kind", FBEmitterKindStr(kind)}, {"brgDeg", FBWrap180(rxAz)}, {"signal", signal}});
    }

    Threat &t = Threats_[slot];
    t.BearingDeg = FBWrap180(rxAz);
    t.ElDeg = rxEl;
    t.SignalNorm = signal;
    t.Mode = mode;
    t.Kind = kind;
    t.LastS = simTimeS;
    t.Heard = true;
    t.Blind = false;
  }

  /* Halten, dann fallen lassen: eine wegstreichende Keule soll das Display nicht blinken lassen. */
  for (int i = 0; i < Count_;) {
    Threat &t = Threats_[i];
    if (t.Heard || simTimeS - t.LastS < kHoldS) { i++; continue; }
    FBLog::Info("rwr", "THREAT_DROP", {{"symbol", t.Id}, {"mode", FBRwrThreatModeStr(t.Mode)},
        {"heldS", simTimeS - t.LastS}});
    for (int j = i; j < Count_ - 1; j++) Threats_[j] = Threats_[j + 1];
    Count_--;
  }

  Publish(state, simTimeS);
}

/* Rangfolge: erst MODUS (Enum-Ordnung), dann Empfangsleistung, Tiebreak = Tabellenreihenfolge — so
 * flackert das Prioritaetssymbol nicht zwischen zwei gleichwertigen Bedrohungen. */
void FBRwrSystem::Publish(FBState &state, double simTimeS) {
  FBRwrBlock &b = state.Rwr;
  int order[kMaxRwrThreats];
  int n = 0;
  bool hiddenSearch = false;
  for (int i = 0; i < Count_; i++) {
    if (!SearchShown_ && Threats_[i].Mode == FBRwrThreatMode::Search) { hiddenSearch = true; continue; }
    order[n++] = i;
  }
  for (int i = 1; i < n; i++) {   /* Insertion Sort: n <= 8, keine Allokation, stabil */
    int key = order[i];
    int j = i - 1;
    while (j >= 0 && (Threats_[order[j]].Mode < Threats_[key].Mode ||
                      (Threats_[order[j]].Mode == Threats_[key].Mode &&
                       Threats_[order[j]].SignalNorm < Threats_[key].SignalNorm))) {
      order[j + 1] = order[j];
      j--;
    }
    order[j + 1] = key;
  }
  int cap = MaxDisplayed();
  if (n > cap) n = cap;

  bool launch = false, activity = false, anyNew = false;
  for (int i = 0; i < n; i++) {
    const Threat &t = Threats_[order[i]];
    FBRwrThreat &out = b.Threats[i];
    out.Id = t.Id;
    out.BearingDeg = (float)t.BearingDeg;
    out.ElDeg = (float)t.ElDeg;
    out.SignalNorm = (float)t.SignalNorm;
    out.LethalityNorm = (float)Lethality(t.Mode, t.SignalNorm);
    out.AgeS = (float)std::fmax(0.0, simTimeS - t.LastS);
    out.Mode = t.Mode;
    out.Kind = t.Kind;
    out.New = simTimeS - t.FirstS < kNewThreatS;
    if (out.New) anyNew = true;
    if (t.Mode == FBRwrThreatMode::Missile) launch = true;
    if (t.Mode != FBRwrThreatMode::Search) activity = true;
  }
  b.ThreatCount = n;
  b.PriorityIndex = n > 0 ? 0 : -1;   /* die Rangfolge oben hat sie nach vorn sortiert */
  b.MissileLaunch = launch;
  b.Activity = activity;
  b.HiddenSearch = hiddenSearch;
  b.H.Publish(simTimeS);

  ThreatCount_ = n;
  PriorityMode_ = n > 0 ? (int)b.Threats[0].Mode : -1;
  TopBearingDeg_ = n > 0 ? b.Threats[0].BearingDeg : 0.0f;
  TopElDeg_ = n > 0 ? b.Threats[0].ElDeg : 0.0f;
  TopLethality_ = n > 0 ? b.Threats[0].LethalityNorm : 0.0f;
  Launch_ = launch;
  Activity_ = activity;
  NewThreat_ = anyNew;
  BlockStatus_ = (int)b.H.Status;
}

void FBRwrSystem::DeclareTelemetry(FBTelemetrySchema &schema) const {
  /* ERSTE Spalte ist die Blockgueltigkeit: FBStateBusTelemetrys Liste sitzt in der MITTE jeder je
   * gemessenen telemetry.csv, ein Name mehr dort verschoebe jede Spalte rechts davon. */
  schema.Add("blk_rwr");
  schema.Add("rwr_on");
  schema.Add("rwr_threats");
  schema.Add("rwr_mode");            /* Modus-Ordinal der Prioritaetsbedrohung, -1 = nichts gehoert */
  schema.Add("rwr_brg", "deg");      /* relativ zur eigenen Nase */
  schema.Add("rwr_el", "deg");
  schema.Add("rwr_leth");
  schema.Add("rwr_new");
  schema.Add("rwr_launch");
  schema.Add("rwr_act");
}

void FBRwrSystem::SampleTelemetry(FBTelemetryRow &row) const {
  row.Push(BlockStatus_);
  row.Push(Powered_);
  row.Push(ThreatCount_);
  row.Push(PriorityMode_);
  row.Push((double)TopBearingDeg_);
  row.Push((double)TopElDeg_);
  row.Push((double)TopLethality_);
  row.Push(NewThreat_);
  row.Push(Launch_);
  row.Push(Activity_);
}

} // namespace FlightBox::Sensors
