/* FlightBox — FBSystemHealth: ONE unit's system health register — which of its systems still work, and
 * how well. FBFlightMonitor's and FBMissionMonitor's structural sibling: owned by the CLIENT (one per
 * units/FBSimUnit), fed only by a core-owned judgement, and read — never written — by the module that
 * flies the aircraft. A module deciding its own damage would be the same class of cheat as a module
 * declaring its own crash.
 *
 * THE WRITE GATE IS THE TYPE, NOT A CONVENTION. Every mutator below is PRIVATE and has exactly one
 * friend: core/FBDamageModel, which produces a state change only as the result of a resolved weapon
 * burst. So there is no API anywhere — not on a const handle, not on a non-const one — by which a
 * system, a pilot or a module can mark itself (or anybody else) damaged or repaired; a `grep -rn
 * FBSystemHealth src/systems src/modules` finds reads only, and it cannot find anything else, because
 * nothing else compiles.
 *
 * MONOTONE BY DECISION: a state never improves. There is no repair in flight, and a monotone register
 * is what makes a run's damage picture a function of the bursts it took and of nothing else — no
 * ordering question, no healing race between two systems that observe it.
 *
 * WHAT THE THREE STATES MEAN TO A CONSUMER (the coupling that gives this file its point, see
 * core/FBAvionicsBlocks.h + core/FBBlockStatus.h):
 *   Intact   — the system runs and publishes its output block normally.
 *   Degraded — it still runs and still publishes, with reduced performance where a reduced performance
 *              can be DERIVED (the radar's range, the engine's ceiling, the FLCS's authority). Where it
 *              cannot, a system has no degraded behaviour and its layout entry simply never produces
 *              one (modules/f16/FBF16Damage).
 *   Failed   — the system does not run and does not publish. Its block goes Invalid, and everything the
 *              avionics bus already does with an invalid block follows by itself: the HUD dashes the
 *              readout, the warning set reports the condition as INHIBITED rather than as absent, and
 *              the pilot refuses the actions whose data basis is gone. None of that is re-implemented
 *              here; that it happens for free is the reason the bus carries validity heads at all. */
#ifndef FBSYSTEMHEALTH_H
#define FBSYSTEMHEALTH_H

#include <cstdint>
#include "FBTelemetry.h"

namespace FlightBox {

/* The system inventory a damage model can address. Deliberately the module SLOT set (CLAUDE.md's
 * systems/ inventory) plus the three physical items a hit can take out that are not avionics boxes —
 * engine, flight controls, structure — because those are exactly the ones whose consequence JSBSim
 * itself can carry. Append only: the ordinal is telemetry-visible (the dmg_* bitmask columns). */
enum class FBSystemId : uint8_t {
  Engine = 0,       /* propulsion: thrust */
  FlightControls,   /* FLCS/hydraulics: control authority */
  Structure,        /* airframe: drag */
  AirData,          /* ADC + its probes */
  RadarAlt,         /* radar altimeter (CARA) */
  Nav,              /* INS/navigation */
  Radar,            /* the active air-to-air set */
  FireControl,      /* the fire-control computer's launch zone */
  Stores,           /* SMS: the racks and their wiring */
  Datalink,         /* the net terminal */
  Rwr,              /* the warning receiver */
  Countermeasures,  /* the dispenser */
  Gun,              /* the internal gun: drum, feed and barrels — APPENDED, see the enum's own rule */
  Count
};

const char *FBSystemIdStr(FBSystemId id);

enum class FBHealthState : uint8_t { Intact = 0, Degraded = 1, Failed = 2 };
const char *FBHealthStateStr(FBHealthState s);

class FBSystemHealth {
public:
  FBHealthState State(FBSystemId id) const { return S_[(int)id]; }
  bool Ok(FBSystemId id) const { return S_[(int)id] == FBHealthState::Intact; }
  bool Degraded(FBSystemId id) const { return S_[(int)id] == FBHealthState::Degraded; }
  bool Failed(FBSystemId id) const { return S_[(int)id] == FBHealthState::Failed; }
  /* "may this system run at all" — the gate a module asks before cycling a slot. */
  bool Working(FBSystemId id) const { return S_[(int)id] != FBHealthState::Failed; }

  bool Damaged() const { return Failed_ != 0 || Degraded_ != 0; }
  uint32_t FailedMask() const { return Failed_; }
  uint32_t DegradedMask() const { return Degraded_; }
  int Hits() const { return Hits_; }

  /* THE MISSION-LEVEL QUESTION, and a stated model decision: an aircraft is combat-ineffective once the
   * AIRFRAME can no longer complete its sortie — engine, flight controls or structure failed. Avionics
   * losses, however total, are explicitly NOT part of it: a jet with a dead radar and dead racks is out
   * of the fight but still flies, and what this predicate feeds (core/FBMissionMonitor) judges the
   * SORTIE, not the engagement. The unit is not "dead" when this turns false — it keeps flying exactly
   * as long as the physics lets it (CLAUDE.md's honest path: a destroyed jet falls because its engine is
   * out and its controls are gone, not because a flag said so). */
  bool CombatEffective() const {
    return !Failed(FBSystemId::Engine) && !Failed(FBSystemId::FlightControls) &&
           !Failed(FBSystemId::Structure);
  }

private:
  friend class FBDamageModel;   /* THE one writer — see the class banner */
  /* Monotone: a state only ever gets worse. Returns true if this call actually changed it. */
  bool Worsen(FBSystemId id, FBHealthState s);
  void NoteHit() { Hits_++; }
  /* THE ONE PIECE OF DAMAGE STATE THAT IS NOT A SYSTEM'S: how much areal energy a zone of this airframe
   * has taken from PROJECTILES, cumulatively. It exists because a gun is a continuous stream that this
   * simulator necessarily cuts into per-tick bundles (core/FBGun.h) — judging each bundle on its own
   * would make the damage a function of the tick rate, which is precisely what CLAUDE.md Prinzip 4
   * forbids. A warhead has no equivalent and does not use this: a burst is ONE event, and one event's
   * energy is what it is. `zone` is the FBDamageZone ordinal; the array is sized for all of them. */
  double AddKinetic(int zone, double fluxJm2);

  static constexpr int kMaxZones = 5;   /* core/FBDamageModel's FBDamageZone, including None */
  double Kinetic_[kMaxZones]{};
  FBHealthState S_[(int)FBSystemId::Count]{};
  uint32_t Failed_ = 0, Degraded_ = 0;   /* the same information as S_, as the telemetry bitmasks */
  int Hits_ = 0;
};

/* The register's telemetry channel (core/FBTelemetry.h). Its own source rather than more columns on an
 * existing one, and registered LAST by the unit, for the appending rule every source in this tree
 * follows: a new source adds columns at the right-hand end and moves nothing that was ever measured. */
class FBSystemHealthTelemetry : public FBTelemetrySource {
public:
  explicit FBSystemHealthTelemetry(const FBSystemHealth &health) : H_(health) {}

  const char *TelemetryName() const override { return "dmg"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

private:
  const FBSystemHealth &H_;   /* borrowed — the unit owns the register */
};

} // namespace FlightBox
#endif
