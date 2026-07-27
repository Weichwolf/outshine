/* FlightBox — FBFdm: ONE simulated airframe's JSBSim instance, the ONE-TU seam to the vendored engine.
 * Getters are not inline (FB convention) because the header may not name the pimpl'd type they read.
 * Details: doc/flightbox/fdm.md */
#ifndef FBFDM_H
#define FBFDM_H

#include <memory>

namespace FlightBox {

struct fb_fdm_state {
  double roll, pitch, yaw;   /* deg (phi/theta/psi) */
  double p, q, r;            /* deg/s body rates */
  double lat, lon, elev;     /* deg geodetic, deg, m ASL */
  double speed, gs, cas;     /* true airspeed, groundspeed, calibrated airspeed (density-corrected), m/s */
  double mach;               /* Mach number (dimensionless) */
  double vx, vy, vz;         /* X-Plane local: +x east, +y up, +z south, m/s */
  double nx, ny, nz;         /* body load factors, g (long/lat/normal) */
  double alphaDeg;           /* angle of attack, deg (aero/alpha-deg) */
};

struct FBFdmSpawn;   /* fdm/FBFdmBoot.h — the IC declaration, reachable only through FBFdmBoot */

class FBFdm {
public:
  ~FBFdm();
  FBFdm(const FBFdm &) = delete;
  FBFdm &operator=(const FBFdm &) = delete;

  static constexpr double kStepS = 0.01;   /* 100 Hz — the ONE definition of the bridge step rate */

  /* Advances one kStepS step; `out` keeps its last good value if the engine raised (latched Faulted_),
   * so the step never throws into the module's substep loop. */
  void Step(fb_fdm_state &out);
  bool Faulted() const { return Faulted_; }

  /* ---- Commanded inputs: the ONLY way anything above this class influences the physics. ---- */

  /* roll/pitch/yaw in [-1,1], thr in [0,1]; the throttle is slew-limited before it reaches the engine. */
  void SetControls(double roll, double pitch, double yaw, double thr);

  void SetGear(double cmd);             /* [0,1], 1=down (gear/gear-cmd-norm) */
  void SetFlap(double cmd);             /* [0,1] */
  void SetSpeedbrake(double cmd);       /* [0,1] */
  void SetWheelBrakes(double left, double right);   /* per side, [0,1] */
  void SetNosewheelSteer(double cmd);   /* [-1,1]; the model's own gain block sets the deflection */

  /* Propulsion-wide starter_cmd/cutoff_cmd (FGPropulsion defaults to ALL engines). */
  void EngineStart();
  void EngineCutoff();

  /* FGPropulsion's own tank inventory; running dry starves the engine natively inside JSBSim. The
   * total/percent setters distribute proportional to EACH tank's capacity share. */
  void SetFuelTankLbs(int idx, double lbs);
  void SetFuelTotalLbs(double lbs);
  void SetFuelPct(double pct);          /* 0..100 of total declared capacity */

  /* ---- External stores through JSBSim's own mass/external-force models; locations in the loaded
   * model's STRUCTURAL frame (inches). doc/flightbox/fdm.md §9. ---- */

  int  AddStorePointMass(const char *name, double xIn, double yIn, double zIn);   /* index, or -1 */
  void SetStorePointMassLbs(int index, double lbs);
  /* Carriage drag AREA (CdA, ft^2) at the loaded stations' centroid; call per loadout CHANGE, not per
   * frame. cdaFt2 <= 0 never creates the force at all. */
  void SetStoresDrag(double cdaFt2, double xIn, double yIn, double zIn);

  /* ---- BATTLE DAMAGE as physics; values and their derivation in core/FBDamageModel. Set ONLY by the
   * unit's owner. Defaults are a no-op, so an undamaged airframe is bit-identical. ---- */

  void SetControlAuthority(double norm);   /* scales every commanded deflection inside SetControls */
  void SetThrottleLimit(double maxNorm);   /* caps commanded throttle: no afterburner range left */
  void SetDamageDrag(double cdaFt2);       /* own <external_reactions> channel, never the stores one */

  double GetQbarPsf() const;      /* what both drag channels are measured against */
  double GetCgXIn() const;        /* structural CG station */

  /* World truth from the App's elevation hook, so gear/contact collide against the DEM. */
  void SetGroundElevM(double m);

  /* World truth from the App's WEATHER hook (core/FBWeatherProvider), the air mass's own velocity in
   * m/s. JSBSim's FGWinds carries exactly this vector in ft/s and subtracts it from the ground velocity
   * to get the aerodynamic one, so a wind FROM 270 is (N=0, E=+v, D=0) and nothing about the airframe,
   * the FCS or the pilot has to know it exists. Never set = still air, bit-identical to before. */
  void SetWindNedMs(double n, double e, double d);

  /* ---- Readbacks: const, so a `const FBFdm&` is a read-only handle. ---- */

  /* What the FDM is CURRENTLY colliding against — proves the DEM value reached JSBSim. */
  double GetGroundElevM() const;

  /* CG height above ground when the lowest active contact touches, level. gearDown=false -> only fixed
   * structure (belly). Per model + gear state, so touchdown/crash/camera eye are geometry-true. */
  double GetGroundClearanceM(bool gearDown) const;

  double GetGearPos() const;            /* 0=up .. 1=down, kinematic-lagged */
  double GetSpeedbrakePos() const;      /* 0..1, lagged */
  bool GetWow() const;                  /* model-wide: true iff ANY bogey is compressed */

  /* True iff any ctSTRUCTURE contact is compressed — enumerates contact TYPE, never an index or an
   * aircraft name, so a model without such points simply always reads false. */
  bool GetStructureContact() const;

  /* Peak strut compression force (lbf) this tick — the model's own spring/damper reaction, not a
   * derived sink-rate heuristic. */
  double GetMaxGearForceLbs() const;

  double GetWeightLbs() const;
  bool GetEngineRunning(int engineIndex) const;

  int    GetFuelTankCount() const;
  double GetFuelTankLbs(int idx) const;
  double GetFuelTotalLbs() const;
  double GetFuelCapacityLbs() const;    /* sum of every tank's own declared capacity */

private:
  /* IC-Abschottung: private loading ctor, FBFdmBoot the ONE friend — a borrowed reference cannot
   * re-place, re-trim or re-spawn the airframe it is flying. */
  friend class FBFdmBoot;
  FBFdm();
  /* Load/Step are the exception FIREWALL around the unguarded bodies: a broken aircraft.xml must end as
   * a logged mission FAIL, not as std::terminate with no RESULT line to branch on. */
  bool Load(const FBFdmSpawn &spawn);
  bool LoadUnguarded(const FBFdmSpawn &spawn);
  void StepUnguarded(fb_fdm_state &out);
  bool EnsureDragForce(const char *name, double xIn, double yIn, double zIn);

  struct Impl;
  std::unique_ptr<Impl> P;
  bool Faulted_ = false;
};

} // namespace FlightBox
#endif
