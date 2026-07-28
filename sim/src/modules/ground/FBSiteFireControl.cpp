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
  bool heard = esm.H.Readable() && esm.ThreatCount > 0;
  if (heard) {
    if (NowS_ - CueS_ > kCueHoldS)
      FBLog::Info("site", "CUE", {{"brgDeg", esm.Threats[0].BearingDeg},
          {"signal", esm.Threats[0].SignalNorm}, {"threats", esm.ThreatCount}});
    CueS_ = NowS_;
  }
  Cue_ = heard;
  const bool cued = !EmconHold_ || NowS_ - CueS_ <= kCueHoldS;

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
        SalvoLeft_ = Spec_->RoundsPerEngagement;
        Enter(State::Engage, "reaction time elapsed");
      }
      break;
    }
    case State::Engage: {
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
