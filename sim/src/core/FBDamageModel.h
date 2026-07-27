/* FlightBox — FBDamageModel: what a weapon burst DOES to an aircraft. The ONE writer of
 * core/FBSystemHealth (it is that class's only friend), owned by the client that owns the simulation —
 * a module never resolves its own damage, exactly as it never judges its own crash (CLAUDE.md "Kein
 * Cheaten").
 *
 * IT IS A MODEL, AND IT SAYS SO. Nothing below is a measurement. What IS observed and checkable is the
 * input: the burst geometry (the runner's own closest-approach computation on the published poses), the
 * closure, and the warhead mass out of the store catalogue. What is MODELLED is the step from those
 * three numbers to a system state, and it is built from the two things that are actually physics —
 * isotropic fragment spread and kinetic energy — plus one threshold per system, which is a setting.
 *
 * THE ENERGY (FBFragmentFluxJm2 below), in three steps, each with its assumption stated:
 *   1. FRAGMENT MASS: kCaseFraction of the warhead mass becomes fragments. 0.5 [SET] is the usual order
 *      for a blast-fragmentation case; doc/f16/weapons.md §4.7 lists warhead internals as a genuine gap,
 *      so this is a stated setting and not a citation.
 *   2. AREAL DENSITY: those fragments spread ISOTROPICALLY, so at range r a surface sees
 *      m_frag / (4*pi*r^2) kg/m^2. The isotropy is the model's one geometric assumption — a real
 *      warhead sprays into a focused band, which would make the result depend on the burst's angle off
 *      the missile's own axis, and nothing here claims to know that band.
 *   3. SPECIFIC ENERGY: each fragment arrives at the vector sum of its ejection velocity (kFragSpeedMs,
 *      radial) and the closure between the two aircraft. For a radially symmetric spray the mean
 *      magnitude of that sum is sqrt(v_eject^2 + v_closure^2), which is what is used — deliberately not
 *      v_eject + v_closure, which would only be true for the fragments thrown straight ahead.
 *      flux = 0.5 * areal_density * v_eff^2, in J/m^2.
 * The result is a 1/r^2 law in energy: doubling the miss distance quarters what arrives. That, and not
 * any single threshold, is what makes the model behave sensibly at ranges nobody calibrated it at.
 *
 * THE ZONES. An aircraft is not a point: where the burst sits relative to the AIRFRAME AXIS decides
 * which systems are near it. So the layout (module data — modules/f16/FBF16Damage) cuts the airframe
 * into zones along its own longitudinal axis and names which systems sit in each, and this file
 * computes, PER ZONE, the distance from the burst to that zone's own stretch of axis and the flux at
 * it. Every zone is evaluated, not just the nearest one: fragments go everywhere, they simply arrive
 * thinner further away, and letting the 1/r^2 law say so is more honest than partitioning the airframe
 * and giving one partition everything. The airframe is modelled as that axis segment and nothing more —
 * no cross-section, no shielding, no fragment count.
 *
 * DETERMINISM IS STRUCTURAL: there is no random number anywhere in this file, no time dependence and no
 * hidden state. Same geometry, same warhead, same closure -> same masks, always. */
#ifndef FBDAMAGEMODEL_H
#define FBDAMAGEMODEL_H

#include <cstdint>
#include "FBSystemHealth.h"

namespace FlightBox {

/* The zone names are generic (an aircraft has a front, a middle and a back); WHERE those zones sit on a
 * given airframe and WHAT lives in them is the module's own table. */
enum class FBDamageZone : uint8_t { None = 0, Nose, Forward, Center, Aft };
const char *FBDamageZoneStr(FBDamageZone z);

/* One system in one zone, with the two energies at which it degrades and fails (J/m^2). A system with
 * no derivable degraded behaviour simply declares DegradeJm2 == FailJm2 and therefore never has one. */
struct FBZoneSystem {
  FBSystemId Id = FBSystemId::Engine;
  double DegradeJm2 = 0.0;
  double FailJm2 = 0.0;
};

/* One zone: the stretch of the airframe's longitudinal axis it occupies (metres from the CG, + forward,
 * so AftM < FwdM) and the systems in it. Plain arrays with a count — the whole layout is a compile-time
 * table a module hands out by const reference; nothing allocates. */
struct FBDamageZoneSpec {
  FBDamageZone Zone = FBDamageZone::None;
  double AftM = 0.0, FwdM = 0.0;
  const FBZoneSystem *Systems = nullptr;
  int SystemCount = 0;
};

/* The layout, plus the ONE piece of airframe geometry a projectile stream needs that a fragment burst
 * does not: how big a target the aircraft presents to something arriving from a given direction. A
 * warhead sprays isotropically and the airframe's cross-section never enters its arithmetic (see the
 * banner); a burst of gunfire is a narrow pattern that either lands on the aircraft or does not, so the
 * PRESENTED AREA is what decides how much of it arrives. Two numbers rather than one, because the
 * difference between them is a factor of three on any fighter and it is free to interpolate: the area
 * seen along the longitudinal axis, and the area seen across it. A module that declares neither (the
 * default, and every released store) presents nothing and takes no gun damage — which is correct: a
 * bomb in free flight is not something anybody shoots at. */
struct FBDamageLayout {
  const FBDamageZoneSpec *Zones = nullptr;
  int ZoneCount = 0;
  double FrontalAreaM2 = 0.0;   /* seen head-on/from astern */
  double LateralAreaM2 = 0.0;   /* seen from the side or from above */
  /* ...and how far the airframe REACHES in those two views (metres from its centre): half the span seen
   * from astern, half the length seen from the side. The area says how much material there is, this says
   * how far out it is scattered — see core/FBGunBallistics.h's two-scale hit model for why one number
   * cannot do both jobs. Zero = undeclared, and the hit model then treats the target as compact. */
  double FrontalExtentM = 0.0;
  double LateralExtentM = 0.0;
};

/* The presented area for a stream arriving along `fwd/right/down` in the target's own body frame (the
 * vector need not be normalised). |cos| of the axis angle times frontal, |sin| times lateral — the
 * simplest interpolation that is exact at both ends, and no claim at all about the shape in between. */
double FBPresentedAreaM2(const FBDamageLayout &layout, double fwd, double right, double down);
/* ...and the matching half-extent for that same direction, interpolated the same way. */
double FBPresentedExtentM(const FBDamageLayout &layout, double fwd, double right, double down);

/* ONE burst, as the owner of the simulation measured it: WHERE it went off in the target's own body
 * frame (+forward/+right/+down from the CG, metres — the runner rotates its closest-approach vector
 * into it), how fast the two were closing, and how much warhead was in it. */
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

/* ---- The fragmentation model's own two parameters (see the file banner, both [SET]) ---- */
constexpr double kCaseFraction = 0.5;    /* warhead mass that becomes fragments */
constexpr double kFragSpeedMs = 1800.0;  /* initial fragment ejection speed */

/* ---- The PHYSICAL consequences, all in one place so the whole "what damage feels like" model can be
 * read at once. Every one of them is applied through JSBSim (units/FBSimUnit::ApplyDamageToAirframe ->
 * fdm/FBFdm), never by a second, parallel flight model. ---- */

/* FLCS/hydraulics. A degraded system is HALF the commanded surface deflection [SET, but it is the one
 * number with a structural reason: the F-16 has two independent hydraulic systems driving its actuators,
 * so losing one is the natural meaning of "degraded"]. A failed one is no authority at all — the
 * surfaces stop answering and the aircraft flies on whatever trim and stability the model has left,
 * which is exactly the departure JSBSim then integrates for itself. */
constexpr double kAuthorityDegraded = 0.5;
constexpr double kAuthorityFailed = 0.0;

/* Propulsion. Degraded = the afterburner is gone: the throttle cannot be commanded past military power.
 * 0.6 is where the F-16 model's own throttle range puts the AB gate [DERIVED from the model's
 * throttle-cmd-norm convention]. Failed = fuel cutoff, i.e. JSBSim's own engine-out, no thrust term
 * invented here. */
constexpr double kThrottleLimitDegraded = 0.6;

/* Structure. Battle damage is holes and torn skin: extra drag, applied as a drag AREA through the same
 * <external_reactions> mechanism the carriage drag already uses (fdm/FBFdm::SetDamageDrag), acting
 * through the CG so no pitching moment is claimed that nobody can source. 1.5 / 6.0 ft^2 [SET] — for
 * scale, a clean F-16's own zero-lift drag area is of the order of 4 ft^2, so a degraded airframe is
 * "noticeably dirty" and a failed one is "flying with a hole in it". */
constexpr double kDamageDragFt2Degraded = 1.5;
constexpr double kDamageDragFt2Failed = 6.0;

/* Radar. A degraded set is half its antenna aperture, and detection range follows the radar equation:
 * R^4 ~ Pt*G^2 with G ~ A, so R ~ sqrt(A) and half the aperture is 1/sqrt(2) of the range. [DERIVED] */
constexpr double kRadarRangeDegraded = 0.7071067811865476;

/* The energy arriving at a surface `rangeM` from a burst of `warheadKg`, with the two aircraft closing
 * at `closureMs` (J/m^2). Public so a report, a harness or a log line can reproduce the exact number
 * behind a damage verdict instead of trusting it. */
double FBFragmentFluxJm2(double warheadKg, double rangeM, double closureMs);

/* ONE BURST OF GUNFIRE ARRIVING ON AN AIRFRAME — the kinetic counterpart of FBBurst, and deliberately a
 * different input type rather than a flag on the same one, because the two weapon effects are known
 * through different things. A warhead is known by its MASS and the model derives the energy that reaches
 * a surface from it (fragment spread, 1/r^2). A gun burst is known by the ENERGY DENSITY it delivers,
 * which the owner of the simulation has already computed from the round count, the impact speed and the
 * pattern's spread at that range (core/FBGunBallistics.h's FBGunFluxJm2) — a number this file has no
 * business re-deriving and could not, since it never sees a round.
 *
 * WHAT IT SHARES WITH A WARHEAD BURST, and why that is legitimate: the DESTINATION. Both express what
 * arrives as J/m^2 of areal energy at a place on the airframe, and both are judged against the same
 * per-system thresholds, so one damage register answers for both without a second, uncalibrated set of
 * numbers. That is a stated modelling decision and not a physical claim: 20 mm impacts and warhead
 * fragments do not damage structure by the same mechanism, and expressing both as areal energy is the
 * common currency this simulator uses, not a statement that they are equivalent.
 *
 * WHERE IT LANDS. A bullet stream is a narrow pattern, not an isotropic spray, so it does NOT reach
 * every zone: the rounds arrive in a footprint of radius ~SpreadM about the point where the stream's
 * axis passed the airframe, and only the zones that footprint overlaps see anything at all. That, and
 * not any threshold, is what makes a gun a surgical weapon and a warhead an area one. */
struct FBKineticBurst {
  double FwdM = 0.0;        /* where the stream's axis passed, along the airframe axis from the CG */
  double FluxJm2 = 0.0;     /* areal energy arriving there */
  double SpreadM = 0.0;     /* the pattern's sigma at the target — the footprint's own size */
  double Rounds = 0.0;      /* expected rounds on the airframe, for the record */
  double ImpactSpeedMs = 0.0;
};

class FBDamageModel {
public:
  /* Resolves one burst against one aircraft's layout and health register. Returns what changed; the
   * register itself is the state. A layout with no zones (the default for any module that has not
   * declared one — a released store, for instance) takes no damage and returns an empty result. */
  static FBDamageResult Apply(const FBBurst &burst, const FBDamageLayout &layout, FBSystemHealth &health);

  /* ...and the same for a burst of gunfire (see FBKineticBurst). Same register, same thresholds, same
   * monotone rules, same absence of randomness — only the geometry of what arrives differs. */
  static FBDamageResult ApplyKinetic(const FBKineticBurst &burst, const FBDamageLayout &layout,
                                     FBSystemHealth &health);
};

} // namespace FlightBox
#endif
