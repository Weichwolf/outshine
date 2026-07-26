/* FlightBox — FBStoresSystem: the Stores Management slot every module carries — what hangs on this
 * aircraft, what it weighs, what it costs in drag, and the ONE path by which a store leaves it. It is
 * the airframe-agnostic DEFAULT (interface + implementation in one class, the FBSystemSlots.h pattern);
 * a module supplies its own pylon GEOMETRY (modules/f16/FBF16Sms declares the F-16's nine stations) and
 * may override Run() if its SMS genuinely behaves differently.
 *
 * WHAT MAKES THE LOADOUT REAL RATHER THAN A NUMBER ON A DISPLAY: every loaded station is a JSBSim POINT
 * MASS on the carrier's own FDM (fdm/FBFdm::AddStorePointMass), so mass, centre of gravity and the
 * inertia tensor are the engine's, not this class's — and the total carriage drag is a JSBSim external
 * force at the loaded stations' centroid. Releasing a store sets its point mass to zero in the same
 * tick: the aircraft becomes lighter, better balanced and cleaner AS PHYSICS, with nothing here
 * modelling the effect. This class only keeps the books and tells the engine what changed.
 *
 * IT NEVER SPAWNS ANYTHING. A release produces an FBStoreRelease record (core/FBStore.h) in a small
 * fixed queue; the OWNER of the simulation drains it and spawns the weapon as its own unit, because
 * producing an FDM requires fdm/FBFdmBoot.h, which nothing under systems/ or modules/ may include
 * (CLAUDE.md "Kein Cheaten"). That boundary is why the release path ends here in a value, not in a new
 * airframe.
 *
 * THE PICKLE IS A COMMAND, NOT A CALL. Release() is reached from the module's avionics command bus
 * (core/FBCommandBus), so it can be REFUSED and the refusal is on the record — master arm not in ARM
 * and weight on the wheels are hardware interlocks (doc/f16/controls-commands.md §6.2), an empty or
 * unselected station is the valid-command-wrong-context case (§6.5). */
#ifndef FBSTORESSYSTEM_H
#define FBSTORESSYSTEM_H

#include "FBAvionicsCommand.h"
#include "FBState.h"
#include "FBStore.h"
#include "FBTelemetry.h"
#include "FBFdm.h"

namespace FlightBox {

class FBStoresSystem : public FBTelemetrySource {
public:
  static constexpr int kMaxStations = kMaxStoreStations;
  /* Releases waiting for the owner to drain them. A ripple of every station this airframe could carry
   * still fits, so the queue can never be the thing that loses a store. */
  static constexpr int kMaxPendingReleases = kMaxStations;

  ~FBStoresSystem() override = default;

  /* ---- setup, once, by the module that owns this slot ---- */
  /* Declares station `number` (1-based, as the jet numbers its pylons) at a structural location in the
   * loaded model's own frame (inches). Stations must be declared before AttachFdm. */
  void DeclareStation(int number, double xIn, double yIn, double zIn);
  /* Binds the airframe and creates one JSBSim point mass per declared station (weight 0 = empty). */
  void AttachFdm(FBFdm &fdm);

  /* ---- loadout ---- */
  bool Load(int station, const FBStoreSpec &spec);   /* false: unknown station or already occupied */
  int  StationCount() const { return Count_; }
  int  LoadedCount() const;
  double LoadedMassLbs() const;
  FBStoreKind StoreAt(int station) const;

  void SetMasterArm(FBArmState s) { Arm_ = s; }
  FBArmState MasterArm() const { return Arm_; }

  /* ---- guided rounds ----
   * WHAT THE FIRE CONTROL HANDS THE SMS, once per tick: its current estimate of the target it has
   * solved for (core/FBWeaponUplink.h). The SMS neither computes nor second-guesses it — it copies it
   * onto a round at launch (the launch programming) and radiates it as the midcourse uplink afterwards.
   * Which BOX produced it is the module's business (the F-16's is modules/f16/FBF16FireControl), which
   * is why it arrives as data instead of this generic slot reaching for a concrete system. */
  void SetTargetState(const FBWeaponTargetState &t) { Target_ = t; }
  /* Who this aircraft is, so a launched round knows whose uplink to listen to (FBModule::SetUnitIdentity
   * -> the module wires it, exactly like the radar's and the terminal's identity). */
  void SetUnitId(int id) { SelfId_ = id; }

  /* What this aircraft is RADIATING to a round it launched (units/FBUnit's FBUnitSignature). Active
   * only while a lock-requiring store has actually been launched AND the fire control still has a
   * target: the moment the lock is lost the uplink stops, which is the whole tactical point of the
   * midcourse phase. It does NOT stop when the round's flight ends — nothing tells the launcher that,
   * and nothing tells the real one either; the transmitter goes quiet when the pilot stops supporting
   * the shot, which here means when the track is gone. */
  const FBWeaponUplink &Uplink() const { return Uplink_; }
  bool SelectStation(int station);   /* false: unknown or empty station */
  int  SelectedStation() const { return Selected_; }

  /* ---- the pickle ---- */
  /* Releases the SELECTED station's store, or refuses with the reason the jet would give (class
   * banner). Returns true iff a store left the aircraft. `nowS` stamps the release record. */
  bool Release(double nowS, FBCommandOutcome &outcome, FBCommandReason &reason);
  /* The owner's drain: hands out the oldest un-taken release, FIFO. False when there is none. */
  bool TakeRelease(FBStoreRelease &out);

  /* Publishes the stores block; the one override point. */
  virtual void Run(FBState &state, double dt);

  const char *TelemetryName() const override { return "sms"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

private:
  struct Station {
    double XIn = 0.0, YIn = 0.0, ZIn = 0.0;
    int    PointMass = -1;                       /* index in the FDM, -1 = not attached */
    FBStoreKind Kind = FBStoreKind::None;
    double MassLbs = 0.0, DragAreaFt2 = 0.0;
  };

  /* Re-states the whole loadout to the FDM: every point mass and the aggregate drag area at the loaded
   * stations' centroid. Called on load and on release — never per frame, because nothing about a
   * loadout changes between those two events. */
  void PublishLoadout();
  void SelectNextLoaded();
  int  IndexOf(int station) const;   /* station number -> array index, -1 if undeclared */

  Station Stations_[kMaxStations];
  int StationNumber_[kMaxStations]{};   /* the jet's own pylon numbers, in declaration order */
  int Count_ = 0;
  FBArmState Arm_ = FBArmState::Sim;   /* a jet powers up with the master arm SAFE */
  int Selected_ = -1;
  bool Wow_ = true;                    /* until air data says otherwise, assume on the ground */
  FBFdm *Fdm_ = nullptr;               /* borrowed, never owned (AttachFdm) */

  /* The guided-round state (see SetTargetState/Uplink above). LaunchZone_ is the last fire-control
   * solution the bus carried, cached in Run() so Release() — which is reached from the command bus, not
   * from a tick — can refuse a shot outside it without reaching for the bus itself. */
  int SelfId_ = 0;
  FBWeaponTargetState Target_{};
  FBWeaponUplink Uplink_{};
  bool   HaveZone_ = false, InZone_ = false;
  double ZoneRangeM_ = 0.0, ZoneMinM_ = 0.0, ZoneMaxM_ = 0.0, ZoneRtrM_ = 0.0;
  double ZoneTtaS_ = -1.0, ZoneTtiS_ = -1.0;
  bool   RadarLocked_ = false;
  int    GuidedInFlight_ = 0;          /* lock-requiring rounds this jet has launched this sortie */

  FBStoreRelease Pending_[kMaxPendingReleases]{};
  int PendingCount_ = 0;
  int Released_ = 0;
};

} // namespace FlightBox
#endif
