/* ONE unit's system health register. Owned by the CLIENT, fed only by a core-owned judgement, and READ
 * — never written — by the module that flies the aircraft.
 * THE WRITE GATE IS THE TYPE: every mutator is private with exactly one friend, core/FBDamageModel.
 * MONOTONE by decision: no repair in flight, so a run's damage picture is a function of the bursts it
 * took and nothing else. What the three states mean to a consumer: doc/core.md, Abschnitt 6.1. */
#ifndef FBSYSTEMHEALTH_H
#define FBSYSTEMHEALTH_H

#include <cstdint>
#include "FBTelemetry.h"

namespace FlightBox {

/* The module SLOT set plus the three non-avionics items whose consequence JSBSim itself can carry.
 * Append only: the ordinal is telemetry-visible (the dmg_* bitmask columns). */
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
  Gun,              /* the internal gun: drum, feed and barrels */
  /* THE SECOND ENGINE, appended for exactly the reason this enum says to append: the ordinal is the
   * bit position in the dmg_* telemetry masks, and every trace ever measured carries the old ones. A
   * single-engine airframe declares only `Engine` in its zone table, so this bit can never be set on
   * one, and CombatEffective (below) keeps its old meaning for it word for word. A TWIN declares both,
   * and "one engine out" — the state a two-engine fighter is actually characterised by — finally has
   * somewhere to live. Which nacelle is which is the MODULE's table, not this list's business. */
  Engine2,
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

  /* PROPULSION as a whole, which is not the same question as "is engine 0 out" on a twin. The register
   * learns how many an airframe has from the only place that knows — the module's own damage layout,
   * walked by FBDamageModel on the first burst. Before any burst the answer is the same either way, so
   * there is no window in which the two readings differ. A single-engine airframe never declares
   * Engine2, so this reduces to Failed(Engine) word for word. */
  bool PropulsionOut() const {
    if (!Failed(FBSystemId::Engine)) return false;
    return !HasEngine2_ || Failed(FBSystemId::Engine2);
  }

  /* A stated model decision: ineffective once the AIRFRAME cannot finish the sortie. Avionics losses,
   * however total, are explicitly NOT part of it — this judges the SORTIE, not the engagement, and a
   * unit is not "dead" when it turns false: it flies exactly as long as the physics lets it. */
  bool CombatEffective() const {
    return !PropulsionOut() && !Failed(FBSystemId::FlightControls) &&
           !Failed(FBSystemId::Structure);
  }

private:
  friend class FBDamageModel;   /* THE one writer */
  /* Monotone: a state only ever gets worse. Returns true if this call actually changed it. */
  bool Worsen(FBSystemId id, FBHealthState s);
  void NoteHit() { Hits_++; }
  /* What this AIRFRAME has, learned from its layout rather than declared twice. Only Engine2 needs it:
   * every other id means the same thing whether the aircraft carries one of the box or none. */
  void NoteSystemPresent(FBSystemId id) { if (id == FBSystemId::Engine2) HasEngine2_ = true; }
  /* The one piece of damage state that is not a system's: cumulative PROJECTILE energy per zone. A gun
   * is a continuous stream cut into per-tick bundles, so judging each bundle alone would make the damage
   * a function of the tick rate. A warhead has no equivalent — a burst is ONE event. */
  double AddKinetic(int zone, double fluxJm2);

  static constexpr int kMaxZones = 5;   /* FBDamageZone, including None */
  double Kinetic_[kMaxZones]{};
  FBHealthState S_[(int)FBSystemId::Count]{};
  uint32_t Failed_ = 0, Degraded_ = 0;   /* the same information as S_, as the telemetry bitmasks */
  int Hits_ = 0;
  bool HasEngine2_ = false;              /* see NoteSystemPresent/PropulsionOut */
};

/* Its own source, registered LAST by the unit: a new source appends columns and moves nothing that was
 * ever measured. */
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
