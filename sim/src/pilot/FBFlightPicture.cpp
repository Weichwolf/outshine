#include "FBFlightPicture.h"
#include "FBGeodesy.h"
#include "FBLog.h"
#include "FBUnits.h"
#include <cmath>
#include <cstring>

namespace FlightBox::Pilot {

bool FBSortContractFromString(const char *s, FBSortContract &out) {
  if (!std::strcmp(s, "none"))  { out = FBSortContract::None;  return true; }
  if (!std::strcmp(s, "left"))  { out = FBSortContract::Left;  return true; }
  if (!std::strcmp(s, "right")) { out = FBSortContract::Right; return true; }
  if (!std::strcmp(s, "near"))  { out = FBSortContract::Near;  return true; }
  if (!std::strcmp(s, "far"))   { out = FBSortContract::Far;   return true; }
  return false;
}

namespace {

/* An echo placed in the WORLD: bearing and elevation angle are the block's world-referenced pair, so
 * this needs no un-rotation of a look-old body vector through a now-current attitude.
 */
void ContactPoint(const Fdm::fb_fdm_state &st, const FBRadarContact &c, double &latDeg, double &lonDeg,
                  double &altM) {
  double h = c.BearingDeg * kDeg2Rad;
  double coslat = std::cos(st.lat * kDeg2Rad);
  double gnd = c.RangeM * std::cos(c.ElevAngleDeg * kDeg2Rad);
  latDeg = st.lat + gnd * std::cos(h) / kMPerDeg;
  lonDeg = st.lon + gnd * std::sin(h) / (kMPerDeg * (std::fabs(coslat) > 1e-6 ? coslat : 1e-6));
  altM = st.elev + c.RangeM * std::sin(c.ElevAngleDeg * kDeg2Rad);
}

} // namespace

/* The flight as this pilot has it: himself out of his own state, everybody else out of a message. The
 * two are deliberately the SAME type — the only difference is the age, and that difference is the
 * whole reason a station is held on a report rather than on a position. */
void FBFlightPicture::BuildMembers(const FBState &state, const Fdm::fb_fdm_state &st) {
  MemberCount_ = 0;
  LeadIdx_ = SelfIdx_ = -1;
  if (!Self_.Declared()) return;

  FBFlightMember &me = Members_[MemberCount_++];
  me = FBFlightMember{};
  me.Self = true;
  me.Position = Self_.Position;
  me.LatDeg = st.lat; me.LonDeg = st.lon; me.AltM = st.elev;
  me.HeadingDeg = st.yaw; me.SpeedMs = st.speed;
  me.Report = Report_;
  SelfIdx_ = 0;
  if (Self_.IsLead()) LeadIdx_ = 0;

  const FBDatalinkBlock &dl = state.Datalink;
  if (!dl.H.Readable()) return;
  for (int i = 0; i < dl.TrackCount && MemberCount_ < kMaxMembers; i++) {
    const FBDatalinkTrack &t = dl.Tracks[i];
    if (t.FlightPos <= 0 || std::strcmp(t.FlightName, Self_.Name.c_str()) != 0) continue;
    FBFlightMember &m = Members_[MemberCount_++];
    m = FBFlightMember{};
    m.UnitId = t.UnitId;
    m.Position = t.FlightPos;
    m.LatDeg = t.LatDeg; m.LonDeg = t.LonDeg; m.AltM = t.AltM;
    m.HeadingDeg = t.HeadingDeg; m.SpeedMs = t.SpeedMs;
    m.AgeS = t.AgeS;
    m.Report = t.Report;
    /* The LEAD is position 1 and nothing else — a flight without one is a parse error, so a missing
     * lead here means only that his terminal is not being heard. */
    if (m.Position == 1) LeadIdx_ = MemberCount_ - 1;
  }
}

/* TIME TO A SHOT, and it is the whole cost function: a fighter's job against a contact is to point at
 * it and close to its own commit range, so the two terms are the turn and the run.
 *   t = |ATA| / omega  +  max(0, R - Rcommit) / V
 * Every quantity is either measured off this pilot's own picture (the contact) or off a PPLI (the
 * member). What a peer's OWN commit range and turn rate are cannot be known, so this pilot assumes his
 * mates fly as he does — the same modelling choice FBTrackDatum already makes about the OPPONENT. */
double FBFlightPicture::CostS(const FBFlightMember &m, double tgtLat, double tgtLon,
                              const FBSortParams &p) const {
  double brg = FBBearingDeg(m.LatDeg, m.LonDeg, tgtLat, tgtLon);
  double ata = std::fabs(FBWrap180(brg - m.HeadingDeg));
  double omega = p.TurnRateDegS > 0.1 ? p.TurnRateDegS : 0.1;
  double rangeM = FBPlanarDistM(m.LatDeg, m.LonDeg, tgtLat, tgtLon);
  double v = m.SpeedMs > 50.0 ? m.SpeedMs : 50.0;
  return ata / omega + std::fmax(0.0, rangeM - p.CommitRangeM) / v;
}

/* THE BRIEFED CONTRACT applied to the picture this pilot actually has. Left/right are measured against
 * the FLIGHT's axis, not against this jet's nose: a rule anchored to one's own heading would mean
 * something different to every member and could not be a contract. */
int FBFlightPicture::ContractPick(const FBState &state, const FBSortParams &p) const {
  if (Contract_ == FBSortContract::None || !Self_.Declared()) return 0;
  const FBRadarBlock &r = state.Radar;
  if (!r.H.Readable()) return 0;
  /* WHICH END of the contract this member takes: the lead takes the first-named end, each wingman the
   * next one along. With two contacts and a two-ship that is exactly "he takes left, I take right". */
  int rank = Self_.Position - 1;

  int order[kMaxRadarContacts];
  int n = 0;
  for (int i = 0; i < r.ContactCount; i++) {
    if (r.Contacts[i].Iff == FBIffReply::Friendly) continue;
    order[n++] = i;
  }
  if (n == 0) return 0;
  auto key = [&](int idx) {
    const FBRadarContact &c = r.Contacts[idx];
    switch (Contract_) {
      case FBSortContract::Left:  return  FBWrap180(c.BearingDeg - p.AxisDeg);
      case FBSortContract::Right: return -FBWrap180(c.BearingDeg - p.AxisDeg);
      case FBSortContract::Near:  return  (double)c.RangeM;
      case FBSortContract::Far:   return -(double)c.RangeM;
      case FBSortContract::None:  break;
    }
    return 0.0;
  };
  for (int a = 0; a < n; a++)   /* insertion sort: n <= 8, and the order must be exactly reproducible */
    for (int b = a + 1; b < n; b++) {
      double ka = key(order[a]), kb = key(order[b]);
      if (kb < ka || (kb == ka && order[b] < order[a])) { int t = order[a]; order[a] = order[b]; order[b] = t; }
    }
  /* More members than contacts: the surplus doubles up on the last one rather than going home. */
  int take = rank < n ? rank : n - 1;
  return r.Contacts[order[take]].TrackNum;
}

/* THE ASSIGNMENT. Three information levels, applied in order of how much they are worth:
 *   1. what a mate SAYS he is on (his report, correlated against MY contacts by position),
 *   2. what the flight can WORK OUT from the shared picture (greedy minimum-cost matching),
 *   3. what was AGREED before takeoff (the contract), when there is no shared picture at all.
 * Every member runs the identical computation on the same data, so a cooperative flight reaches the
 * same matching without exchanging a single word about it. */
int FBFlightPicture::Assign(const FBState &state, const Fdm::fb_fdm_state &st, double nowS,
                            const FBSortParams &p) {
  (void)nowS;
  const FBRadarBlock &r = state.Radar;
  Duplicate_ = false;
  FreeContacts_ = 0;
  if (!Self_.Declared()) { Src_ = FBSortSource::None; return 0; }

  /* NO SHARED PICTURE — nobody else's terminal is being heard. Then the only thing left is what was
   * agreed before takeoff, and a flight without a contract sorts nothing at all. */
  if (MemberCount_ <= 1) {
    int pick = ContractPick(state, p);
    Src_ = pick != 0 ? FBSortSource::Contract : FBSortSource::None;
    return pick;
  }
  if (!r.H.Readable() || r.ContactCount == 0) { Src_ = FBSortSource::None; return 0; }

  double cLat[kMaxRadarContacts], cLon[kMaxRadarContacts], cAlt[kMaxRadarContacts];
  bool live[kMaxRadarContacts] = {};
  int claimedBy[kMaxRadarContacts];
  for (int i = 0; i < r.ContactCount; i++) {
    claimedBy[i] = -1;
    if (r.Contacts[i].Iff == FBIffReply::Friendly) continue;
    live[i] = true;
    ContactPoint(st, r.Contacts[i], cLat[i], cLon[i], cAlt[i]);
  }

  /* 1. THE DECLARATIONS, and only from a SENIOR member. A mate reports a POINT, never a track: this
   * jet correlates it against its own echoes and may fail to, which is the honest failure mode of a
   * shared picture. The gate grows with the report's age because the point it names keeps moving while
   * the message travels.
   *
   * WHY SENIORITY AND NOT SYMMETRY. Two members that each yield to the other's LAST message are a
   * feedback loop whose delay is the net cycle, and it oscillates at exactly that period: [MESS,
   * pair-2v2-f16, first cut] both vipers swapped targets every 1.0 s for sixty consecutive cycles,
   * each believing the other was on the one it had just left. A flight resolves that the way a flight
   * resolves everything — the LEAD's call stands and the wingman takes what is left. So a claim counts
   * only from a lower position number, and the lead honours nobody: his matching IS the flight's, and
   * it moves only when the geometry does.
   *
   * AND ONLY WHEN IT IS UNAMBIGUOUS. Two aircraft in combat spread are one spread apart while a report
   * is as stale as the net cycle; if the second-best echo is nearly as close as the best, the
   * correlation is a coin toss, and a coin toss is not information. Then nothing is claimed and both
   * members fall back on the matching, which they compute identically anyway. */
  bool assigned[kMaxMembers] = {};
  int  memberTrack[kMaxMembers] = {};
  int  selfPos = SelfIdx_ >= 0 ? Members_[SelfIdx_].Position : 0;
  for (int m = 0; m < MemberCount_; m++) {
    if (m == SelfIdx_ || !Members_[m].Report.Engaging) continue;
    const FBFlightMember &mate = Members_[m];
    if (mate.Position >= selfPos) continue;
    double gate = kCorrelateBaseM + mate.AgeS * kCorrelateSpeedMs;
    int best = -1; double bestD = gate, nextD = gate;
    for (int i = 0; i < r.ContactCount; i++) {
      if (!live[i] || claimedBy[i] >= 0) continue;
      double dh = FBPlanarDistM(mate.Report.TgtLatDeg, mate.Report.TgtLonDeg, cLat[i], cLon[i]);
      double dv = cAlt[i] - mate.Report.TgtAltM;
      double d = std::sqrt(dh * dh + dv * dv);   /* two contacts can share a bearing and differ by a block */
      if (d < bestD) { nextD = bestD; bestD = d; best = i; }
      else if (d < nextD) { nextD = d; }
    }
    if (best >= 0 && bestD < kCorrelateUniqueFrac * nextD) {
      claimedBy[best] = m; assigned[m] = true; memberTrack[m] = r.Contacts[best].TrackNum;
    }
  }

  /* 2. THE MATCHING over what is left, greedy on the cost above and strictly ordered so every member
   * computes the same answer: lowest cost first, ties by flight position and then by track number. */
  for (;;) {
    int bestM = -1, bestC = -1; double bestCost = 0.0;
    for (int m = 0; m < MemberCount_; m++) {
      if (assigned[m]) continue;
      for (int i = 0; i < r.ContactCount; i++) {
        if (!live[i] || claimedBy[i] >= 0) continue;
        double cost = CostS(Members_[m], cLat[i], cLon[i], p);
        bool better = bestM < 0 || cost < bestCost ||
                      (cost == bestCost && (Members_[m].Position < Members_[bestM].Position ||
                                            (Members_[m].Position == Members_[bestM].Position &&
                                             r.Contacts[i].TrackNum < r.Contacts[bestC].TrackNum)));
        if (better) { bestCost = cost; bestM = m; bestC = i; }
      }
    }
    if (bestM < 0) break;
    claimedBy[bestC] = bestM; assigned[bestM] = true; memberTrack[bestM] = r.Contacts[bestC].TrackNum;
  }

  /* 3. MORE SHOOTERS THAN TARGETS: nobody goes home while something is unengaged, so the surplus takes
   * the contact it reaches soonest even though somebody is already on it. */
  for (int m = 0; m < MemberCount_; m++) {
    if (assigned[m]) continue;
    int best = -1; double bestCost = 0.0;
    for (int i = 0; i < r.ContactCount; i++) {
      if (!live[i]) continue;
      double cost = CostS(Members_[m], cLat[i], cLon[i], p);
      if (best < 0 || cost < bestCost) { bestCost = cost; best = i; }
    }
    if (best >= 0) { assigned[m] = true; memberTrack[m] = r.Contacts[best].TrackNum; }
  }

  for (int i = 0; i < r.ContactCount; i++) if (live[i] && claimedBy[i] < 0) FreeContacts_++;

  int mine = SelfIdx_ >= 0 ? memberTrack[SelfIdx_] : 0;
  for (int m = 0; m < MemberCount_; m++)
    if (m != SelfIdx_ && mine != 0 && memberTrack[m] == mine) Duplicate_ = true;
  Src_ = mine != 0 ? FBSortSource::Cooperative : FBSortSource::None;

  /* HYSTERESIS. The turn is already inside the cost, so what a swap costs on top of it is the settle
   * time of the new single-target track — a swap that saves less than that buys nothing. */
  if (AssignTrack_ != 0 && mine != AssignTrack_) {
    int oldIdx = -1, newIdx = -1;
    for (int i = 0; i < r.ContactCount; i++) {
      if (!live[i]) continue;
      if (r.Contacts[i].TrackNum == AssignTrack_) oldIdx = i;
      if (r.Contacts[i].TrackNum == mine) newIdx = i;
    }
    if (oldIdx >= 0 && newIdx >= 0 && (claimedBy[oldIdx] < 0 || claimedBy[oldIdx] == SelfIdx_)) {
      double keep = CostS(Members_[SelfIdx_], cLat[oldIdx], cLon[oldIdx], p);
      double take = CostS(Members_[SelfIdx_], cLat[newIdx], cLon[newIdx], p);
      if (keep - take < p.SwitchMarginS + kMinSwitchGainS) mine = AssignTrack_;
    }
  }
  return mine;
}

void FBFlightPicture::Update(const FBState &state, const Fdm::fb_fdm_state &st, double nowS, double dt,
                             bool bound, bool threatened, const FBSortParams &p) {
  Report_.Bound = bound;
  SelfBound_ = bound;
  /* Der Stationsfehler ist eine Messung DIESES Ticks: wer gerade keine Station haelt, hat keinen —
   * ein stehengebliebener letzter Wert saehe aus wie ein gehaltener. */
  StationErrM_ = -1.0;
  if (!Self_.Declared()) {
    MemberCount_ = 0; LeadIdx_ = SelfIdx_ = -1;
    AssignTrack_ = 0; Src_ = FBSortSource::None; MateBound_ = false;
    return;
  }

  BuildMembers(state, st);

  MateBound_ = false;
  for (int m = 0; m < MemberCount_; m++)
    if (m != SelfIdx_ && Members_[m].Report.Bound) MateBound_ = true;

  int want = Assign(state, st, nowS, p);
  if (want != AssignTrack_) {
    if (want != 0) {
      Switches_++;
      FBLog::Info("flight", "SORT_ASSIGN", {{"t", nowS}, {"track", want}, {"was", AssignTrack_},
          {"src", std::string(Src_ == FBSortSource::Cooperative ? "cooperative" : "contract")},
          {"mates", MateCount()}, {"free", FreeContacts_}, {"dup", Duplicate_ ? 1 : 0}});
    } else if (AssignTrack_ != 0) {
      FBLog::Info("flight", "SORT_DROP", {{"t", nowS}, {"was", AssignTrack_}, {"mates", MateCount()}});
    }
    AssignTrack_ = want;
  }

  if (SelfBound_ && MateBound_) BothBoundS_ += dt;
  if (SelfBound_ && !MateBound_ && MateCount() > 0) CoverS_ += dt;
  if (SelfBound_ && threatened) ExposedS_ += dt;
}

void FBFlightPicture::SetOwnEngagement(bool engaging, double latDeg, double lonDeg, double altM) {
  Report_.Engaging = engaging;
  Report_.TgtLatDeg = latDeg;
  Report_.TgtLonDeg = lonDeg;
  Report_.TgtAltM = (float)altM;
}

void FBFlightPicture::DeclareTelemetry(FBTelemetrySchema &schema) const {
  schema.Add("flt_pos");
  schema.Add("flt_mates");
  schema.Add("flt_src");        /* 0 none, 1 cooperative, 2 contract */
  schema.Add("flt_assign");
  schema.Add("flt_switch");
  schema.Add("flt_dup");
  schema.Add("flt_free");
  schema.Add("flt_bound");
  schema.Add("flt_mate_bound");
  schema.Add("flt_both_s", "s");
  schema.Add("flt_cover_s", "s");
  schema.Add("flt_exposed_s", "s");
  schema.Add("flt_defer_s", "s");
  schema.Add("flt_sta", "m");
}

void FBFlightPicture::SampleTelemetry(FBTelemetryRow &row) const {
  row.Push(Self_.Position);
  row.Push(MateCount());
  row.Push(Src_ == FBSortSource::Cooperative ? 1 : (Src_ == FBSortSource::Contract ? 2 : 0));
  row.Push(AssignTrack_);
  row.Push(Switches_);
  row.Push(Duplicate_);
  row.Push(FreeContacts_);
  row.Push(SelfBound_);
  row.Push(MateBound_);
  row.Push(BothBoundS_);
  row.Push(CoverS_);
  row.Push(ExposedS_);
  row.Push(DeferS_);
  row.Push(StationErrM_);
}

} // namespace FlightBox::Pilot
