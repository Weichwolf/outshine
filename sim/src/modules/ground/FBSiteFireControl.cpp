#include "FBSiteFireControl.h"
#include "FBGeodesy.h"
#include "FBLog.h"
#include "FBUnits.h"
#include <cmath>

namespace FlightBox::Modules {

const char *FBSiteFireControl::StateName(State s) {
  switch (s) {
    case State::Cold: return "COLD";
    case State::Dark: return "DARK";
    case State::Search: return "SEARCH";
    case State::Track: return "TRACK";
    case State::Engage: return "ENGAGE";
    case State::Scoot: return "SCOOT";
    case State::Reload: return "RELOAD";
  }
  return "?";
}

const char *FBSiteFireControl::NetStateName(NetState s) {
  switch (s) {
    case NetState::Unnetted: return "UNNETTED";
    case NetState::Netted: return "NETTED";
    case NetState::Silent: return "SILENT";
  }
  return "?";
}

void FBSiteFireControl::SetControlNode(const std::string &callsign) {
  std::size_t n = 0;
  while (n + 1 < (std::size_t)kDatalinkCallsignLen && n < callsign.size()) {
    ControlNode_[n] = callsign[n];
    n++;
  }
  ControlNode_[n] = '\0';
  NetState_ = NetState::Silent;   /* subscribed and not yet hearing anybody is exactly Silent */
}

/* WHO MAY SHOOT. Unnetted is today's behaviour verbatim; a node transmits its own declared state; a
 * member takes the node's while it hears it and its own declared fallback the moment it does not. */
FBWeaponsControl FBSiteFireControl::EffectiveWcs() const {
  if (IsNode_) return NodeWcs_;
  if (ControlNode_[0] == '\0') return FBWeaponsControl::Free;
  return NetState_ == NetState::Netted ? NetWcs_ : Autonomy_;
}

void FBSiteFireControl::NoteNetAimApplied(bool azimuth, double deg) {
  if (azimuth) {
    NetAimAzDeg_ = deg;
    if (Entry_ == CueEntry::WaitAz) Entry_ = CueEntry::PostEl;
  } else {
    NetAimElDeg_ = deg;
    NetAimValid_ = true;
    if (Entry_ == CueEntry::WaitEl) Entry_ = CueEntry::Idle;
  }
}

void FBSiteFireControl::Bind(const FBSiteSpec &spec, const FBState &trackBus) {
  Spec_ = &spec;
  TrackBus_ = &trackBus;
  if (Rounds_ < 0) Rounds_ = spec.RoundsDefault;
  RailsLeft_ = RailsAtMost();
}

int FBSiteFireControl::RailsAtMost() const {
  if (!Spec_) return 0;
  int rails = Spec_->RailCount > 0 ? Spec_->RailCount : Spec_->RoundsDefault;
  return rails < Rounds_ ? rails : Rounds_;
}

void FBSiteFireControl::Enter(State s, const char *why) {
  if (s == State_) return;
  bool wasLoud = SearchRadiating() || TrackRadiating();
  State old = State_;
  State_ = s;
  StateSinceS_ = NowS_;
  bool nowLoud = SearchRadiating() || TrackRadiating();
  FBLog::Info("site", "STATE", {{"from", StateName(old)}, {"to", StateName(s)}, {"why", why},
      {"roundsLeft", Rounds_}, {"railsLeft", RailsLeft_}});
  if (nowLoud && !wasLoud) FBLog::Info("site", "RADIATE", {{"why", why}});
  if (!nowLoud && wasLoud) FBLog::Info("site", "GO_DARK", {{"why", why}});
}

/* A position that is COLD, DARK or SCOOTing radiates nothing at all; every other state sweeps. RELOAD
 * keeps the acquisition set up — reloading the rails does not switch off the antenna. */
bool FBSiteFireControl::SearchRadiating() const {
  if (!Spec_ || Spec_->SearchRangeM <= 0.0) return false;
  return State_ == State::Search || State_ == State::Track || State_ == State::Engage ||
         State_ == State::Reload;
}

/* The fire control radiates while it is working a track AND while it is trying to get one — see
 * Handover_. That is the whole point of two beams: the acquisition set never stops sweeping. */
bool FBSiteFireControl::TrackRadiating() const {
  if (!Spec_ || Spec_->TrackRangeM <= 0.0) return false;
  if (State_ == State::Track || State_ == State::Engage) return true;
  return State_ == State::Search && Handover_;
}

/* Four numbers and a clock. The LOW edge is as decisive as the high one: going under the S-75's 450 m
 * floor is exactly what puts an attacker into the flak, and that geometry is this test. */
bool FBSiteFireControl::InEnvelope(double rangeM, double tgtAltM, double siteAltM,
                                   const char **why) const {
  /* A POSITION WITH NO TRACKING RADAR CANNOT APPLY THIS TEST, and pretending otherwise would be the
   * cheat: an eye publishes no range and no altitude, ever. What binds an optical position is therefore
   * its GUNNER's sight (measured: 3 784 m beam-on, 2 493 m head-on, zero at night) and afterwards the
   * ROUND's own seeker or the barrel's own reach — never a number this class invented. */
  if (Spec_->TrackRangeM <= 0.0) return true;
  double rMax = Spec_->EnvMaxM;
  if (EngageMaxM_ > 0.0 && EngageMaxM_ < rMax) rMax = EngageMaxM_;   /* a mission may only clamp DOWN */
  if (rangeM < Spec_->EnvMinM) { *why = "inside Rmin"; return false; }
  if (rangeM > rMax) { *why = "beyond Rmax"; return false; }
  double aglM = tgtAltM - siteAltM;
  if (aglM < Spec_->EnvAltMinM) { *why = "below the altitude floor"; return false; }
  if (aglM > Spec_->EnvAltMaxM) { *why = "above the altitude ceiling"; return false; }
  /* THE ROUND'S OWN REACH IN TIME, not a second range: it is retired at its catalogue lifetime, so a
   * shot whose predicted flight time exceeds that one is a round that expires on the way. */
  const FBStoreSpec *store = FBStoreSpecOf(Spec_->Store);
  if (store && store->Perf.BurnoutMassKg > 0.0 && store->MaxFlightS > 0.0) {
    double vTerm = kExhaustMs * std::log(store->Perf.LaunchMassKg / store->Perf.BurnoutMassKg);
    double meanMs = 0.5 * vTerm;
    if (meanMs > 1.0 && rangeM / meanMs > store->MaxFlightS) { *why = "outside the round's endurance"; return false; }
  }
  return true;
}

/* ---- THE NET, in four short steps, and not one of them writes a contact ---- */

/* RECEIVE. The control node's message is found in this terminal's OWN datalink block by the sender's
 * callsign — a cooperative net gives the SENDER's identity away and says nothing about the target. What
 * comes out is a direction, a range and two staleness terms. */
void FBSiteFireControl::ReceiveNet(const FBState &state, const Fdm::fb_fdm_state &st) {
  NetHeld_ = false;
  NetCue_ = false;
  InSector_ = false;
  NetAgeS_ = -1.0;
  CueAgeS_ = -1.0;
  if (IsNode_) { NetState_ = NetState::Netted; return; }   /* a node is its own picture */
  if (ControlNode_[0] == '\0') { NetState_ = NetState::Unnetted; return; }

  const FBDatalinkBlock &dl = state.Datalink;
  const FBDatalinkTrack *node = nullptr;
  if (dl.H.Readable())
    for (int i = 0; i < dl.TrackCount; i++)
      if (std::strcmp(dl.Tracks[i].Callsign, ControlNode_) == 0) { node = &dl.Tracks[i]; break; }

  NetState old = NetState_;
  NetState_ = node ? NetState::Netted : NetState::Silent;
  if (NetState_ != old) {
    FBLog::Info("net", "STATE", {{"from", NetStateName(old)}, {"to", NetStateName(NetState_)},
        {"node", std::string(ControlNode_)}});
    if (NetState_ == NetState::Silent) {
      CueLoggedS_ = -1e9;   /* the next cue is a NEW one and says so, however long the silence lasted */
      FBLog::Warn("net", "AUTONOMOUS", {{"node", std::string(ControlNode_)},
          {"fallback", FBWeaponsControlStr(Autonomy_)}});
    }
  }
  if (!node) return;

  NetHeld_ = true;
  NetAgeS_ = node->AgeS;
  NetWcs_ = node->Net.Wcs;
  if (!node->Net.Reporting) return;   /* the node is up and has nothing: a picture, and it is empty */

  CueLatDeg_ = node->Net.LatDeg;
  CueLonDeg_ = node->Net.LonDeg;
  CueAltM_ = node->Net.AltM;
  CueAgeS_ = node->Net.TgtLookAgeS;
  CueRngM_ = FBPlanarDistM(st.lat, st.lon, CueLatDeg_, CueLonDeg_);
  CueBrgDeg_ = FBBearingDeg(st.lat, st.lon, CueLatDeg_, CueLonDeg_);
  /* THE ELEVATION IS THE POINT of the whole mechanism, and the equation is the MiG-29 manual's own for
   * the same act: putting the window on the wrong altitude band is how a set flies past a target it
   * could easily have seen. Both halves go through core/FBBodyAngle, which for a MOUNT is arithmetically
   * a no-op in elevation (a ground mount publishes roll = pitch = 0) — and that is exactly why it is
   * written: the rule then holds because the transform says so, not because the number happens to be
   * zero on this one platform. */
  CueElDeg_ = FBBodyAngle::FromWorldElevation(
      std::atan2(CueAltM_ - st.elev, CueRngM_ > 1.0 ? CueRngM_ : 1.0) * kRad2Deg, st.pitch).Deg();
  CueAzDeg_ = FBBodyAngle::FromTrueBearing(CueBrgDeg_, st.yaw).Deg();

  InSector_ = !HaveSector_ || std::fabs(FBWrap180(CueBrgDeg_ - SectorCentreDeg_)) <= SectorHalfDeg_;
  if (!InSector_) {
    Entry_ = CueEntry::Idle;   /* abandoned, not queued: a cue for the neighbour is not this one's */
    if (WasInSector_ || OutLoggedS_ < 0.0) {
      OutLoggedS_ = NowS_;
      WasInSector_ = false;
      FBLog::Info("net", "CUE_OUT_OF_SECTOR", {{"brgDeg", CueBrgDeg_},
          {"centreDeg", SectorCentreDeg_}, {"halfDeg", SectorHalfDeg_}});
    }
    return;
  }
  WasInSector_ = true;
  NetCue_ = true;
}

/* ENTER. Two entries over the command bus, one after the other, each charged its own latency — the cue
 * therefore costs TIME, and a cue superseded while still being typed is abandoned rather than queued. */
void FBSiteFireControl::EnterCue(FBCommandBus &avionics) {
  if (!NetCue_) return;
  double movedM = FBPlanarDistM(CueLatDeg_, CueLonDeg_, EnteredLatDeg_, EnteredLonDeg_);
  bool moved = !NetAimValid_ && Entry_ == CueEntry::Idle ? true : movedM > kCueSupersedeM;
  if (moved && Entry_ != CueEntry::Idle) {
    FBLog::Info("net", "CUE_SUPERSEDED", {{"movedM", movedM}, {"brgDeg", CueBrgDeg_}});
    Entry_ = CueEntry::Idle;
  }
  if (Entry_ == CueEntry::PostEl) {
    /* The SECOND entry, and only once the first has been committed: the cost of a cue is two of these,
     * one after the other, and that is what makes a fresher net worth something. */
    FBCommandAck ack = avionics.PostAntennaEl(FBBodyAngle::Measured(CueElDeg_), NowS_);
    if (ack.Outcome != FBCommandOutcome::Rejected) Entry_ = CueEntry::WaitEl;
    return;
  }
  if (Entry_ != CueEntry::Idle || !moved) return;
  FBCommandAck ack = avionics.PostAntennaAz(FBBodyAngle::Measured(CueAzDeg_), NowS_);
  if (ack.Outcome == FBCommandOutcome::Rejected) return;
  EnteredLatDeg_ = CueLatDeg_;
  EnteredLonDeg_ = CueLonDeg_;
  Entry_ = CueEntry::WaitAz;
  /* A CONTINUOUS cue is one event, not one per reporting cycle: the line is written when the position
   * starts being cued and then at the sampling rate a reader can use. What happened in between is in
   * `net_cue*`, which is a time series and is the right shape for it. */
  if (NowS_ - CueLoggedS_ >= kCueLogS) {
    CueLoggedS_ = NowS_;
    FBLog::Info("net", "CUE", {{"brgDeg", CueBrgDeg_}, {"rngM", CueRngM_}, {"altM", CueAltM_},
        {"elDeg", CueElDeg_}, {"lookAgeS", CueAgeS_}, {"msgAgeS", NetAgeS_},
        {"wcs", FBWeaponsControlStr(EffectiveWcs())}});
  }
}

/* PUBLISH. What this position's own acquisition set measured, as a POINT — no id, no team, no type,
 * because the FBRadarContact it is built from has none of the three. */
void FBSiteFireControl::PublishNet(const FBState &state, const Fdm::fb_fdm_state &st) {
  MyReport_ = FBNetReport{};
  MyReport_.Wcs = IsNode_ ? NodeWcs_ : EffectiveWcs();
  const FBRadarBlock &acq = state.Radar;
  const FBRadarContact *best = nullptr;
  if (acq.H.Readable())
    for (int i = 0; i < acq.ContactCount; i++)
      if (!best || acq.Contacts[i].RangeM < best->RangeM) best = &acq.Contacts[i];
  if (!best && TrackBus_ && TrackBus_->Radar.H.Readable()) {
    const FBRadarBlock &fcr = TrackBus_->Radar;
    if (fcr.LockIndex >= 0 && fcr.LockIndex < fcr.ContactCount) best = &fcr.Contacts[fcr.LockIndex];
  }
  if (!best) return;
  double brg = best->BearingDeg * kDeg2Rad, elv = best->ElevAngleDeg * kDeg2Rad;
  double horiz = best->RangeM * std::cos(elv);
  double coslat = std::cos(st.lat * kDeg2Rad);
  MyReport_.Reporting = true;
  MyReport_.LatDeg = st.lat + horiz * std::cos(brg) / kMPerDeg;
  MyReport_.LonDeg = st.lon + (std::fabs(coslat) > 1e-6 ? horiz * std::sin(brg) / (kMPerDeg * coslat) : 0.0);
  MyReport_.AltM = (float)(st.elev + best->RangeM * std::sin(elv));
  MyReport_.TgtLookAgeS = (float)best->LookAgeS;
}

/* CORRELATE. doc/formation.md §5.2's gate verbatim, second consumer: is the point somebody else
 * reported the same aircraft this set is holding? Ambiguity is a coin toss and claims nothing. */
void FBSiteFireControl::CorrelateNet(const FBState &trackBus, const Fdm::fb_fdm_state &st) {
  Correlated_ = false;
  if (!NetCue_) return;
  const FBRadarBlock &fcr = trackBus.Radar;
  if (!fcr.H.Readable() || fcr.ContactCount <= 0) return;
  double ageS = std::max(0.0, CueAgeS_) + std::max(0.0, NetAgeS_);
  double gateM = kCorrelateM + ageS * kCorrelateSpeedMs;
  double bestM = 1e18, secondM = 1e18;
  double coslat = std::cos(st.lat * kDeg2Rad);
  for (int i = 0; i < fcr.ContactCount; i++) {
    const FBRadarContact &c = fcr.Contacts[i];
    double brg = c.BearingDeg * kDeg2Rad, elv = c.ElevAngleDeg * kDeg2Rad;
    double horiz = c.RangeM * std::cos(elv);
    double lat = st.lat + horiz * std::cos(brg) / kMPerDeg;
    double lon = st.lon + (std::fabs(coslat) > 1e-6 ? horiz * std::sin(brg) / (kMPerDeg * coslat) : 0.0);
    double alt = st.elev + c.RangeM * std::sin(elv);
    double planarM = FBPlanarDistM(lat, lon, CueLatDeg_, CueLonDeg_);
    double dAlt = alt - CueAltM_;
    double d = std::sqrt(planarM * planarM + dAlt * dAlt);
    if (d < bestM) { secondM = bestM; bestM = d; }
    else if (d < secondM) { secondM = d; }
  }
  if (bestM > gateM) return;
  if (secondM < 2.0 * bestM) {
    FBLog::Info("net", "CORRELATE_AMBIGUOUS", {{"bestM", bestM}, {"secondM", secondM}, {"gateM", gateM}});
    return;
  }
  Correlated_ = true;
}

Pilot::FBPilotCommands FBSiteFireControl::Run(const FBState &state, FBCommandBus &avionics,
                                              const Systems::FBAirframeControls &airframe,
                                              const Fdm::fb_fdm_state &st, const FBFlightPlan &plan,
                                              const FBRunway *runway, double dt) {
  (void)airframe; (void)plan; (void)runway;
  Pilot::FBPilotCommands c;   /* a position flies nothing: Guidance stays None */
  NowS_ += dt;
  if (!Spec_ || !TrackBus_) return c;

  const double reactionS = ReactionS_ >= 0.0 ? ReactionS_ : Spec_->ReactionS;

  /* ---- THE CUE. A passive bearing and a power, never a range: the position still has to search for
   * what it heard, which is the difference between a receiver and a timer. ---- */
  const FBRwrBlock &esm = state.Rwr;
  /* AN AIRBORNE EMITTER, and the word is the contract's: a battery does not scramble because its own
   * early-warning set next door is sweeping, and after this round there IS such a set next door. The
   * discriminator is the receiver's own estimated KIND — the two airborne values against the two
   * surface ones — and nothing that names a unit. doc/modules/ground/module.md §Spec 5. */
  const FBRwrThreat *airborne = nullptr;
  if (esm.H.Readable())
    for (int i = 0; i < esm.ThreatCount; i++)
      if (esm.Threats[i].Kind == FBEmitterKind::AirborneFireControl ||
          esm.Threats[i].Kind == FBEmitterKind::MissileSeeker) { airborne = &esm.Threats[i]; break; }
  bool heard = airborne != nullptr;
  if (heard) {
    if (NowS_ - CueS_ > kCueHoldS)
      FBLog::Info("site", "CUE", {{"brgDeg", airborne->BearingDeg},
          {"signal", airborne->SignalNorm}, {"threats", esm.ThreatCount}});
    CueS_ = NowS_;
  }
  Cue_ = heard;

  /* ---- THE NET. A DIRECTION TO LOOK and never a track: nothing below writes a contact, and the
   * position's own radar still has to detect, firm and pass its own envelope test. ---- */
  ReceiveNet(state, st);
  EnterCue(avionics);
  /* THE SECOND LEGAL WAKE-UP, and it is legal for the same reason the passive receiver is: a received
   * signal with a range, an age and a sender that can be killed — not a timer and not a registry read.
   * doc/air-defence-network.md §3, the EMCON interaction. */
  if (NetCue_) CueS_ = NowS_;

  /* THE BRIEFED PLAN, and it outranks every other reason to be on the air: a crew told to go dark at a
   * time goes dark at that time. Written as a separate term rather than folded into `cued` so a
   * position with no plan takes exactly the decisions it took before this existed. */
  const bool briefedQuiet = EmconOffS_ > 0.0 && NowS_ >= EmconOffS_ &&
                            (EmconOnS_ <= 0.0 || NowS_ < EmconOnS_);
  const bool cued = !briefedQuiet && (!EmconHold_ || NowS_ - CueS_ <= kCueHoldS);

  /* ---- `emcon react`: THE ONE CUE FOR "WE ARE UNDER ATTACK" THIS TREE CAN HONESTLY GIVE A CREW.
   * The position's own health register, which a module may READ and never write. Not a sensor, not a
   * warning, not a message — a fact it owns about itself. Everything else was refused with its reason
   * in doc/air-to-ground.md §5.1: the inbound round radiates nothing, there is no missile-warning
   * channel, and there is no acoustic or seismic one to invent. ---- */
  if (EmconReact_) {
    if (SeenHits_ < 0) SeenHits_ = Hits_;
    else if (Hits_ > SeenHits_) {
      SeenHits_ = Hits_;
      /* Enter() writes the GO_DARK line itself, with `why=damage`: a second one here would be the same
       * event twice. */
      if (State_ != State::Cold && State_ != State::Scoot && ScootS_ > 0.0) Enter(State::Scoot, "damage");
    }
  }

  /* ---- THE PICTURE. Two sources, never fused into one file: the acquisition set (or the eye, for a
   * position that has no radar) says WHERE to look; the fire-control set says whether it HAS him. ---- */
  const FBRadarBlock &acq = state.Radar;
  const FBVisualBlock &eye = state.Visual;
  const FBRadarBlock &fcr = TrackBus_->Radar;
  bool haveCue = false;
  double cueAz = 0.0, cueEl = 0.0;
  int contacts = 0;
  if (acq.H.Readable() && acq.ContactCount > 0) {
    contacts = acq.ContactCount;
    const FBRadarContact *best = nullptr;
    for (int i = 0; i < acq.ContactCount; i++)
      if (!best || acq.Contacts[i].RangeM < best->RangeM) best = &acq.Contacts[i];
    if (best) { haveCue = true; cueAz = best->AzDeg; cueEl = best->ElDeg; }
  } else if (eye.H.Readable() && eye.ContactCount > 0) {
    /* THE EYE HAS NO RANGE, EVER. What it gives a position is a direction — and for the two MANPADS
     * rows and the optical gun that is the only channel there is. The range comes from the weapon's own
     * head afterwards, never from here. */
    contacts = eye.ContactCount;
    const FBVisualContact *best = nullptr;
    for (int i = 0; i < eye.ContactCount; i++)
      if (!best || eye.Contacts[i].SizeMrad > best->SizeMrad) best = &eye.Contacts[i];
    if (best) { haveCue = true; cueAz = best->AzDeg; cueEl = best->ElDeg; }
  }
  /* THE CUE MOVES THE ANTENNA — and only when this position's own sets have nothing of their own. What
   * is used is the COMMITTED entry, not the message: the crew acts on what it typed. */
  if (!haveCue && NetCue_ && NetAimValid_) {
    haveCue = true;
    cueAz = NetAimAzDeg_;
    cueEl = NetAimElDeg_;
  }
  TrackCount_ = contacts;
  Handover_ = haveCue && (State_ == State::Search || State_ == State::Track || State_ == State::Engage);
  if (haveCue) { AimAzDeg_ = cueAz; AimElDeg_ = cueEl; HaveAim_ = true; }

  /* ---- THE FIRE-CONTROL SET's own answer. For a position without one (MANPADS, optical AAA) the eye's
   * own firm contact IS the track: the gunner is the fire control. ---- */
  bool locked = false;
  double rangeM = 0.0, tgtAltM = 0.0;
  if (Spec_->TrackRangeM > 0.0) {
    if (fcr.H.Readable() && fcr.LockIndex >= 0 && fcr.LockIndex < fcr.ContactCount) {
      const FBRadarContact &t = fcr.Contacts[fcr.LockIndex];
      locked = true;
      rangeM = t.RangeM;
      TrackRangeM_ = t.RangeM;
      TrackBearingDeg_ = t.BearingDeg;
      TrackClosureMs_ = t.ClosureMs;
      AimAzDeg_ = t.AzDeg;
      AimElDeg_ = t.ElDeg;
      HaveAim_ = true;
      double brg = t.BearingDeg * kDeg2Rad, elv = t.ElevAngleDeg * kDeg2Rad;
      double horiz = t.RangeM * std::cos(elv);
      double e = horiz * std::sin(brg), n = horiz * std::cos(brg);
      tgtAltM = st.elev + t.RangeM * std::sin(elv);
      double coslat = std::cos(st.lat * kDeg2Rad);
      Target_.Valid = true;
      Target_.LatDeg = st.lat + n / kMPerDeg;
      Target_.LonDeg = st.lon + (std::fabs(coslat) > 1e-6 ? e / (kMPerDeg * coslat) : 0.0);
      Target_.AltM = tgtAltM;
      Target_.StampS = fcr.H.StampS - t.LookAgeS;
      /* The velocity is the pilot's own tracker, fed from the SET's block — a contact is an echo
       * without one, and the round's midcourse needs it. */
      BfmTrack().Update(*TrackBus_, st, NowS_);
      const FBBfmBlock &trk = BfmTrack().Block();
      if (trk.H.Readable()) {
        Target_.VelE = trk.VelE; Target_.VelN = trk.VelN; Target_.VelU = trk.VelU;
      }
    } else {
      Target_.Valid = false;
    }
  } else if (haveCue) {
    /* An optical engagement: a direction, and the round finds its own range. The envelope test then
     * uses the SEEKER's reach as the stand-in for a range the position cannot measure — which is
     * exactly why a MANPADS is bound by its gunner's eyes and not by its missile. */
    locked = true;
    AimAzDeg_ = cueAz;
    AimElDeg_ = cueEl;
    HaveAim_ = true;
    const FBVisualContact *best = nullptr;
    for (int i = 0; i < eye.ContactCount; i++)
      if (!best || eye.Contacts[i].SizeMrad > best->SizeMrad) best = &eye.Contacts[i];
    if (best) {
      TrackBearingDeg_ = best->BearingDeg;
      /* THE ONE RANGE AN EYE CAN STATE: none. What the launch programming needs is not a range but a
       * LINE — an angle-only head is slaved to the line of sight and finds its own target on it — so the
       * phantom is placed at the ROUND's own seeker reach along the measured bearing. Any range on that
       * line gives the same direction from the launcher; this one keeps the estimate inside the reach
       * the head is being asked to search. It is a POINTING aid and never a measurement, and the
       * consequence is named where the eye's limits are (doc/modules/ground/module.md). */
      const FBStoreSpec *rspec = FBStoreSpecOf(Spec_->Store);
      rangeM = rspec && rspec->Perf.SeekerRangeM > 0.0 ? rspec->Perf.SeekerRangeM
                                                       : 0.5 * (Spec_->EnvMinM + Spec_->EnvMaxM);
      TrackRangeM_ = rangeM;
      tgtAltM = st.elev + rangeM * std::sin(best->ElevAngleDeg * kDeg2Rad);
      double brg = best->BearingDeg * kDeg2Rad, elv = best->ElevAngleDeg * kDeg2Rad;
      double horiz = rangeM * std::cos(elv);
      double coslat = std::cos(st.lat * kDeg2Rad);
      Target_.Valid = true;
      Target_.LatDeg = st.lat + horiz * std::cos(brg) / kMPerDeg;
      Target_.LonDeg = st.lon + (std::fabs(coslat) > 1e-6 ? horiz * std::sin(brg) / (kMPerDeg * coslat) : 0.0);
      Target_.AltM = tgtAltM;
      Target_.StampS = NowS_;
    }
  } else {
    Target_.Valid = false;
  }
  Lock_ = locked;
  if (locked) LastTrackS_ = NowS_;
  CorrelateNet(*TrackBus_, st);
  PublishNet(state, st);

  /* ---- MASTER ARM, once: a position that came up armed is one whose crew did it, and it travels the
   * command bus at that action's own latency exactly as a pilot's hand would. ---- */
  if (!Armed_ && Spec_->Channels > 0 && State_ != State::Cold) {
    FBCommandAck ack = avionics.Post(FBCommandTarget::MasterArm, 1.0, NowS_);
    if (ack.Outcome != FBCommandOutcome::Rejected) Armed_ = true;
  }

  /* ---- THE MACHINE ---- */
  const double inState = NowS_ - StateSinceS_;
  switch (State_) {
    case State::Cold:
      if (inState >= Spec_->WarmupS) Enter(cued ? State::Search : State::Dark, "warmup complete");
      break;
    case State::Dark:
      if (cued) Enter(State::Search, EmconHold_ ? "passive cue" : "emcon free");
      break;
    case State::Search:
      if (!cued) { Enter(State::Dark, "cue expired"); break; }
      if (locked) {
        const char *why = nullptr;
        if (Spec_->Channels > 0 && Rounds_ > 0 && InEnvelope(rangeM, tgtAltM, st.elev, &why)) {
          TrackSinceS_ = NowS_;
          Enter(State::Track, "firm track inside the envelope");
          FBLog::Info("site", "TRACK", {{"brgDeg", TrackBearingDeg_}, {"rangeM", TrackRangeM_},
              {"closureMs", TrackClosureMs_}, {"altM", tgtAltM}, {"reactionS", reactionS}});
        }
      }
      break;
    case State::Track: {
      if (briefedQuiet) { Enter(State::Dark, "emission plan"); break; }
      if (!locked && NowS_ - LastTrackS_ > kBreakHoldS) {
        FBLog::Info("site", "BREAK", {{"reason", "track lost"}, {"intoS", NowS_ - TrackSinceS_}});
        Enter(State::Search, "track lost");
        break;
      }
      const char *why = nullptr;
      if (locked && !InEnvelope(rangeM, tgtAltM, st.elev, &why)) {
        FBLog::Info("site", "BREAK", {{"reason", why}, {"intoS", NowS_ - TrackSinceS_},
            {"rangeM", rangeM}, {"altM", tgtAltM}});
        Enter(State::Search, why);
        break;
      }
      if (NowS_ - TrackSinceS_ >= reactionS && Rounds_ > 0 && RailsLeft_ > 0) {
        /* THE FIRE-CONTROL AUTHORITY sits IN FRONT OF the existing launch decision and adds no launch
         * path. `free` is what every position did before nets existed; `tight` needs the track to
         * CORRELATE with a live cue; `hold` never launches and still radiates, which is what makes a
         * position a decoy that costs the attacker a weapon. */
        FBWeaponsControl wcs = EffectiveWcs();
        bool release = wcs == FBWeaponsControl::Free ||
                       (wcs == FBWeaponsControl::Tight && Correlated_);
        if (release) {
          LoggedInhibit_ = false;
          SalvoLeft_ = Spec_->RoundsPerEngagement;
          Enter(State::Engage, "reaction time elapsed");
        } else if (!LoggedInhibit_) {
          LoggedInhibit_ = true;
          FBLog::Info("net", "WCS", {{"state", FBWeaponsControlStr(wcs)},
              {"netState", NetStateName(NetState_)}, {"correlated", Correlated_},
              {"effect", "launch inhibited"}, {"rangeM", TrackRangeM_}});
        }
      }
      break;
    }
    case State::Engage: {
      if (briefedQuiet) { Enter(State::Dark, "emission plan"); break; }
      if (!locked && NowS_ - LastTrackS_ > kBreakHoldS) {
        FBLog::Info("site", "BREAK", {{"reason", "track lost"}, {"intoS", NowS_ - TrackSinceS_}});
        Enter(ScootS_ > 0.0 ? State::Scoot : State::Search, "track lost in the engagement");
        break;
      }
      if (SalvoLeft_ > 0 && Rounds_ > 0 && RailsLeft_ > 0 && NowS_ - LastShotS_ >= Spec_->SalvoS) {
        if (Spec_->Store != FBStoreKind::None) avionics.Post(FBCommandTarget::WeaponRelease, 1.0, NowS_);
        else if (Spec_->Gun != FBGunKind::None) avionics.Post(FBCommandTarget::GunTrigger, 1.0, NowS_);
        LastShotS_ = NowS_;
      }
      break;
    }
    case State::Scoot:
      if (inState >= ScootS_) Enter(cued ? State::Search : State::Dark, "scoot complete");
      break;
    case State::Reload:
      if (NowS_ >= ReloadUntilS_) {
        RailsLeft_ = RailsAtMost();
        Enter(RailsLeft_ > 0 ? State::Search : State::Dark, "rails refilled");
      }
      break;
  }
  EngagedS_ = State_ == State::Engage ? NowS_ - StateSinceS_ : 0.0;

  /* ---- WHAT ACTUALLY LEFT THE RAIL. Read off the SMS's own counter rather than off the command that
   * was posted: the box may have said no, and a position that counted its intentions would have an
   * infinite magazine. ---- */
  int released = 0;
  if (state.Stores.H.Readable()) released = state.Stores.ReleasedCount;
  else if (state.Gun.H.Readable()) released = state.Gun.RoundsFired;
  if (SeenReleases_ < 0) SeenReleases_ = released;
  if (released > SeenReleases_) {
    int fired = Spec_->Store != FBStoreKind::None ? released - SeenReleases_ : 1;
    SeenReleases_ = released;
    Rounds_ -= fired;
    if (Rounds_ < 0) Rounds_ = 0;
    RailsLeft_ -= fired;
    if (RailsLeft_ < 0) RailsLeft_ = 0;
    SalvoLeft_ -= fired;
    Launches_ += fired;
    FBLog::Info("site", "LAUNCH", {{"rangeM", TrackRangeM_}, {"brgDeg", TrackBearingDeg_},
        {"altM", Target_.AltM}, {"roundsLeft", Rounds_}, {"railsLeft", RailsLeft_},
        {"salvoLeft", SalvoLeft_ > 0 ? SalvoLeft_ : 0},
        {"trackToLaunchS", NowS_ - TrackSinceS_}});
    if (Rounds_ <= 0) {
      FBLog::Info("site", "DEPLETED", {{"launches", Launches_}});
      Enter(State::Dark, "magazine empty");
    } else if (RailsLeft_ <= 0 && Spec_->ReloadS > 0.0) {
      ReloadUntilS_ = NowS_ + Spec_->ReloadS;
      Enter(State::Reload, "rails empty");
    } else if (SalvoLeft_ <= 0) {
      /* A SECOND engagement is a SECOND decision, so the reaction clock restarts: a battery that
       * emptied its magazine into one target inside one reaction time would make the number meaningless. */
      TrackSinceS_ = NowS_;
      if (ScootS_ > 0.0) Enter(State::Scoot, "salvo complete, scooting");
      else Enter(State::Track, "salvo complete");
    }
  }

  Beam0_ = SearchRadiating() ? 1 : 0;
  Beam1_ = TrackRadiating() ? (State_ == State::Engage ? 3 : 2) : 0;
  return c;
}

void FBSiteNetTelemetry::DeclareTelemetry(FBTelemetrySchema &schema) const {
  schema.Add("net_state");            /* FBSiteFireControl::NetState ordinal */
  schema.Add("net_age_s", "s");       /* how long ago the node's message was heard */
  schema.Add("net_cue");
  schema.Add("net_cue_brg", "deg");
  schema.Add("net_cue_rng_m", "m");
  schema.Add("net_cue_age_s", "s");   /* the SENDER's own look age — the second staleness term */
  schema.Add("net_wcs");              /* FBWeaponsControl ordinal, as it EFFECTIVELY applies here */
  schema.Add("net_in_sector");
  schema.Add("net_correlated");
}

void FBSiteNetTelemetry::SampleTelemetry(FBTelemetryRow &row) const {
  row.Push((int)Fc_.NetState_);
  row.Push(Fc_.NetAgeS_);
  row.Push(Fc_.NetCue_);
  row.Push(Fc_.NetCue_ ? Fc_.CueBrgDeg_ : -1.0);
  row.Push(Fc_.NetCue_ ? Fc_.CueRngM_ : -1.0);
  row.Push(Fc_.CueAgeS_);
  row.Push((int)Fc_.EffectiveWcs());
  row.Push(Fc_.InSector_);
  row.Push(Fc_.Correlated_);
}

void FBSiteFireControl::DeclareTelemetry(FBTelemetrySchema &schema) const {
  schema.Add("site_state");        /* FBSiteFireControl::State ordinal */
  schema.Add("site_beam0");        /* 0 = silent, 1 = the acquisition set is sweeping */
  schema.Add("site_beam1");        /* 0 silent / 2 track / 3 guidance */
  schema.Add("site_tracks");
  schema.Add("site_lock");
  schema.Add("site_rounds");
  schema.Add("site_rails");
  schema.Add("site_engaged_s", "s");
  schema.Add("site_cue");
  schema.Add("site_launches");
  schema.Add("site_trk_rng", "m");
  schema.Add("site_trk_brg", "deg");
}

void FBSiteFireControl::SampleTelemetry(FBTelemetryRow &row) const {
  row.Push((int)State_);
  row.Push(Beam0_);
  row.Push(Beam1_);
  row.Push(TrackCount_);
  row.Push(Lock_);
  row.Push(Rounds_);
  row.Push(RailsLeft_);
  row.Push(EngagedS_);
  row.Push(Cue_);
  row.Push(Launches_);
  row.Push(Lock_ ? TrackRangeM_ : -1.0);
  row.Push(Lock_ ? TrackBearingDeg_ : -1.0);
}

} // namespace FlightBox::Modules
