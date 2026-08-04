/* FlightBox — FBUnit: the base interface every world entity shares. Identity is set once at
 * construction; a Unit is a VIEW onto whatever owns the ground truth, never a copy that can drift.
 * Details: doc/units-and-missions.md */
#ifndef FBUNIT_H
#define FBUNIT_H

#include <string>
#include "FBCountermeasure.h"
#include "FBEmitter.h"
#include "FBFlight.h"
#include "FBNetReport.h"
#include "FBTeam.h"
#include "FBVisualContact.h"
#include "FBStore.h"
#include "FBWeaponUplink.h"

namespace FlightBox::World { class FBWorld; }

namespace FlightBox::Units {

class FBUnitRegistry;

/* A kind exists only where the OWNER must treat the unit differently — a Weapon's physical K.O. is a
 * detonation and not the end of the run, a Ground unit has no airframe to judge, and neither is
 * something another unit's AIR-to-air sensors look for. Everywhere else all three are full units. */
enum class FBUnitKind { Aircraft, Weapon, Ground };

/* WHERE THIS UNIT'S MOVING PARTS STAND — the pose of the airframe's surfaces, published on the same
 * barrier as the pose of the airframe itself and for the same reason. It rides in FBUnitPose because
 * that is what it IS: geometry, not a signature. A sensor slot already receives exact geodetic ground
 * truth here and is trusted to degrade it; a deflected aileron adds no capability that lat/lon did not
 * already hand over, so there is no second gate to build.
 * Angles are the FDM's OWN surface positions (fdm/FBFdm.h), never a stick command: the FCS schedules
 * and rate-limits, so drawing the command would draw a second aeroplane. Every field is 0 for a unit
 * whose model declares nothing of the kind. */
struct FBUnitArticulation {
  float AileronLRad = 0.0f, AileronRRad = 0.0f;
  float ElevonLRad = 0.0f, ElevonRRad = 0.0f;   /* differential horizontal tail */
  float RudderRad = 0.0f;
  float LefDeg = 0.0f;
  float SpeedbrakeDeg = 0.0f;
  float GearNorm = 0.0f;      /* 0 up .. 1 down, kinematic-lagged */
  float HookNorm = 0.0f;
  float CanopyNorm = 0.0f;    /* 0 closed .. 1 open */
};

struct FBUnitPose {
  double LatDeg = 0.0, LonDeg = 0.0, ElevM = 0.0;   /* geodetic, m ASL */
  double RollDeg = 0.0, PitchDeg = 0.0, YawDeg = 0.0;
  double SpeedMs = 0.0;      /* true airspeed/ground speed as the unit type defines it */
  double HeadingDeg = 0.0;   /* ground track, true, deg 0..360 */
  /* THE SURFACE UNDER THIS UNIT, from the owner's own elevation hook — terrain, not identity. It is
   * here because a radio horizon is measured from the antenna's height above the reflecting SURFACE,
   * and ElevM alone cannot say what that is: two positions at 936 m ASL are not 252 km apart in line of
   * sight, they are on the same hillside. doc/air-defence-network.md §Gaps collision 1. */
  double GroundAslM = 0.0;
  FBUnitArticulation Art;
};

/* WHAT AN EYE COULD SEE OF THIS UNIT, published like the radar cross-section beside it and for the
 * identical reason: how big an aeroplane looks and what kind of aeroplane it is are properties of the
 * OBSERVED unit, and a sensor carrying a table of them would be reading the registry's identity by the
 * back door. The three numbers are the largest dimension of the silhouette in each of the three
 * orthogonal views, metres — the target module's damage layout read as geometry, not a second table.
 *
 * `TypeName` is the module's FBModuleRegistry key and NOTHING else: no callsign, no unit id, no team.
 * It is public mission data (the `module` line spells it), which is why publishing it hands nothing
 * over — and why only a sensor that has EARNED it by angular resolution may copy it into a contact
 * (doc/sensors.md §9.7). All zero / empty = nothing to see: a released store, a ground target, any
 * module with no damage layout. */
struct FBVisualSignature {
  float FrontalM = 0.0f;   /* head-on / from astern: the wingspan of a conventional aeroplane */
  float LateralM = 0.0f;   /* from the side: its length */
  float PlanM = 0.0f;      /* from directly above or below */
  char  TypeName[kVisualTypeNameLen] = {};
};

/* The presented dimension along a line of sight given in the TARGET's body frame (+fwd/+right/+down,
 * need not be normalised). Weights are the squared direction cosines, which sum to exactly one: the
 * result is a convex combination of the three views, exact on each axis and never larger than the
 * largest of them. Deliberately NOT the |cos|+sin law FBPresentedAreaM2 uses — that one is an AREA
 * proxy and reports 1.41x the larger view at 45 deg, which for a DIMENSION would be an aeroplane
 * bigger than itself. */
inline float FBPresentedDimensionM(const FBVisualSignature &v, double fwd, double right, double down) {
  double l2 = fwd * fwd + right * right + down * down;
  if (l2 < 1e-18) return v.FrontalM;
  return (float)((fwd * fwd * v.FrontalM + right * right * v.LateralM + down * down * v.PlanM) / l2);
}

/* What this unit RADIATES — the part of its system state another unit's sensors may legitimately
 * notice, published at the same barrier as the pose so no receiver ever reads half of it. */
struct FBUnitSignature {
  bool DatalinkXmt = false;   /* MIDS terminal powered AND transmitting (XMT ON) */
  FBWeaponUplink Uplink;      /* midcourse guidance to a weapon this unit launched */
  /* THE LASER SPOT this unit is holding on a point, beside the uplink and for the same reason: it is a
   * published STATE a semi-active weapon reads, not a message anybody delivers. Inactive for every unit
   * that never released a laser-guided round. doc/air-to-ground.md §3.2. */
  FBLaserDesignation Designation;
  bool IffXpdr = false;       /* AN/APX-113 answering Mode 4 */
  /* Any installed turbine in AUGMENTED thrust. Not an emission — a plume is not transmitted, it is
   * RADIATED heat — but the same question the rest of this struct answers: what may a foreign sensor
   * legitimately notice about this unit? An infrared head may notice exactly this
   * (sensors/FBIrstSystem), and nothing else here tells it. */
  bool Afterburner = false;
  /* WHAT A RADAR GETS BACK, as one number: the radar cross-section this airframe presents. It belongs
   * here and not in the looking radar for the same reason the afterburner bit does — it is a property
   * of the OBSERVED unit, and a sensor that carried a table of who reflects how much would be reading
   * identity out of the registry by the back door. 0 = not declared (a store, a ground target): the
   * gate then behaves exactly as it did before this field existed. doc/sensors.md, Spec. */
  float RcsM2 = 0.0f;
  /* THE BEAMS incl. where each points; every entry silent Mode::None by default. Index 0 is the unit's
   * own Radar() slot and is the ONLY one an airframe ever writes; index 1 is the second antenna a
   * ground battery radiates from at the same time (core/FBEmitter.h, kMaxEmitterBeams). */
  FBEmitterSignature Radar[kMaxEmitterBeams];
  /* WHAT THIS UNIT TELLS ITS OWN FLIGHT. It rides the cooperative terminal, so it reaches nobody
   * unless `DatalinkXmt` is true and nobody outside the faction ever — the datalink is the only
   * consumer. A unit in no flight leaves it empty and is invisible to all of it. */
  FBFlightReport Flight;
  /* Chaff hangs off the DISPENSING unit rather than being units of its own — stated consequence: a
   * cloud can only decoy a radar looking at the aircraft that threw it. */
  FBChaffCloud Chaff[kMaxChaffClouds];
  /* ...and the infrared half, under exactly the same rule and with the same consequence for an
   * infrared seeker (sensors/FBIrstSystem). */
  FBFlareCloud Flare[kMaxFlareClouds];
  /* ...and what an EYE gets: a shape and a kind, nothing that names this unit. */
  FBVisualSignature Visual;
  /* WHAT THIS UNIT PUTS ON AN AIR-DEFENCE NET. It rides the cooperative terminal like FBFlightReport
   * beside it, so it reaches nobody unless `DatalinkXmt` is true; Reporting false is every unit that is
   * on no net. It carries a POINT and never an identity — core/FBNetReport.h. */
  FBNetReport Net;
  /* COMMUNICATIONS JAMMING, as a property of any unit rather than a unit type — the same decision the
   * radar cross-section beside it makes, and the reason an F-16 can stand in for a 707 without a new
   * airframe. Metres of denial radius; 0 = not a jammer. It is INAUDIBLE: no FBEmitterSignature is
   * published for it, so nothing can bear on it and there is no home-on-jam.
   * doc/air-defence-network.md §6. */
  float CommJamM = 0.0f;
};

class FBUnit {
public:
  FBUnit(int id, std::string name, FBUnitKind kind, FBUnitTeam team, FBFlightId flight = FBFlightId{})
      : Id(id), Name(std::move(name)), Kind(kind), Team(team), Flight(std::move(flight)) {}
  virtual ~FBUnit() = default;

  int GetId() const { return Id; }
  const std::string &GetName() const { return Name; }   /* callsign — the .fbm `unit <id>` token */
  FBUnitKind GetKind() const { return Kind; }
  FBUnitTeam GetTeam() const { return Team; }
  /* Identity, like the team: undeclared (Position 0) for every unit the mission does not put in a
   * flight, and every consumer treats that as "no flight" rather than "flight 0". */
  const FBFlightId &GetFlight() const { return Flight; }

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
  FBFlightId Flight;
};

} // namespace FlightBox::Units
#endif
