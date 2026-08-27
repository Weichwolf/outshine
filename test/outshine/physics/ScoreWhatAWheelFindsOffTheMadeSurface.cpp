#include <cmath>
#include <cstdio>
#include <string>

#include "Check.h"
#include "DriveTick.h"

namespace {

// A CAR THAT DRIVES ONTO GRASS DOES NOT TAKE OFF. It keeps four contacts and loses grip.
//
// Until this case was written it did take off, and the mechanism was one line:
// `Footing::Found` was set from `fabs(offset) <= edgeM` -- an offset against a corridor ribbon --
// and `Rig::Bear` skips a contact entirely when `Found` is false. A wheel over the edge was
// reported AIRBORNE, `Reading::OffTheSurface` went positive, and `DriveTick` returned on the
// spot. Touching the verge ended the run.
//
// Three things are wrong with that and only the first is obvious:
//
//   1. it is not what happens. A verge is ground.
//   2. it makes the CORRIDOR decide physics. A corridor is a hint a mind reads on the way to a
//      destination; what a wheel stands on is a property of the world, and a world where only
//      my own corridor is solid is a world with exactly one vehicle in it. Other traffic is
//      unspellable in it, and so is a kerb, a pothole or a patch of ice.
//   3. it hides the failure it should show. A drive that ends at the first verge never reports
//      HOW BADLY it left the road, because it stops before the answer exists.
//
// So the oracle here is not a number, it is a behaviour with three parts, and each is checked
// against what the world is rather than against what we built:
//
//   the wheel keeps its contact          -- ground is ground
//   its grip is the GROUND's, not the road's -- the surface under it decides, not an offset
//   the drive continues                  -- leaving the made surface is an event, not an end
//
// The negative control for all three is the ribbon restored as the source of the answer.
constexpr double kLengthM = 400.0;
constexpr double kHalfWidthM = 3.5;
constexpr double kRoadFriction = 1.0;
constexpr double kVergeFriction = 0.35;
constexpr double kAsideM = 5.0;

// Static equilibrium, so the car starts where it would settle and the first step is not a
// launch: 1500 kg over four springs of 90 000 N/m compresses each by
//   m*g / (4*k) = 1500 * 9.81 / (4 * 90000) = 0.0409 m
// and the mount sits 0.35 m below the body's origin with a contact reaching 0.35 m, so the body
// stands at 2 * 0.35 - 0.0409 = 0.659 m.
constexpr double kRideHeightM = 0.6591;

// `DriveTick` keeps its tally only past 50 m, so a case that starts at the origin measures a
// drive that never reported anything. The car starts beyond that mark.
constexpr double kStartAtM = 60.0;

class Verge final : public outshine::Sim::Underfoot {
public:
  explicit Verge(double friction) : Friction_(friction) {}

  [[nodiscard]] outshine::Sim::Underneath At(double lat, double lon) const override {
    (void)lat;
    (void)lon;
    ++Asked_;
    outshine::Sim::Underneath out;
    out.Known = true;
    out.HeightAslM = 0.0;
    out.Friction = Friction_;
    return out;
  }

  [[nodiscard]] long Asked() const { return Asked_; }

private:
  double Friction_;
  mutable long Asked_ = 0;
};

[[nodiscard]] bool Straight(outshine::Sim::Corridor &out, std::string &error) {
  outshine::Placed from{};
  from.HeadingRad = 1.5707963267948966;
  const outshine::Segment along{outshine::Curve::Straight, kLengthM, 0.0, 0.0};
  if (!out.Line.Lay(from, {along}, error)) { return false; }
  if (!out.Line.Rise({outshine::Knot{0.0, 0.0, 0.0}, outshine::Knot{kLengthM, 0.0, 0.0}},
                     error)) {
    return false;
  }
  if (!out.Line.Bank({outshine::Knot{0.0, 0.0, 0.0}, outshine::Knot{kLengthM, 0.0, 0.0}},
                     error)) {
    return false;
  }
  out.Bake(kLengthM);
  for (size_t at = 0; at < out.Fine.size(); ++at) {
    out.Fine[at].AsideM = 0.0;
    out.Fine[at].EdgeM = kHalfWidthM;
    out.Fine[at].LaneHalfM = kHalfWidthM;
    out.Fine[at].Friction = kRoadFriction;
  }
  out.SpanM = kLengthM;
  out.NarrowestLaneM = 2.0 * kHalfWidthM;
  out.BudgetM = 1.0;
  out.HoldWithinM = 1.0;
  out.AsideFriction = kVergeFriction;
  return true;
}

[[nodiscard]] outshine::Sim::Rigged Stood() {
  outshine::Sim::Rigged out;
  out.Stood = true;
  out.Axles.WheelbaseM = 2.6;
  out.Axles.SteerLimitRad = 0.55;
  out.Envelope.Grip = 1.0;
  out.Envelope.GravityMs2 = 9.81;
  out.Envelope.MassKg = 1500.0;
  out.Envelope.DriveN = 6000.0;
  out.Envelope.BrakeN = 12000.0;
  out.Envelope.DragArea = 0.7;
  out.Envelope.AirDensity = 1.225;
  out.Envelope.SettleS = 0.6;
  out.Envelope.HoldWithinM = 1.0;
  out.Envelope.CorneringNPerRad = 55000.0;
  out.TightestM = 20.0;

  outshine::Physics::Rig &rig = out.Rig;
  rig.Count = 4;
  const double halfTrackM = 0.8;
  for (size_t which = 0; which < 4; ++which) {
    outshine::Physics::Mount &mount = rig.Mounts[which];
    mount.AtM[0] = which % 2 == 0 ? -halfTrackM : halfTrackM;
    mount.AtM[1] = -0.35;
    mount.AtM[2] = which < 2 ? -1.3 : 1.3;
    mount.Strut.ReachM = 0.35;
    mount.Strut.StiffnessNPerM = 90000.0;
    mount.Strut.DampingNsPerM = 5000.0;
    mount.Strut.TravelM = 0.15;
    mount.Sheds.CorneringNPerRad = 55000.0;
    mount.Sheds.Grip = 1.0;
    mount.Sheds.FrictionAtLoadN = 3900.0;
    mount.Sheds.LoadFalloff = 0.15;
    mount.Sheds.RelaxationM = 0.4;
    mount.Steering.Applied.Ratio = which < 2 ? 1.0 : 0.0;
    mount.Spin.Applied.Ratio = which < 2 ? 0.0 : 0.5;
    mount.Spin.Resisting.Ratio = 0.25;
  }
  return out;
}

struct Rode {
  long calls = 0;
  long asked = 0;
  long answered = 0;
  size_t airborne = 0;
  double worstRatio = 0.0;
  bool offTheRoad = false;
  bool ended = false;
  double reachedM = 0.0;
  double leastClearanceM = 0.0;
  double bodyUpM = 0.0;
};

[[nodiscard]] Rode Over(const outshine::Sim::Corridor &way, const Verge &verge, double asideM) {
  outshine::Sim::Rigged stood = Stood();
  outshine::Sim::DriveState drive;
  drive.Rig = stood.Rig;
  drive.CarWidthM = 1.8;
  drive.NearM = kStartAtM;
  drive.Body.MassKg = stood.Envelope.MassKg;
  for (int axis = 0; axis < 3; ++axis) { drive.Body.InertiaKgM2[axis] = 2000.0; }
  drive.Body.PositionM[0] = asideM;
  drive.Body.PositionM[1] = kRideHeightM;
  drive.Body.PositionM[2] = -kStartAtM;
  drive.Body.OrientationQ[0] = 1.0;
  drive.Body.VelocityMs[2] = -12.0;

  Rode out;
  for (int step = 0; step < 200; ++step) {
    const outshine::Sim::Ridden &rode =
        outshine::Sim::DriveTick(way, stood, verge, drive, 0.01, nullptr);
    out.asked = rode.GroundAsked;
    out.answered = rode.GroundAnswered;
    out.airborne = rode.MostAirborne > out.airborne ? rode.MostAirborne : out.airborne;
    out.worstRatio = rode.WorstRatio > out.worstRatio ? rode.WorstRatio : out.worstRatio;
    out.offTheRoad = out.offTheRoad || rode.OffTheRoad;
    out.reachedM = rode.ReachedM;
    out.leastClearanceM = rode.LeastClearanceM;
    out.bodyUpM = drive.Body.PositionM[1];
    if (!rode.Found || rode.Lost || rode.Arrived) {
      out.ended = true;
      break;
    }
  }
  out.calls = verge.Asked();
  return out;
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  outshine::Sim::Corridor way;
  std::string error;
  if (!Straight(way, error)) {
    Unprepared(("the straight corridor did not lay: " + error).c_str());
    return Report();
  }

  const Verge grass(kVergeFriction);
  const Rode aside = Over(way, grass, kAsideM);

  const Verge unused(kVergeFriction);
  const Rode inLane = Over(way, unused, 0.0);

  std::printf("IN LANE   asked %ld  answered %ld  airborne %zu  off %d  reached %.2f m\n",
              inLane.asked, inLane.answered, inLane.airborne, inLane.offTheRoad ? 1 : 0,
              inLane.reachedM);
  std::printf("          least clearance %.4f m   body up %.4f m\n", inLane.leastClearanceM,
              inLane.bodyUpM);
  std::printf("ON VERGE  asked %ld  answered %ld  airborne %zu  off %d  reached %.2f m\n",
              aside.asked, aside.answered, aside.airborne, aside.offTheRoad ? 1 : 0,
              aside.reachedM);

  CHECK(inLane.asked == 0,
        "a car inside the made surface asks the ground nothing: the corridor it was given IS the "
        "surface there, and a query per wheel per step for an answer already in hand is work on "
        "the frame path that buys nothing");
  std::printf("ON VERGE  the tick REPORTED %ld ask(s) and MADE %ld call(s)\n", aside.asked,
              aside.calls);
  CHECK(aside.calls == aside.asked,
        "**ONE QUERY PER WHEEL PER STEP, AND THE TICK REPORTS EVERY ONE IT MAKES**: the query "
        "used to answer a height and the caller assembled the normal from TWO more at the post "
        "spacing, so a step made three calls and published one. Each of the three could block, "
        "and the counter said nothing. The query answers height, normal and friction together "
        "now, from one lookup of the tile it already found");
  CHECK(aside.asked > 0 && aside.answered == aside.asked,
        "**A WHEEL PAST THE EDGE ASKS THE GROUND**, and on this world every ask is answered, so "
        "the number below is the ground's word and not a fallback nobody noticed");
  CHECK(aside.airborne < 4,
        "**IT DOES NOT TAKE OFF**: a car standing 5 m from a centreline whose made surface "
        "reaches 3.5 m has wheels over the verge, and a verge is GROUND. Until this stood, "
        "`Footing::Found` was an offset against a ribbon and every one of those wheels was "
        "reported airborne");
  CHECK(aside.reachedM > kStartAtM + 5.0,
        "and the drive CARRIES ON past where it left the made surface -- at least five metres of "
        "it, not the one step it takes to notice. Leaving the made surface is an EVENT the tally "
        "records, not a verdict the tick returns on: a run that stops at the first verge can "
        "never report how badly it left the road, because it stops before the answer exists");
  CHECK(aside.offTheRoad,
        "the event is still recorded, so nothing was lost by not ending: the tally says the car "
        "left the made surface even though the physics carried on");

  const Verge ice(0.08);
  const Rode onIce = Over(way, ice, kAsideM);
  std::printf("ON ICE    worst ratio %.4f   on grass %.4f\n", onIce.worstRatio, aside.worstRatio);
  CHECK(onIce.worstRatio > aside.worstRatio,
        "**THE GROUND DECIDES, NOT THE OFFSET**: the same car at the same place over a surface "
        "gripping 0.08 works its tyres harder than over one gripping 0.35, so what is under the "
        "wheel is what changed the physics -- an offset against a ribbon cannot tell the two "
        "apart because it is the same offset");

  Covers("sim: a wheel past the made surface asks the ground what is under it, keeps its "
         "contact, grips by that ground and not by a corridor, and leaving the made surface is "
         "an event the drive records rather than an end it returns on");
  return Report();
}
