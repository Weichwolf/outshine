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

#include "FBPilot.h"
#include "FBSite.h"

namespace FlightBox::Modules {

class FBSiteFireControl : public Pilot::FBPilot {
public:
  /* Telemetry ordinals — append, never reorder. */
  enum class State { Cold = 0, Dark, Search, Track, Engage, Scoot, Reload };
  static const char *StateName(State s);

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

  /* The six mission keys of doc/modules/ground/module.md §Spec 9, each already validated by the module. */
  /* Both applied in the spawn IC window, before the first Run(), so they set the state the position
   * BOOTS in rather than a flag something later has to interpret. */
  void SetEmconHold(bool on) {
    EmconHold_ = on;
    if (State_ != State::Cold) State_ = on ? State::Dark : State::Search;
  }
  void SetColdStart(bool on) { if (on) State_ = State::Cold; }
  void SetRounds(int n) { Rounds_ = n < 0 ? 0 : n; RailsLeft_ = RailsAtMost(); }
  void SetEngageMaxM(double m) { EngageMaxM_ = m; }
  void SetReactionS(double s) { ReactionS_ = s; }
  void SetScootS(double s) { ScootS_ = s; }

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
  void Enter(State s, const char *why);
  int  RailsAtMost() const;
  /* The envelope test, all four numbers plus the round's own reach in TIME. Returns false with a reason
   * so the trace says WHY a position that saw a target did not shoot at it. */
  bool InEnvelope(double rangeM, double tgtAltM, double siteAltM, const char **why) const;

  const FBSiteSpec *Spec_ = nullptr;
  const FBState *TrackBus_ = nullptr;   /* borrowed: the fire-control set's own block */

  State State_ = State::Search;
  bool   EmconHold_ = false;
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

  /* Telemetry copies. */
  int   TrackCount_ = 0, Beam0_ = 0, Beam1_ = 0, Launches_ = 0;
  bool  Lock_ = false, Cue_ = false;
  double EngagedS_ = 0.0;
};

} // namespace FlightBox::Modules
#endif
