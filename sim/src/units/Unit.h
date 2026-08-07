/* outshine — Unit: the base interface every world entity shares. Identity is set once at
 * construction; a Unit is a VIEW onto whatever owns the ground truth, never a copy that can drift.
 * Details: doc/units-and-missions.md */
#ifndef UNIT_H
#define UNIT_H

#include <cstdint>
#include <string>
#include "Countermeasure.h"
#include "Emitter.h"
#include "Flight.h"
#include "NetReport.h"
#include "Team.h"
#include "VisualContact.h"
#include "Store.h"
#include "WeaponUplink.h"

namespace outshine::World { class World; }

namespace outshine::Units {

class UnitRegistry;

/* A kind exists only where the OWNER must treat the unit differently — a Weapon's physical K.O. is a
 * detonation and not the end of the run, a Ground unit has no airframe to judge, and neither is
 * something another unit's AIR-to-air sensors look for. Everywhere else all three are full units. */
enum class UnitKind { Aircraft, Weapon, Ground };

/* WHERE THIS UNIT'S MOVING PARTS STAND — the pose of the airframe's surfaces, published on the same
 * barrier as the pose of the airframe itself and for the same reason. It rides in UnitPose because
 * that is what it IS: geometry, not a signature. A sensor slot already receives exact geodetic ground
 * truth here and is trusted to degrade it; a deflected aileron adds no capability that lat/lon did not
 * already hand over, so there is no second gate to build.
 * Angles are the FDM's OWN surface positions (fdm/Fdm.h), never a stick command: the FCS schedules
 * and rate-limits, so drawing the command would draw a second aeroplane. Every field is 0 for a unit
 * whose model declares nothing of the kind. */
struct UnitArticulation {
  float AileronLRad = 0.0f, AileronRRad = 0.0f;
  float ElevonLRad = 0.0f, ElevonRRad = 0.0f;   /* differential horizontal tail */
  float RudderRad = 0.0f;
  float LefDeg = 0.0f;
  float SpeedbrakeDeg = 0.0f;
  float GearNorm = 0.0f;      /* 0 up .. 1 down, kinematic-lagged */
  float HookNorm = 0.0f;
  float CanopyNorm = 0.0f;    /* 0 closed .. 1 open */
};

struct UnitPose {
  double LatDeg = 0.0, LonDeg = 0.0, ElevM = 0.0;   /* geodetic, m ASL */
  double RollDeg = 0.0, PitchDeg = 0.0, YawDeg = 0.0;
  double SpeedMs = 0.0;      /* true airspeed/ground speed as the unit type defines it */
  double HeadingDeg = 0.0;   /* ground track, true, deg 0..360 */
  /* THE SURFACE UNDER THIS UNIT, from the owner's own elevation hook — terrain, not identity. It is
   * here because a radio horizon is measured from the antenna's height above the reflecting SURFACE,
   * and ElevM alone cannot say what that is: two positions at 936 m ASL are not 252 km apart in line of
   * sight, they are on the same hillside. doc/air-defence-network.md §Gaps collision 1. */
  double GroundAslM = 0.0;
  UnitArticulation Art;
};

/* WHAT AN EYE COULD SEE OF THIS UNIT, published like the radar cross-section beside it and for the
 * identical reason: how big an aeroplane looks and what kind of aeroplane it is are properties of the
 * OBSERVED unit, and a sensor carrying a table of them would be reading the registry's identity by the
 * back door. The three numbers are the largest dimension of the silhouette in each of the three
 * orthogonal views, metres — the target module's damage layout read as geometry, not a second table.
 *
 * `TypeName` is the module's ModuleRegistry key and NOTHING else: no callsign, no unit id, no team.
 * It is public mission data (the `module` line spells it), which is why publishing it hands nothing
 * over — and why only a sensor that has EARNED it by angular resolution may copy it into a contact
 * (doc/sensors.md §9.7). All zero / empty = nothing to see: a released store, a ground target, any
 * module with no damage layout. */
struct VisualSignature {
  float FrontalM = 0.0f;   /* head-on / from astern: the wingspan of a conventional aeroplane */
  float LateralM = 0.0f;   /* from the side: its length */
  float PlanM = 0.0f;      /* from directly above or below */
  char  TypeName[kVisualTypeNameLen] = {};
};

/* The presented dimension along a line of sight given in the TARGET's body frame (+fwd/+right/+down,
 * need not be normalised). Weights are the squared direction cosines, which sum to exactly one: the
 * result is a convex combination of the three views, exact on each axis and never larger than the
 * largest of them. Deliberately NOT the |cos|+sin law PresentedAreaM2 uses — that one is an AREA
 * proxy and reports 1.41x the larger view at 45 deg, which for a DIMENSION would be an aeroplane
 * bigger than itself. */
inline float PresentedDimensionM(const VisualSignature &v, double fwd, double right, double down) {
  double l2 = fwd * fwd + right * right + down * down;
  if (l2 < 1e-18) return v.FrontalM;
  return (float)((fwd * fwd * v.FrontalM + right * right * v.LateralM + down * down * v.PlanM) / l2);
}

/* WHAT A HIT DID TO THIS UNIT, as far as it is RADIATED: fire, wreckage and the smoke off a holed
 * airframe are not a reading of the damage register, they are what the register produces in the air
 * around the aircraft — so they belong beside the plume and the cross-section rather than in a second
 * channel. Copied verbatim from core/SystemHealth at the publish barrier, which is monotone and has
 * exactly one writer: an effect built on this can only ever be READING the simulation, and nothing
 * here is reachable in the direction that would let it write back. */
struct DamageSignature {
  uint16_t Hits = 0;             /* monotone burst count — a RISE is one detonation, nothing else is */
  bool CombatEffective = true;   /* false = the airframe cannot finish the sortie: it burns */
  bool Destroyed = false;        /* physics finished it (core/FlightMonitor -> ApplyPhysicalKo) */
};

/* What this unit RADIATES — the part of its system state another unit's sensors may legitimately
 * notice, published at the same barrier as the pose so no receiver ever reads half of it. */
struct UnitSignature {
  bool DatalinkXmt = false;   /* MIDS terminal powered AND transmitting (XMT ON) */
  WeaponUplink Uplink;      /* midcourse guidance to a weapon this unit launched */
  /* THE LASER SPOT this unit is holding on a point, beside the uplink and for the same reason: it is a
   * published STATE a semi-active weapon reads, not a message anybody delivers. Inactive for every unit
   * that never released a laser-guided round. doc/air-to-ground.md §3.2. */
  LaserDesignation Designation;
  bool IffXpdr = false;       /* the transponder is answering Mode 4 */
  /* Any installed turbine in AUGMENTED thrust. Not an emission — a plume is not transmitted, it is
   * RADIATED heat — but the same question the rest of this struct answers: what may a foreign sensor
   * legitimately notice about this unit? An infrared head may notice exactly this
   * (sensors/IrstSystem), and nothing else here tells it. */
  bool Afterburner = false;
  /* WHAT A RADAR GETS BACK, as one number: the radar cross-section this airframe presents. It belongs
   * here and not in the looking radar for the same reason the afterburner bit does — it is a property
   * of the OBSERVED unit, and a sensor that carried a table of who reflects how much would be reading
   * identity out of the registry by the back door. 0 = not declared (a store, a ground target): the
   * gate then behaves exactly as it did before this field existed. doc/sensors.md, Spec. */
  float RcsM2 = 0.0f;
  /* THE BEAMS incl. where each points; every entry silent Mode::None by default. Index 0 is the unit's
   * own Radar() slot and is the ONLY one an airframe ever writes; index 1 is the second antenna a
   * ground battery radiates from at the same time (core/Emitter.h, kMaxEmitterBeams). */
  EmitterSignature Radar[kMaxEmitterBeams];
  /* WHAT THIS UNIT TELLS ITS OWN FLIGHT. It rides the cooperative terminal, so it reaches nobody
   * unless `DatalinkXmt` is true and nobody outside the faction ever — the datalink is the only
   * consumer. A unit in no flight leaves it empty and is invisible to all of it. */
  FlightReport Flight;
  /* Chaff hangs off the DISPENSING unit rather than being units of its own — stated consequence: a
   * cloud can only decoy a radar looking at the aircraft that threw it. */
  ChaffCloud Chaff[kMaxChaffClouds];
  /* ...and the infrared half, under exactly the same rule and with the same consequence for an
   * infrared seeker (sensors/IrstSystem). */
  FlareCloud Flare[kMaxFlareClouds];
  /* ...and what an EYE gets: a shape and a kind, nothing that names this unit. */
  VisualSignature Visual;
  DamageSignature Damage;
  /* WHAT THIS UNIT PUTS ON AN AIR-DEFENCE NET. It rides the cooperative terminal like FlightReport
   * beside it, so it reaches nobody unless `DatalinkXmt` is true; Reporting false is every unit that is
   * on no net. It carries a POINT and never an identity — core/NetReport.h. */
  NetReport Net;
  /* COMMUNICATIONS JAMMING, as a property of any unit rather than a unit type — the same decision the
   * radar cross-section beside it makes. Metres of denial radius; 0 = not a jammer. It is INAUDIBLE: no
   * EmitterSignature is published for it, so nothing can bear on it and there is no home-on-jam. */
  float CommJamM = 0.0f;
};

class Unit {
public:
  Unit(int id, std::string name, UnitKind kind, UnitTeam team, FlightId flight = FlightId{})
      : Id(id), Name(std::move(name)), Kind(kind), Team(team), Flight(std::move(flight)) {}
  virtual ~Unit() = default;

  int GetId() const { return Id; }
  const std::string &GetName() const { return Name; }   /* callsign — the .fbm `unit <id>` token */
  UnitKind GetKind() const { return Kind; }
  UnitTeam GetTeam() const { return Team; }
  /* Identity, like the team: undeclared (Position 0) for every unit the mission does not put in a
   * flight, and every consumer treats that as "no flight" rather than "flight 0". */
  const FlightId &GetFlight() const { return Flight; }

  /* SNAPSHOT CONTRACT: the state of the LAST COMPLETED tick, never a half-integrated one — the client
   * steps every unit and only then publishes, so no result can depend on tick ORDER. */
  virtual UnitPose GetPose() const = 0;
  virtual UnitSignature GetSignature() const { return {}; }

  /* `units` is the cast as simulated SENSORS may observe it, `world` the terrain side on top; both
   * borrowed, either may be null in a client that has none. */
  virtual void Run(double dt, const UnitRegistry *units, const World::World *world) {
    (void)dt; (void)units; (void)world;
  }

private:
  int Id;
  std::string Name;
  UnitKind Kind;
  UnitTeam Team;
  FlightId Flight;
};

} // namespace outshine::Units
#endif
