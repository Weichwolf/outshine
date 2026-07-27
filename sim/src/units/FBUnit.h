/* FlightBox — FBUnit: the base interface every world entity shares, controllable or not. The pilot
 * (systems/FBPilot) and any future sensor/weapon system query units through THIS interface, never the
 * concrete type — Ownship today, AI aircraft/ground units later, same shape, so nothing above this
 * layer has to know whose aircraft it's looking at. Identity (Id/Kind/Team) is set once at
 * construction; Pose is read fresh each query (a Unit is a VIEW onto whatever owns the ground truth,
 * see FBSimUnit's banner — never a duplicated copy that can drift out of sync). */
#ifndef FBUNIT_H
#define FBUNIT_H

#include <string>
#include "FBCountermeasure.h"
#include "FBEmitter.h"
#include "FBTeam.h"
#include "FBWeaponUplink.h"

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
  /* The midcourse guidance uplink this unit is radiating to a weapon it launched (core/FBWeaponUplink.h,
   * doc/f16/weapons.md §2.5's datalink->active handover). Published here for the same reason the two
   * switches below are: a guidance transmission is an emission, so the missile receives it through its
   * own comms slot instead of being handed its shooter's private state. Inactive on any unit that is
   * not supporting a shot, which is every unit most of the time. */
  FBWeaponUplink Uplink;
  /* IFF transponder answering (AN/APX-113, doc/f16/datalink-iff.md). Published for the same reason
   * DatalinkXmt is: a reply is a RADIATED signal, so whether this aircraft answers a Mode-4 challenge is
   * observable by another unit's interrogator (systems/FBRadarSystem) and not private to its module. */
  bool IffXpdr = false;
  /* THE RADAR BEAM (core/FBEmitter.h): what this unit's set is radiating, in which mode, and — the part
   * that makes it a geometry problem rather than a flag — WHERE the beam is pointed. The loudest thing
   * a fighter emits, and the one another aircraft's warning receiver (systems/FBRwrSystem) is built to
   * hear. Silent by default: an aircraft with its set off publishes Mode::None, like every unit that
   * carries no radar at all. */
  FBEmitterSignature Radar;
  /* THE CHAFF THIS UNIT HAS THROWN (core/FBCountermeasure.h) — an emission in everything but the
   * direction of causality: it is not radiated, it REFLECTS, but it is the same kind of fact about this
   * unit that another unit's sensor may legitimately notice, published at the same barrier so no radar
   * ever sees half a salvo. Hanging the clouds off the DISPENSING unit rather than making each one a
   * unit of its own is a deliberate scope decision with a stated consequence: a cloud can only decoy a
   * radar that is looking at the aircraft that threw it, never one tracking somebody else nearby. */
  FBChaffCloud Chaff[kMaxChaffClouds];
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
