#include "FBRwrSystem.h"
#include "FBGeodesy.h"
#include "FBLog.h"
#include "FBUnit.h"
#include "FBUnitRegistry.h"
#include <cmath>

namespace FlightBox {

void FBRwrSystem::SetPowered(bool on) {
  if (Powered_ == on) return;
  Powered_ = on;
  if (!on) Count_ = 0;   /* a receiver that comes back up starts from silence, not from a stale picture */
}

/* The emitter's own antenna test, run from the receiving end: rotate the line of sight from the EMITTER
 * to this aircraft into the EMITTER's body frame (its published pose carries the attitude) and ask
 * whether it falls inside the beam window it is radiating. Same transform, same convention, same file as
 * the radar uses to decide what IT sees — which is the point: the two sides of an illumination can never
 * disagree about the geometry. */
bool FBRwrSystem::BeamCovers(const FBEmitterSignature &sig, double rollDeg, double pitchDeg,
                             double yawDeg, double eastM, double northM, double upM) {
  double az = 0.0, el = 0.0;
  FBEnuToBodyLos(rollDeg, pitchDeg, yawDeg, eastM, northM, upM, az, el);
  return std::fabs(FBWrap180(az - sig.AzCenterDeg)) <= sig.AzHalfDeg &&
         std::fabs(el - sig.ElCenterDeg) <= sig.ElHalfDeg;
}

/* doc/f16/defence-rwr-cm.md §1's symbol table, as the three things a receiver distinguishes. A missile's
 * own seeker is the launch case however it happens to be scanning — what makes it the top of the scale
 * is what is behind the antenna. */
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

void FBRwrSystem::Run(FBState &state, const fb_fdm_state &st, const FBUnitRegistry *net,
                      double simTimeS) {
  FBRwrBlock &b = state.Rwr;
  b.Powered = Powered_;
  if (!Powered_ || !net) {
    b.ThreatCount = 0;
    b.PriorityIndex = -1;
    b.MissileLaunch = b.Activity = b.HiddenSearch = false;
    /* Not listening is not "nothing out there" — the difference is the whole reason a block has a head
     * (core/FBBlockStatus.h), and here it is the difference between a quiet sky and a dead box. */
    b.H.Invalidate();
    Count_ = 0;
    ThreatCount_ = 0; PriorityMode_ = 0;
    TopBearingDeg_ = TopElDeg_ = TopLethality_ = 0.0f;
    Launch_ = Activity_ = NewThreat_ = false;
    BlockStatus_ = (int)b.H.Status;
    return;
  }

  for (int i = 0; i < Count_; i++) Threats_[i].Heard = false;

  /* The registry is walked IN ORDER, so a fresh detection's symbol number depends on the mission's
   * declaration order and never on who happened to radiate first. */
  for (const FBUnit *u : net->Units()) {
    if (!u || u->GetId() == SelfId_) continue;   /* own set is not a threat to itself */
    FBUnitSignature sig = u->GetSignature();
    if (sig.Radar.Mode == FBEmitterMode::None) continue;   /* silent unit: nothing to hear */

    FBUnitPose p = u->GetPose();
    double e = 0.0, n = 0.0;
    FBEnuOffsetM(p.LatDeg, p.LonDeg, st.lat, st.lon, e, n);   /* emitter -> self */
    double up = st.elev - p.ElevM;
    double rangeM = std::sqrt(e * e + n * n + up * up);

    /* How far this beam is heard: the emitter's own gate stretched by the one-way path advantage (see
     * kBeamRangeFactor). Nothing about this number leaves the class — what is published is the POWER it
     * produces, never the distance behind it. */
    double hearM = sig.Radar.RangeM * kBeamRangeFactor;
    if (hearM <= 0.0 || rangeM > hearM) continue;

    int slot = -1;
    for (int i = 0; i < Count_; i++)
      if (Threats_[i].UnitId == u->GetId()) { slot = i; break; }

    if (!BeamCovers(sig.Radar, p.RollDeg, p.PitchDeg, p.YawDeg, e, n, up)) continue;   /* pointed somewhere else: not heard at all */

    /* The RECEIVER's own coverage: 360 degrees around, but only so far above and below the fuselage
     * plane (doc/f16/defence-rwr-cm.md §2.1's +-45 deg elevation limit on the F-16's set). A beam that
     * arrives outside it is genuinely inaudible — including one that was audible a second ago, which is
     * how a manoeuvre drops a lock warning without the threat having done anything. */
    double rxAz = 0.0, rxEl = 0.0;
    FBEnuToBodyLos(st.roll, st.pitch, st.yaw, -e, -n, -up, rxAz, rxEl);
    if (std::fabs(rxEl) > ElevCoverageDeg()) {
      /* Logged on the TRANSITION only: a threat that stays in the hole is one event, not one per tick,
       * and what is worth a line is the moment a warning that existed stopped being audible. */
      if (slot >= 0 && !Threats_[slot].Blind) {
        Threats_[slot].Blind = true;
        FBLog::Info("rwr", "THREAT_BLIND", {{"symbol", Threats_[slot].Id},
            {"rxElDeg", rxEl}, {"limitDeg", ElevCoverageDeg()},
            {"mode", FBRwrThreatModeStr(Threats_[slot].Mode)}});
      }
      continue;
    }

    /* Received power, normalised against what this receiver can hear at all: one-way propagation, so
     * power goes as 1/r^2 and the figure is 1 at zero range and 0 where the signal disappears into the
     * noise. This is the ONE proximity cue the box has (core/FBRwrThreat.h). */
    double ratio = rangeM / hearM;
    double signal = 1.0 - ratio * ratio;
    if (signal < 0.0) signal = 0.0;

    FBEmitterKind kind = Classify(sig.Radar);
    FBRwrThreatMode mode = ModeOf(sig.Radar, kind);

    if (slot < 0) {
      if (Count_ >= kMaxRwrThreats) continue;   /* the table is full; nothing is displaced */
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

  /* Hold, then drop: an emission that stops being heard keeps its symbol for kHoldS (class banner), so
   * a beam sweeping past does not blink the display. */
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

/* The display: the detected set, ranked, filtered and capped. Ranking is by MODE first (a tracking
 * radar outranks any number of searching ones — the scale core/FBRwrThreat.h's enum order defines) and
 * by received power within a mode, with the table's own order as the tie-break, so the priority symbol
 * never flickers between two equal threats. */
void FBRwrSystem::Publish(FBState &state, double simTimeS) {
  FBRwrBlock &b = state.Rwr;
  int order[kMaxRwrThreats];
  int n = 0;
  bool hiddenSearch = false;
  for (int i = 0; i < Count_; i++) {
    if (!SearchShown_ && Threats_[i].Mode == FBRwrThreatMode::Search) { hiddenSearch = true; continue; }
    order[n++] = i;
  }
  for (int i = 1; i < n; i++) {   /* insertion sort: n <= 8, no allocation, stable */
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
  b.PriorityIndex = n > 0 ? 0 : -1;   /* the ranking above put it first */
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
  /* FIRST column is this source's block validity: the RWR block was added to FBState after
   * core/FBStateBusTelemetry's list was already sitting in the middle of every measured telemetry.csv,
   * and appending a name there would move every column to its right (see that file's banner). */
  schema.Add("blk_rwr");
  schema.Add("rwr_on");
  schema.Add("rwr_threats");
  schema.Add("rwr_mode");            /* the priority threat's mode ordinal, -1 = nothing heard */
  schema.Add("rwr_brg", "deg");      /* relative to own nose */
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

} // namespace FlightBox
