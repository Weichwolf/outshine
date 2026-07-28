/* What a weapon burst DOES to an aircraft — the ONE writer of core/FBSystemHealth (its only friend),
 * owned by the client that owns the simulation. It is a MODEL, and it says so: only the inputs (burst
 * geometry, closure, warhead mass) are observed. Determinism is structural — no randomness, no time
 * dependence, no hidden state. Energy chain, zone rule and every constant below:
 * doc/core.md, Abschnitt 6.2. */
#ifndef FBDAMAGEMODEL_H
#define FBDAMAGEMODEL_H

#include <cstdint>
#include "FBSystemHealth.h"

namespace FlightBox {

/* Generic names; WHERE the zones sit and WHAT lives in them is the module's own table. */
enum class FBDamageZone : uint8_t { None = 0, Nose, Forward, Center, Aft };
const char *FBDamageZoneStr(FBDamageZone z);

/* One system in one zone; DegradeJm2 == FailJm2 = no derivable degraded behaviour. */
struct FBZoneSystem {
  FBSystemId Id = FBSystemId::Engine;
  double DegradeJm2 = 0.0;
  double FailJm2 = 0.0;
};

/* One zone: the stretch of longitudinal axis it occupies (m from the CG, + forward) and its systems. */
struct FBDamageZoneSpec {
  FBDamageZone Zone = FBDamageZone::None;
  double AftM = 0.0, FwdM = 0.0;
  const FBZoneSystem *Systems = nullptr;
  int SystemCount = 0;
};

/* The layout, plus the geometry only a projectile stream needs: what the airframe PRESENTS from a
 * given direction. Undeclared (the default, and every released store) = presents nothing, takes no gun
 * damage. doc/core.md, Abschnitt 6.2. */
struct FBDamageLayout {
  const FBDamageZoneSpec *Zones = nullptr;
  int ZoneCount = 0;
  double FrontalAreaM2 = 0.0;   /* seen head-on/from astern */
  double LateralAreaM2 = 0.0;   /* seen from the side or from above */
  /* How far it REACHES in those views (half span from astern, half length from the side). Area says how
   * much material, extent how far out it is scattered — the two-scale hit model needs both. */
  double FrontalExtentM = 0.0;
  double LateralExtentM = 0.0;
  /* The THIRD orthogonal view, from directly above or below. The gun's area law folds it into
   * LateralAreaM2 (its `across` term is one number for side and top), which is why there is no plan
   * AREA here — but an EYE measures the largest DIMENSION of a silhouette rather than its area
   * (doc/sensors.md §9.4), and the plan view's largest dimension is a separate fact from the side
   * view's. It sits here and not in a table of its own because the gun and the eye look at the same
   * aeroplane: two presented-geometry tables would be two truths about one airframe. Undeclared = the
   * lateral figure, i.e. exactly the behaviour of a two-view layout. */
  double PlanExtentM = 0.0;
};

/* Direction is in the target's BODY frame and need not be normalised. |cos|*frontal + |sin|*lateral:
 * exact at both ends, no claim about the shape in between. */
double FBPresentedAreaM2(const FBDamageLayout &layout, double fwd, double right, double down);
double FBPresentedExtentM(const FBDamageLayout &layout, double fwd, double right, double down);

/* ONE burst as the owner of the simulation measured it: where it went off in the target's body frame
 * (+fwd/+right/+down from the CG, m), the closure, and the warhead mass. */
struct FBBurst {
  double FwdM = 0.0, RightM = 0.0, DownM = 0.0;
  double ClosureMs = 0.0;
  double WarheadKg = 0.0;
};

struct FBDamageResult {
  FBDamageZone Zone = FBDamageZone::None;   /* the zone that took the highest flux */
  double RangeM = 0.0;                      /* burst -> that zone's structure */
  double PeakFluxJm2 = 0.0;
  uint32_t NewlyFailed = 0, NewlyDegraded = 0;   /* bitmasks over FBSystemId — what THIS burst changed */
  bool WasEffective = true, NowEffective = true;
  bool Changed() const { return NewlyFailed != 0 || NewlyDegraded != 0; }
};

/* ---- The fragmentation model's own two parameters, both [SET] ---- */
constexpr double kCaseFraction = 0.5;    /* warhead mass that becomes fragments */
constexpr double kFragSpeedMs = 1800.0;  /* initial fragment ejection speed */

/* ---- The PHYSICAL consequences, all applied through JSBSim and never by a second flight model.
 * Each derivation: doc/core.md, Abschnitt 6.2 ("Die physischen Folgen"). ---- */

/* Half commanded deflection [SET, structural: the F-16 has two independent hydraulic systems]. */
constexpr double kAuthorityDegraded = 0.5;
constexpr double kAuthorityFailed = 0.0;

/* AB gate in the model's own throttle-cmd-norm convention [DERIVED]; failed = JSBSim's own cutoff. */
constexpr double kThrottleLimitDegraded = 0.6;

/* ONE engine of a TWIN out [DERIVED]: half the installed thrust is gone, and on a deck whose engines
 * share one throttle-cmd-norm the honest expression of that is half the commanded range. It sits below
 * the augmentation gate, so it also takes the afterburner with it — which is what losing an engine
 * does. A single-engine airframe never reaches this branch (its only engine failing is a cutoff). */
constexpr double kThrottleLimitOneEngineOut = 0.5;

/* Extra drag AREA through <external_reactions>, through the CG — no pitching moment claimed. [SET] */
constexpr double kDamageDragFt2Degraded = 1.5;
constexpr double kDamageDragFt2Failed = 6.0;

/* Half aperture over the radar equation: R ~ sqrt(A) -> 1/sqrt(2). [DERIVED] */
constexpr double kRadarRangeDegraded = 0.7071067811865476;

/* Public so a report or log line can reproduce the exact number behind a damage verdict. */
double FBFragmentFluxJm2(double warheadKg, double rangeM, double closureMs);

/* ONE burst of gunfire — a DIFFERENT input type, not a flag on FBBurst: a warhead is known by its mass
 * and this model derives the energy, a gun burst is known by the energy DENSITY the caller already
 * computed (core/FBGunBallistics.h). Shared destination (J/m^2 against the same thresholds) is a stated
 * modelling decision, not a claim that the two mechanisms are equivalent. It lands in a FOOTPRINT, not
 * on every zone. doc/core.md, Abschnitt 6.2. */
struct FBKineticBurst {
  double FwdM = 0.0;        /* where the stream's axis passed, along the airframe axis from the CG */
  double FluxJm2 = 0.0;     /* areal energy arriving there */
  double SpreadM = 0.0;     /* the pattern's sigma at the target — the footprint's own size */
  double Rounds = 0.0;      /* expected rounds on the airframe, for the record */
  double ImpactSpeedMs = 0.0;
};

class FBDamageModel {
public:
  /* Returns what changed; the register itself is the state. A layout with no zones takes no damage. */
  static FBDamageResult Apply(const FBBurst &burst, const FBDamageLayout &layout, FBSystemHealth &health);

  /* Same register, thresholds and monotone rules — only the geometry of what arrives differs. */
  static FBDamageResult ApplyKinetic(const FBKineticBurst &burst, const FBDamageLayout &layout,
                                     FBSystemHealth &health);

private:
  /* The layout is the airframe's declaration of WHAT IT HAS; the register needs one bit of it (a second
   * engine). Walked whole rather than per-hit zone, so the answer cannot depend on where a burst landed. */
  static void NoteLayout(const FBDamageLayout &layout, FBSystemHealth &health);
};

} // namespace FlightBox
#endif
