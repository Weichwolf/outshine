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

enum class FBUnitKind { Aircraft };

struct FBUnitPose {
  double LatDeg = 0.0, LonDeg = 0.0, ElevM = 0.0;   /* geodetic, m ASL */
  double RollDeg = 0.0, PitchDeg = 0.0, YawDeg = 0.0;
  double SpeedMs = 0.0;      /* true airspeed/ground speed as the unit type defines it */
  double HeadingDeg = 0.0;   /* ground track, true, deg 0..360 */
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

  /* Per-frame update at whatever rate the owner drives it; NoOp default (a unit whose motion is driven
   * entirely by something else has nothing to do here — FBSimUnit forwards it to the module, which
   * cycles the FDM and its own systems). */
  virtual void Run(double dt, const FBWorld *world) { (void)dt; (void)world; }

private:
  int Id;
  std::string Name;
  FBUnitKind Kind;
  FBUnitTeam Team;
};

} // namespace FlightBox
#endif
