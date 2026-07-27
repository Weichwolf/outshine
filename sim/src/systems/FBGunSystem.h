/* FlightBox — FBGunSystem: the internal-gun slot every module carries — what gun this aircraft has, how
 * many rounds are left in it, and the ONE path by which rounds leave it. The airframe-agnostic DEFAULT
 * (interface + implementation in one class, the FBSystemSlots.h pattern); a module supplies its own gun
 * SPEC and installation geometry (modules/f16/FBF16Gun declares the M61A1 and where its bore sits) and
 * may override Run() if its gun genuinely behaves differently.
 *
 * IT IS systems/FBStoresSystem's SIBLING, and the parallels are exact where they should be:
 *   - the trigger is a COMMAND, not a call (core/FBCommandBus): it can be refused, it takes a hand's
 *     worth of time, and the refusal is on the record. Master arm not in ARM and weight on the wheels
 *     are the hardware interlocks (doc/f16/controls-commands.md §6.2); an empty drum is Depleted, which
 *     is the one refusal that is a fact about the AIRCRAFT rather than about the request;
 *   - IT NEVER RESOLVES A HIT. Firing produces FBGunBurst records (core/FBGun.h) in a small fixed queue;
 *     the OWNER of the simulation drains it, flies the rounds (core/FBGunProjectiles) and decides what
 *     they hit, on the published poses. A gun that scored itself would be the same cheat as a missile
 *     that scored itself (CLAUDE.md "Kein Cheaten").
 * And they differ where the weapons do: a store is released one at a time and becomes a unit; the gun
 * produces a CONTINUOUS stream, so what crosses the boundary is one bundle per tick of squeeze, for as
 * long as the trigger the pilot pulled says to keep firing.
 *
 * WHAT A TRIGGER COMMAND MEANS. Value = the DURATION of the squeeze in seconds (clamped to the gun's own
 * MaxBurstS, reported as Clamped rather than silently trimmed). A command bus models one pilot action
 * per post and enforces a floor between two actions on the same switch, so "hold the trigger" cannot be
 * a stream of commands — it is one command that says how long. A second squeeze while one is running
 * simply extends it to whichever ends later, which is what a pilot's finger does.
 *
 * THE ROUNDS COME OUT AT THE RATE, NOT AT THE TICK. Rate is rounds/minute over a spool-up ramp
 * (core/FBGun.h); this class integrates that rate across whatever dt it is cycled at and carries the
 * FRACTIONAL remainder, so the drum empties in exactly Capacity/rate seconds no matter how the module
 * happens to schedule the slot — and a 0.1 s tick at 6,000 rd/min is ten rounds, not "one burst". */
#ifndef FBGUNSYSTEM_H
#define FBGUNSYSTEM_H

#include "FBArmState.h"
#include "FBAvionicsCommand.h"
#include "FBGun.h"
#include "FBState.h"
#include "FBTelemetry.h"
#include "FBFdm.h"

namespace FlightBox {

class FBGunSystem : public FBTelemetrySource {
public:
  /* One bundle per cycle of the slot; a handful of entries is more than the owner can fall behind by,
   * since it drains the queue every tick. */
  static constexpr int kMaxPendingBursts = 4;

  ~FBGunSystem() override = default;

  /* ---- setup, once, by the module that owns this slot ----
   * `spec` is the catalogue entry (core/FBGun.h) and the offsets place the muzzle in the aircraft's own
   * body frame (+fwd/+right/+down from the CG, metres) with the bore's depression/cant in degrees. Both
   * are the module's knowledge: which gun this jet has and where it is bolted. */
  void Install(const FBGunSpec &spec, double fwdM, double rightM, double downM, double boreDownDeg,
               double boreRightDeg);
  bool Installed() const { return Spec_ != nullptr; }
  const FBGunSpec *Spec() const { return Spec_; }

  void SetMasterArm(FBArmState s) { Arm_ = s; }
  FBArmState MasterArm() const { return Arm_; }
  void SetUnitId(int id) { SelfId_ = id; }
  /* Loads the drum. Defaults to the gun's own capacity at Install; a mission may set it lower (an
   * aircraft that has been shooting) and never higher — a drum that held more than the gun's capacity
   * would be a different gun. */
  bool SetRounds(int rounds);
  int  RoundsRemaining() const { return Rounds_; }
  int  RoundsFired() const { return Fired_; }

  /* ---- the trigger ----
   * Squeezes for `seconds`, or refuses with the reason the jet would give (class banner). Returns true
   * iff the gun will fire. `nowS` is the module's own sim clock. */
  bool Trigger(double seconds, double nowS, FBCommandOutcome &outcome, FBCommandReason &reason);
  bool Firing() const { return FireUntilS_ > 0.0; }

  /* The owner's drain: hands out the oldest un-taken burst, FIFO. False when there is none. */
  bool TakeBurst(FBGunBurst &out);

  /* One cycle of the slot: fires whatever this dt is worth, publishes the gun block. The one override
   * point (the FBAutopilot::Run pattern). `st` is the airframe's live state — the muzzle's position and
   * the round's launch velocity come out of it, and that is the only thing this class reads from it. */
  virtual void Run(FBState &state, const fb_fdm_state &st, double nowS, double dt);

  const char *TelemetryName() const override { return "gun"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

private:
  const FBGunSpec *Spec_ = nullptr;
  double MuzzleFwdM_ = 0.0, MuzzleRightM_ = 0.0, MuzzleDownM_ = 0.0;
  double BoreDownDeg_ = 0.0, BoreRightDeg_ = 0.0;

  FBArmState Arm_ = FBArmState::Sim;   /* a jet powers up with the master arm SAFE */
  int    SelfId_ = 0;
  int    Rounds_ = 0, Fired_ = 0;
  bool   Wow_ = true;                  /* until air data says otherwise, assume on the ground */
  double FireUntilS_ = 0.0;            /* the squeeze's end, absolute sim time; 0 = not firing */
  double FireStartS_ = 0.0;            /* ...and its start, for the spool-up ramp */
  double Fraction_ = 0.0;              /* rounds owed but not yet whole (see the class banner) */
  int    Triggers_ = 0, Refused_ = 0;
  double LastBurstRounds_ = 0.0;
  int    BlockStatus_ = 0;             /* this source's telemetry column, cached in Run() */
  /* The aiming solution, read off the fire-control block for telemetry only (see Run). -1 = none. */
  float  SolRangeM_ = -1.0f, SolAimErrDeg_ = -1.0f, SolSpanMr_ = -1.0f;
  int    SolInFunnel_ = 0;

  FBGunBurst Pending_[kMaxPendingBursts]{};
  int PendingCount_ = 0;
};

} // namespace FlightBox
#endif
