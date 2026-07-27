/* FlightBox — FBUnit: the base interface every world entity shares. Identity is set once at
 * construction; a Unit is a VIEW onto whatever owns the ground truth, never a copy that can drift.
 * Details: doc/flightbox/units-and-missions.md */
#ifndef FBUNIT_H
#define FBUNIT_H

#include <string>
#include "FBCountermeasure.h"
#include "FBEmitter.h"
#include "FBTeam.h"
#include "FBWeaponUplink.h"

namespace FlightBox::World { class FBWorld; }

namespace FlightBox::Units {

class FBUnitRegistry;

/* A kind exists only where the OWNER must treat the unit differently — a Weapon's physical K.O. is a
 * detonation and not the end of the run, a Ground unit has no airframe to judge, and neither is
 * something another unit's AIR-to-air sensors look for. Everywhere else all three are full units. */
enum class FBUnitKind { Aircraft, Weapon, Ground };

struct FBUnitPose {
  double LatDeg = 0.0, LonDeg = 0.0, ElevM = 0.0;   /* geodetic, m ASL */
  double RollDeg = 0.0, PitchDeg = 0.0, YawDeg = 0.0;
  double SpeedMs = 0.0;      /* true airspeed/ground speed as the unit type defines it */
  double HeadingDeg = 0.0;   /* ground track, true, deg 0..360 */
};

/* What this unit RADIATES — the part of its system state another unit's sensors may legitimately
 * notice, published at the same barrier as the pose so no receiver ever reads half of it. */
struct FBUnitSignature {
  bool DatalinkXmt = false;   /* MIDS terminal powered AND transmitting (XMT ON) */
  FBWeaponUplink Uplink;      /* midcourse guidance to a weapon this unit launched */
  bool IffXpdr = false;       /* AN/APX-113 answering Mode 4 */
  FBEmitterSignature Radar;   /* the BEAM incl. where it points; silent Mode::None by default */
  /* Chaff hangs off the DISPENSING unit rather than being units of its own — stated consequence: a
   * cloud can only decoy a radar looking at the aircraft that threw it. */
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

  /* SNAPSHOT CONTRACT: the state of the LAST COMPLETED tick, never a half-integrated one — the client
   * steps every unit and only then publishes, so no result can depend on tick ORDER. */
  virtual FBUnitPose GetPose() const = 0;
  virtual FBUnitSignature GetSignature() const { return {}; }

  /* `units` is the cast as simulated SENSORS may observe it, `world` the terrain side on top; both
   * borrowed, either may be null in a client that has none. */
  virtual void Run(double dt, const FBUnitRegistry *units, const World::FBWorld *world) {
    (void)dt; (void)units; (void)world;
  }

private:
  int Id;
  std::string Name;
  FBUnitKind Kind;
  FBUnitTeam Team;
};

} // namespace FlightBox::Units
#endif
