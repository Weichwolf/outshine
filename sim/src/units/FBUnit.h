/* FlightBox — FBUnit: the base interface every world entity shares, controllable or not. The pilot
 * (systems/FBPilot) and any future sensor/weapon system query units through THIS interface, never the
 * concrete type — Ownship today, AI aircraft/ground units later, same shape, so nothing above this
 * layer has to know whose aircraft it's looking at. Identity (Id/Kind/Team) is set once at
 * construction; Pose is read fresh each query (a Unit is a VIEW onto whatever owns the ground truth,
 * see FBOwnshipUnit's banner — never a duplicated copy that can drift out of sync). */
#ifndef FBUNIT_H
#define FBUNIT_H

namespace FlightBox {

class FBWorld;

enum class FBUnitKind { Aircraft };
enum class FBUnitTeam { Friendly, Hostile, Neutral };

struct FBUnitPose {
  double LatDeg = 0.0, LonDeg = 0.0, ElevM = 0.0;   /* geodetic, m ASL */
  double RollDeg = 0.0, PitchDeg = 0.0, YawDeg = 0.0;
  double SpeedMs = 0.0;      /* true airspeed/ground speed as the unit type defines it */
  double HeadingDeg = 0.0;   /* ground track, true, deg 0..360 */
};

class FBUnit {
public:
  FBUnit(int id, FBUnitKind kind, FBUnitTeam team) : Id(id), Kind(kind), Team(team) {}
  virtual ~FBUnit() = default;

  int GetId() const { return Id; }
  FBUnitKind GetKind() const { return Kind; }
  FBUnitTeam GetTeam() const { return Team; }

  virtual FBUnitPose GetPose() const = 0;

  /* Per-frame update at whatever rate the owner drives it; NoOp default (a unit whose motion is driven
   * entirely by something else, like Ownship's FDM, has nothing to do here — see FBOwnshipUnit). */
  virtual void Run(double dt, const FBWorld *world) { (void)dt; (void)world; }

private:
  int Id;
  FBUnitKind Kind;
  FBUnitTeam Team;
};

} // namespace FlightBox
#endif
