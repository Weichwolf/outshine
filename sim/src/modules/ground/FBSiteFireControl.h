/* FlightBox — FBSiteFireControl: the ENGAGEMENT MACHINE of an air-defence position, in exactly the slot
 * pilot/FBPilot defines and for the same reason modules/missile's guidance sits there — a machine that
 * decides what to do next may only see what its own sensors published (FBState) and may only act
 * through the command bus. It never touches the unit registry and it never spawns anything.
 *
 * SEVEN STATES, and the three RADIATING ones map one-to-one onto the three RWR symbols a pilot reads:
 *
 *   Cold    boot with `set alert cold`, or the Radar failed          radiates nothing
 *   Dark    `set emcon hold` and no cue, or scooting                 radiates nothing
 *   Search  powered and cued                                         beam 0, Mode::Search  -> plain symbol
 *   Track   a firm track designated into the fire-control volume     + beam 1, Mode::Track -> boxed
 *   Engage  a round is up                                            beam 1 -> Mode::Guidance -> flashing + LAUNCH
 *   Scoot   `set scoot_s > 0` and a launch happened                  radiates nothing
 *   Reload  the rails are empty and the magazine is not              radiates beam 0 only
 *
 * The time between the first two is ReactionS, and that is the number a pilot has to live on.
 *
 * WHAT IT DOES NOT DO: it never reads a team, never learns an identity, never asks the world where
 * anything is. Its whole input is its own radar block, its own eye's block, its own passive receiver's
 * block and the clock. doc/modules/ground/module.md §Spec 5. */
#ifndef FBSITEFIRECONTROL_H
#define FBSITEFIRECONTROL_H

#include "FBNetReport.h"
#include "FBPilot.h"
#include "FBSite.h"

namespace FlightBox::Modules {

class FBSiteFireControl;

/* The `net` telemetry source. A separate class for the same reason the judge's zone source is one: the
 * engagement machine already IS the `site` source, and a source carries one name. */
class FBSiteNetTelemetry : public FBTelemetrySource {
public:
  explicit FBSiteNetTelemetry(const FBSiteFireControl &fc) : Fc_(fc) {}
  const char *TelemetryName() const override { return "net"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

private:
  const FBSiteFireControl &Fc_;
};

class FBSiteFireControl : public Pilot::FBPilot {
public:
  /* Telemetry ordinals — append, never reorder. */
  enum class State { Cold = 0, Dark, Search, Track, Engage, Scoot, Reload };
  static const char *StateName(State s);

  /* THE THREE NET STATES, and the third is the conservation rule: a position the mission put on no net
   * behaves exactly as it did before nets existed. doc/air-defence-network.md §5. */
  enum class NetState { Unnetted = 0, Netted, Silent };
  static const char *NetStateName(NetState s);

  /* [SET] How far a reported point must move before it is a NEW cue worth re-entering rather than the
   * same one refreshed. Below it the crew would be typing numbers it already typed; above it the entry
   * never completes against a moving target. Half a kilometre is well inside a fire-control set's own
   * beam at engagement range, so a superseded cue is one that would actually point elsewhere. */
  static constexpr double kCueSupersedeM = 500.0;
  /* The correlation gate of doc/formation.md §5.2, verbatim and for the second consumer: a reported
   * point and an own echo are the same aircraft if they are within this, and unambiguous if the
   * second-best echo is not within twice the distance of the best. */
  static constexpr double kCorrelateM = 1000.0;
  static constexpr double kCorrelateSpeedMs = 300.0;
  /* [SET] How often a CONTINUOUS cue is worth a line. A cue that is refreshed every reporting cycle is
   * one event, not fifty; the time series `net_cue*` carries what happened between two of these. */
  static constexpr double kCueLogS = 30.0;

  /* [SET] How long a firm track may go unrefreshed before the engagement is declared broken. Two
   * fire-control frames: shorter and a single missed look would count as a break, longer and a beam
   * manoeuvre would not read as one. */
  static constexpr double kBreakHoldS = 2.0;
  /* [SET] The cue's own lifetime: an ESM bearing is a reason to switch on, and the position keeps
   * searching for a while after the emitter went quiet rather than immediately going dark again. */
  static constexpr double kCueHoldS = 120.0;
  /* [DERIVED] The mean speed of a surface round over its flight, for the time-of-flight gate: the
   * rocket equation gives the terminal speed back out of the two masses (ve*ln(m0/m1)) and a round
   * accelerating from rest under roughly constant thrust averages half of it. */
  static constexpr double kExhaustMs = 2305.0;

  /* Wiring, once, by the module: the catalogue row, and a BORROWED read-only view of the fire-control
   * set's own bus — the position's second antenna publishes its own radar block, because one block has
   * one writer. */
  void Bind(const FBSiteSpec &spec, const FBState &trackBus);

  /* WHAT THIS POSITION'S OWN REGISTER SAYS ABOUT ITSELF — the module reads its health (it may; every
   * mutator is private to the damage model) and hands the ONE number `react` needs across. A machine
   * that reached for a health register itself would be a machine reading state instead of instruments. */
  void NoteOwnHits(int hits) { Hits_ = hits; }

  /* The six mission keys of doc/modules/ground/module.md §Spec 9, each already validated by the module. */
  /* Both applied in the spawn IC window, before the first Run(), so they set the state the position
   * BOOTS in rather than a flag something later has to interpret. */
  void SetEmconHold(bool on) {
    EmconHold_ = on;
    if (State_ != State::Cold) State_ = on ? State::Dark : State::Search;
  }
  /* THE THIRD VALUE of `set emcon`, and it adds no seventh author-facing key: `react` behaves exactly
   * as `free` and goes dark for ScootS the moment this position's OWN health register reports a change.
   * A crew reacting to its own damage reads no sensor and leaks nothing — and it is the only cue for
   * "we are under attack" this tree can honestly give it, because the round that is attacking radiates
   * nothing and there is no missile-warning channel. THE LIMIT IS STATED WHERE IT BITES: the crew
   * reacts AFTER the round arrives, so FlightBox's anti-radiation weapon is more lethal than history
   * against an attentive crew. doc/air-to-ground.md §5.1. */
  void SetEmconReact(bool on) { EmconReact_ = on; }
  /* THE CREW'S BRIEFED EMISSION PLAN, and it is a VALUE on the same key rather than a seventh one: the
   * position goes off the air at `offS` and (optionally) back on at `onS`, mission-elapsed seconds. It
   * is doctrine written down before the run — the same class of statement `emcon hold` already is, and
   * the same class an aircraft's `brief_*` lines are — and it reads nothing. It exists because the
   * escape window doc/air-to-ground.md §2.2 is built on is CONTINUOUS in the shutdown time, and there
   * is no other way to put a measurement on it: `scoot_s` needs a launch and `react` needs a hit, so
   * neither can be placed. 0 = no plan, which is every position written before this. */
  void SetEmconPlan(double offS, double onS) { EmconOffS_ = offS; EmconOnS_ = onS; }
  void SetColdStart(bool on) { if (on) State_ = State::Cold; }
  void SetRounds(int n) { Rounds_ = n < 0 ? 0 : n; RailsLeft_ = RailsAtMost(); }
  void SetEngageMaxM(double m) { EngageMaxM_ = m; }
  void SetReactionS(double s) { ReactionS_ = s; }
  void SetScootS(double s) { ScootS_ = s; }

  /* ---- The RUNNER-GENERATED net keys (doc/air-defence-network.md §8). Never author-written: they are
   * produced from the mission's `net` block, because membership is a property of the NET and the C1
   * contract's six author-facing keys stay six. ---- */
  void SetControlNode(const std::string &callsign);
  void SetNodeWcs(FBWeaponsControl w) { IsNode_ = true; NodeWcs_ = w; }
  void SetSector(double centreDeg, double halfDeg) {
    HaveSector_ = true; SectorCentreDeg_ = centreDeg; SectorHalfDeg_ = halfDeg;
  }
  void SetAutonomy(FBWeaponsControl w) { Autonomy_ = w; }

  /* WHAT THIS POSITION PUTS ON THE AIR — read by the module at the pose barrier. A point its own
   * acquisition set measured, with that set's own look age. There is no field for an identity. */
  const FBNetReport &NetReport() const { return MyReport_; }

  /* The committed end of the two bus entries the cue costs: the module answers RadarSlewAz/El and hands
   * the value back here, so the antenna acts on what was ENTERED and not on what was heard. */
  void NoteNetAimApplied(bool azimuth, double deg);

  FBTelemetrySource &NetTelemetrySource() { return NetSrc_; }

  /* The one override point, as every pilot's. `plan`/`runway` unused — a position goes nowhere. */
  Pilot::FBPilotCommands Run(const FBState &state, FBCommandBus &avionics,
                             const Systems::FBAirframeControls &airframe, const Fdm::fb_fdm_state &st,
                             const FBFlightPlan &plan, const FBRunway *runway, double dt) override;

  /* ---- what the MODULE reads back and applies to the hardware ---- */
  State GetState() const { return State_; }
  bool SearchRadiating() const;
  bool TrackRadiating() const;
  double AimAzDeg() const { return AimAzDeg_; }     /* body-referenced: the mount's own frame */
  double AimElDeg() const { return AimElDeg_; }
  bool HaveAim() const { return HaveAim_; }
  const FBWeaponTargetState &Target() const { return Target_; }
  int RoundsLeft() const { return Rounds_; }
  int RailsLeft() const { return RailsLeft_; }

  /* A position's trace is not a pilot's, and the bus is per-unit, so this REPLACES the pilot's channels
   * rather than appending: no aircraft's telemetry.csv changes by one column. */
  const char *TelemetryName() const override { return "site"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

private:
  friend class FBSiteNetTelemetry;
  void Enter(State s, const char *why);
  int  RailsAtMost() const;
  /* Receive: the control node's message -> a bearing, an elevation and two staleness terms. Never a
   * track: nothing here writes a contact, and the member's own radar still has to find what it heard. */
  void ReceiveNet(const FBState &state, const Fdm::fb_fdm_state &st);
  /* Enter: the cue reaches the antenna through the command bus, one entry at a time, each charged its
   * own latency and each refusable — the identical construction the GCI chain uses. */
  void EnterCue(FBCommandBus &avionics);
  /* Publish: what this position's OWN acquisition set measured, as a point. */
  void PublishNet(const FBState &state, const Fdm::fb_fdm_state &st);
  /* The formation's correlation gate, second consumer: is the cued point the echo this set is holding? */
  void CorrelateNet(const FBState &trackBus, const Fdm::fb_fdm_state &st);
  FBWeaponsControl EffectiveWcs() const;
  /* The envelope test, all four numbers plus the round's own reach in TIME. Returns false with a reason
   * so the trace says WHY a position that saw a target did not shoot at it. */
  bool InEnvelope(double rangeM, double tgtAltM, double siteAltM, const char **why) const;

  const FBSiteSpec *Spec_ = nullptr;
  const FBState *TrackBus_ = nullptr;   /* borrowed: the fire-control set's own block */

  State State_ = State::Search;
  bool   EmconHold_ = false;
  bool   EmconReact_ = false;
  double EmconOffS_ = 0.0, EmconOnS_ = 0.0;
  int    SeenHits_ = -1;               /* the health register's own counter, to notice a NEW hit */
  double EngageMaxM_ = 0.0;             /* 0 = the row's own Rmax; a mission may only clamp DOWN */
  double ReactionS_ = -1.0;             /* < 0 = the row's own */
  double ScootS_ = 0.0;

  double NowS_ = 0.0;
  double StateSinceS_ = 0.0;
  double CueS_ = -1e9;                  /* last time the passive receiver heard an airborne emitter */
  double TrackSinceS_ = -1.0;           /* when the firm track the engagement stands on was acquired */
  double LastTrackS_ = -1e9;
  double LastShotS_ = -1e9;
  double ReloadUntilS_ = 0.0;
  int    Rounds_ = -1;                  /* < 0 = the row's own default, resolved at Bind */
  int    RailsLeft_ = 0;
  int    SalvoLeft_ = 0;
  int    SeenReleases_ = -1;            /* the SMS's own counter, to notice a round that actually left */
  bool   Armed_ = false;
  /* THE HANDOVER: the acquisition set has something and the fire control has been trained on it. It is
   * what powers the second antenna BEFORE there is a lock — a tracking radar that only switched on once
   * it already had a track could never acquire one, and the moment it starts illuminating is exactly the
   * moment a pilot's receiver should notice. */
  bool   Handover_ = false;
  bool   HaveAim_ = false;
  double AimAzDeg_ = 0.0, AimElDeg_ = 0.0;
  double TrackRangeM_ = 0.0, TrackBearingDeg_ = 0.0, TrackClosureMs_ = 0.0;
  FBWeaponTargetState Target_{};

  /* ---- THE NET. Everything below defaults to "this position is on no net", which is the whole
   * conservation argument: an Unnetted position takes exactly the decisions it took before. ---- */
  char   ControlNode_[kDatalinkCallsignLen] = {};
  bool   IsNode_ = false;
  FBWeaponsControl NodeWcs_ = FBWeaponsControl::Free;   /* what a NODE transmits */
  FBWeaponsControl Autonomy_ = FBWeaponsControl::Hold;  /* the declared fallback when the node is gone */
  FBWeaponsControl NetWcs_ = FBWeaponsControl::Free;    /* what the node last transmitted to US */
  bool   HaveSector_ = false;
  double SectorCentreDeg_ = 0.0, SectorHalfDeg_ = 180.0;
  NetState NetState_ = NetState::Unnetted;
  bool   NetHeld_ = false;              /* a report from the node was readable this tick */
  bool   NetCue_ = false;               /* ...and it fell inside this member's declared sector */
  bool   InSector_ = false, Correlated_ = false;
  bool   LoggedInhibit_ = false;
  double NetAgeS_ = -1.0;               /* the MESSAGE's age: how long ago we heard it */
  double CueLatDeg_ = 0.0, CueLonDeg_ = 0.0, CueAltM_ = 0.0;
  double CueBrgDeg_ = 0.0, CueRngM_ = 0.0, CueAgeS_ = -1.0;   /* CueAgeS_ = the SENDER's own look age */
  double CueAzDeg_ = 0.0, CueElDeg_ = 0.0;
  /* The two-entry state machine: a cue is TYPED, and a cue superseded while still being typed is
   * abandoned rather than queued. */
  enum class CueEntry { Idle = 0, WaitAz, PostEl, WaitEl };
  CueEntry Entry_ = CueEntry::Idle;
  double EnteredLatDeg_ = 0.0, EnteredLonDeg_ = 0.0;   /* the point the pending entry belongs to */
  double CueLoggedS_ = -1e9, OutLoggedS_ = -1.0;
  bool   WasInSector_ = true;
  bool   NetAimValid_ = false;
  double NetAimAzDeg_ = 0.0, NetAimElDeg_ = 0.0;       /* what was COMMITTED, not what was heard */
  FBNetReport MyReport_{};
  FBSiteNetTelemetry NetSrc_{*this};

  /* Telemetry copies. */
  int   TrackCount_ = 0, Beam0_ = 0, Beam1_ = 0, Launches_ = 0;
  bool  Lock_ = false, Cue_ = false;
  double EngagedS_ = 0.0;
  int    Hits_ = 0;
};

} // namespace FlightBox::Modules
#endif
