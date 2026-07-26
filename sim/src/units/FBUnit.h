/* FlightBox — FBUnit: the base interface every world entity shares, controllable or not. The pilot
 * (systems/FBPilot) and any future sensor/weapon system query units through THIS interface, never the
 * concrete type — Ownship today, AI aircraft/ground units later, same shape, so nothing above this
 * layer has to know whose aircraft it's looking at. Identity (Id/Kind/Team) is set once at
 * construction; Pose is read fresh each query (a Unit is a VIEW onto whatever owns the ground truth,
 * see FBSimUnit's banner — never a duplicated copy that can drift out of sync). */
#ifndef FBUNIT_H
#define FBUNIT_H

#include <string>
#include "FBTeam.h"

namespace FlightBox {

class FBWorld;
class FBUnitRegistry;

/* Weapon = a store in free flight after release: its own FDM, its own module, stepped and judged like
 * any other unit (units/FBSimUnit) — the KIND exists because two things about it differ and both are
 * the owner's business, not the unit's: its physical K.O. is a detonation rather than the end of the
 * run, and it is not something another unit's air-to-air sensors are looking for. */
enum class FBUnitKind { Aircraft, Weapon };

struct FBUnitPose {
  double LatDeg = 0.0, LonDeg = 0.0, ElevM = 0.0;   /* geodetic, m ASL */
  double RollDeg = 0.0, PitchDeg = 0.0, YawDeg = 0.0;
  double SpeedMs = 0.0;      /* true airspeed/ground speed as the unit type defines it */
  double HeadingDeg = 0.0;   /* ground track, true, deg 0..360 */
};

/* What this unit RADIATES — the part of its system state another unit's sensors may legitimately
 * notice. A cooperative datalink track exists because the sender's terminal is transmitting, so that
 * switch is not private to the sender's module: it is observable, and therefore published at the same
 * barrier as the pose (no receiver ever reads a transmitter state mid-tick). Emitters that only make
 * sense once the matching sensor exists (radar illumination, jammer, IFF replies) join here. */
struct FBUnitSignature {
  bool DatalinkXmt = false;   /* MIDS terminal powered AND transmitting (XMT ON) */
  /* IFF transponder answering (AN/APX-113, doc/f16/datalink-iff.md). Published for the same reason
   * DatalinkXmt is: a reply is a RADIATED signal, so whether this aircraft answers a Mode-4 challenge is
   * observable by another unit's interrogator (systems/FBRadarSystem) and not private to its module. */
  bool IffXpdr = false;
};

class FBUnit {
public:
  FBUnit(int id, std::string name, FBUnitKind kind, FBUnitTeam team)
      : Id(id), Name(std::move(name)), Kind(kind), Team(team) {}
  virtual ~FBUnit() = default;

  int GetId() const { return Id; }
  const std::string &GetName() const { return Name; }   /* callsign — the .fbm `unit <id>` token */
  FBUnitKind GetKind() const { return Kind; }
  FBUnitTeam GetTeam() const { return Team; }

  /* SNAPSHOT CONTRACT (multi-unit): what this returns is the pose of the LAST COMPLETED tick, never a
   * half-integrated one. The client steps every unit first and only then publishes the new poses (the
   * barrier in FBMissionRunner.cpp / the WASM frame loop), so a unit reading another unit through the
   * FBWorld registry always sees a consistent world state and never depends on tick ORDER — which is
   * exactly what makes the planned per-unit threading (CLAUDE.md "Ausblick Multi-Unit") a pure
   * parallelisation instead of a redesign. */
  virtual FBUnitPose GetPose() const = 0;

  /* The published emission signature, under the SAME snapshot contract as GetPose (see above): a unit
   * that switched its transmitter off during this tick still reads as it was at the last barrier. A
   * unit type with no emitters keeps the silent default. */
  virtual FBUnitSignature GetSignature() const { return {}; }

  /* Per-frame update at whatever rate the owner drives it; NoOp default (a unit whose motion is driven
   * entirely by something else has nothing to do here — FBSimUnit forwards it to the module, which
   * cycles the FDM and its own systems). `units` is the cast of the world as simulated SENSORS may
   * observe it (units/FBUnitRegistry — every entry a last-completed-tick snapshot, including this unit
   * itself), `world` the terrain/streaming side a sensor may need on top of it; both are borrowed and
   * either may be null in a client that has none. */
  virtual void Run(double dt, const FBUnitRegistry *units, const FBWorld *world) {
    (void)dt; (void)units; (void)world;
  }

private:
  int Id;
  std::string Name;
  FBUnitKind Kind;
  FBUnitTeam Team;
};

} // namespace FlightBox
#endif
