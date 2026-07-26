/* FlightBox — FBFdm: ONE simulated airframe's JSBSim instance. This is the ONE-TU seam between our C++
 * and the vendored JSBSim engine (vendor/jsbsim, read-only per CLAUDE.md Prinzip 1): FBFdm.cpp is the
 * ONLY translation unit that includes a JSBSim header — every caller sees this class plus the flat POD
 * state below, never FGFDMExec. The engine lives behind a pimpl for exactly that reason, which is also
 * why the getters are NOT inline in the header (the usual FB convention): they cannot be, the header may
 * not name the type they read from.
 *
 * INSTANCE, not process: every piece of state the engine needs sits in this object, so N FBFdm objects
 * coexist in one process with independent physics (each FGFDMExec builds its own property root — see
 * FBFdm.cpp's "not instance-safe in JSBSim" note for the two things that stay process-wide).
 *
 * OWNERSHIP: whoever owns the simulated entity owns its FBFdm (today: the App/mission runner, one per
 * run — perspectively units/FBUnit, one per unit). Modules and systems BORROW it: `FBFdm&` to command,
 * `const FBFdm&` to read. That const is load-bearing — every command method is non-const and every
 * readback is const, so a read-only handle cannot write the FDM (CLAUDE.md "Kein Cheaten", enforced by
 * the type system rather than by convention).
 *
 * IC/trim is NOT here: an FBFdm can only be produced fully loaded and trimmed, by FBFdmBoot::Spawn
 * (fdm/FBFdmBoot.h — the App-only boot header). There is no Init/Reset method a borrowed reference could
 * reach, so nothing in systems/ or modules/ can re-place, re-trim or re-spawn the airframe it is flying.
 *
 * fb_fdm_state units: angles deg, rates deg/s, lat/lon deg GEODETIC, elev m ASL, speeds m/s, vx/vy/vz
 * X-Plane-local (E,+up,S) m/s (a legacy convention from the pre-pivot bridge, kept because the renderer/
 * HUD math already consumes it), nz g. The POD keeps its snake_case name: it is the FBModule::Run
 * contract every module and telemetry source is written against. */
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
  double alphaDeg;           /* angle of attack, deg (aero/alpha-deg) — mission telemetry's stall check */
};

struct FBFdmSpawn;   /* fdm/FBFdmBoot.h — the IC declaration, reachable only through FBFdmBoot */

class FBFdm {
public:
  ~FBFdm();
  FBFdm(const FBFdm &) = delete;
  FBFdm &operator=(const FBFdm &) = delete;

  /* The fixed bridge step Step() advances (100 Hz). The one definition of that rate — the module's
   * substep accumulator and the test harnesses read it here rather than repeating 0.01. */
  static constexpr double kStepS = 0.01;

  /* Advance one fixed kStepS step, then read the resulting state into `out`. */
  void Step(fb_fdm_state &out);

  /* ---- Commanded inputs: the ONLY way anything above this class influences the physics (the simulated
   * control surface — CLAUDE.md "Kein Cheaten"). ---- */

  /* FBFlightControl's stick/throttle output, normalized: roll/pitch/yaw in [-1,1], thr in [0,1]. The
   * throttle is slew-limited (an ESC/spool ramp) before it reaches the engine. */
  void SetControls(double roll, double pitch, double yaw, double thr);

  /* Gear command [0,1] (1=down; gear/gear-cmd-norm — the loaded model's own flight_control channel
   * drives a kinematic transit through this same property), flap and speedbrake commands [0,1]
   * (fcs/flap-cmd-norm, fcs/speedbrake-cmd-norm). */
  void SetGear(double cmd);
  void SetFlap(double cmd);
  void SetSpeedbrake(double cmd);

  /* Independent per-side wheel brakes [0,1] (fcs/left-brake-cmd-norm / fcs/right-brake-cmd-norm) and
   * nosewheel steering [-1,1] (fcs/steer-cmd-norm, tied generically in FGGroundReactions; a particular
   * model's own scheduled-gain block maps it to whatever deflection its aircraft.xml declares — this
   * class only asserts the pilot's command). */
  void SetWheelBrakes(double left, double right);
  void SetNosewheelSteer(double cmd);

  /* Propulsion-wide starter_cmd/cutoff_cmd (FGPropulsion::SetStarter/SetCutoff default to ALL engines). */
  void EngineStart();
  void EngineCutoff();

  /* Fuel — FGPropulsion's own tank inventory (generic: enumerates by tank index, never assumes how many
   * an aircraft.xml declares). A mission's `set fuel_lbs`/`set fuel_pct` line (FBModule::ApplySetup)
   * writes here; running dry starves the engine NATIVELY inside JSBSim's own FGEngine model — this class
   * never simulates fuel exhaustion itself, it only reads/writes the tanks JSBSim already models. The
   * total/percent setters distribute proportional to EACH tank's own capacity share (how a real fuel-load
   * figure fills the jet), not first-tank-first. */
  void SetFuelTankLbs(int idx, double lbs);
  void SetFuelTotalLbs(double lbs);
  void SetFuelPct(double pct);          /* 0..100 of total declared capacity */

  /* Real ground elevation under the aircraft (m ASL) so gear/contact use the App's elevation hook
   * (FBElevationProvider) rather than a flat default. World truth, fed by whoever owns the world. */
  void SetGroundElevM(double m);

  /* ---- Readbacks: const, so a `const FBFdm&` is a read-only handle (class banner). ---- */

  /* The ground elevation (m ASL) the FDM is CURRENTLY colliding against — lets a caller prove the DEM
   * value actually reached JSBSim, not just that the setter was called. */
  double GetGroundElevM() const;

  /* Model ground clearance (m): CG height above ground when the lowest active contact touches, level.
   * gearDown=true -> wheels on struts; false -> only fixed structure (belly). Per model + gear state, so
   * takeoff/touchdown/crash and the camera eye height are geometry-true rather than a fixed number. */
  double GetGroundClearanceM(bool gearDown) const;

  double GetGearPos() const;            /* 0=up .. 1=down, kinematic-lagged (gear/gear-pos-norm) */
  double GetSpeedbrakePos() const;      /* 0..1, lagged (fcs/speedbrake-pos-norm) */

  /* Model-wide weight-on-wheels (gear/wow — true iff ANY bogey is compressed, FGGroundReactions::GetWOW).
   * A per-gear breakdown belongs to a future landing-gear system, not to this seam. */
  bool GetWow() const;

  /* True iff any declared STRUCTURE-type ground-reaction contact (JSBSim's ctSTRUCTURE — a non-wheeled
   * airframe-geometry point an aircraft.xml may declare beyond its wheeled BOGEY gear) is compressed.
   * Generic over the loaded model: enumerates contact TYPE, never a hardcoded index or aircraft name, so
   * a model with no STRUCTURE points simply always reads false. core/FBFlightMonitor's structural-contact
   * K.O. consumes this. */
  bool GetStructureContact() const;

  /* Peak strut compression force (lbf, unsigned) across every wheeled BOGEY gear this tick
   * (FGLGear::GetCompForce) — the model's own spring/damper reaction, not a derived sink-rate heuristic.
   * FBFlightMonitor compares it against GetWeightLbs() to judge a hard landing from physics alone. */
  double GetMaxGearForceLbs() const;

  /* Current gross weight, lb (inertia/weight-lbs, generic FGMassBalance tie) — FBPilot's rotation-speed
   * table is indexed by weight, not a fixed number. */
  double GetWeightLbs() const;

  /* propulsion/engine[N]/set-running for one engine index. */
  bool GetEngineRunning(int engineIndex) const;

  int    GetFuelTankCount() const;
  double GetFuelTankLbs(int idx) const;
  double GetFuelTotalLbs() const;       /* the sum FBFdmTelemetrySource's fuelLbs column reports */
  double GetFuelCapacityLbs() const;    /* sum of every tank's own declared capacity */

private:
  friend class FBFdmBoot;   /* the ONE producer of a loaded instance (fdm/FBFdmBoot.h) */
  FBFdm();
  bool Load(const FBFdmSpawn &spawn);

  struct Impl;
  std::unique_ptr<Impl> P;
};

} // namespace FlightBox
#endif
