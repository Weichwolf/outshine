#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include "Carriageway.h"
#include "Drive.h"
#include "Rig.h"

using outshine::Curve;
using outshine::Envelope;
using outshine::Knot;
using outshine::Placed;
using outshine::ReferenceLine;
using outshine::Segment;
using outshine::SpeedProfile;
using outshine::Stand;
using outshine::Standing;
using outshine::Physics::Bear;
using outshine::Physics::Body;
using outshine::Physics::Controls;
using outshine::Physics::Fall;
using outshine::Physics::Footing;
using outshine::Physics::Mount;
using outshine::Physics::Place;
using outshine::Physics::Reading;
using outshine::Physics::Resist;
using outshine::Physics::Rig;
using outshine::Physics::Step;
using outshine::Physics::Turn;
using outshine::Physics::Wrench;
using outshine::Pilot::Axles;
using outshine::Pilot::Demand;
using outshine::Pilot::Drive;
using outshine::Pilot::Hold;
using outshine::Pilot::Locate;
using outshine::Pilot::Placement;
using outshine::Pilot::ReachOf;
using outshine::Pilot::Reins;
using outshine::Pilot::Steering;
using outshine::Pilot::TightestPerM;

namespace {

constexpr double kGravity[3] = {0.0, -9.80665, 0.0};
constexpr double kRadiusM = 400.0;
constexpr double kCurvature = 1.0 / kRadiusM;
constexpr double kEnterM = 150.0;
constexpr double kSpiralM = 120.0;
constexpr double kArcM = 300.0;
constexpr double kGrade = 0.04;
constexpr double kCrestM = 6.0;
constexpr double kBankRad = 0.06;
constexpr double kHalfWidthM = 3.5;

constexpr double kMassKg = 1610.0;
constexpr double kCentreM = 0.55;
constexpr double kAnchorM = 0.333;
constexpr double kHalfTrackM = 0.774;
constexpr double kHalfBaseM = 1.405;
constexpr double kWheelbaseM = 2.810;
constexpr double kTyreRadiusM = 0.333;
constexpr double kDragCoefficient = 0.66;
constexpr double kFrontalM2 = 2.19;
constexpr double kGrip = 0.95;
constexpr double kSettleS = 1.0;
constexpr double kSteerLimitRad = 0.55;
constexpr size_t kCorners = 4;

Envelope F31Envelope(void) {
  Envelope out;
  out.GravityMs2 = 9.80665;
  out.Grip = kGrip;
  out.MassKg = kMassKg;
  out.DriveN = 400.0 * 3.08 / kTyreRadiusM;
  out.BrakeN = 2200.0 / kTyreRadiusM;
  out.DragArea = kDragCoefficient * kFrontalM2;
  out.AirDensity = 1.225;
  return out;
}

Rig F31Rig(void) {
  Rig out;
  out.Count = kCorners;
  const double atM[kCorners][3] = {{-kHalfTrackM, kAnchorM - kCentreM, -kHalfBaseM},
                                   {kHalfTrackM, kAnchorM - kCentreM, -kHalfBaseM},
                                   {-kHalfTrackM, kAnchorM - kCentreM, kHalfBaseM},
                                   {kHalfTrackM, kAnchorM - kCentreM, kHalfBaseM}};
  const double reachM[kCorners] = {0.45635, 0.45635, 0.44909, 0.44909};
  const double stiffness[kCorners] = {32000.0, 32000.0, 34000.0, 34000.0};
  const double damping[kCorners] = {3400.0, 3400.0, 3600.0, 3600.0};
  for (size_t which = 0; which < kCorners; ++which) {
    Mount &mount = out.Mounts[which];
    for (int axis = 0; axis < 3; ++axis) { mount.AtM[axis] = atM[which][axis]; }
    mount.Touches.ReachM = reachM[which];
    mount.Touches.StiffnessNPerM = stiffness[which];
    mount.Touches.DampingNsPerM = damping[which];
    mount.Touches.TravelM = 0.18;
    mount.Touches.StopNPerM = 450000.0;
    mount.Touches.LimitN = 24000.0;
    mount.Sheds.StiffnessNPerRad = 55000.0;
    mount.Sheds.RelaxationM = 0.4;
    mount.Sheds.Friction = kGrip;
    mount.SteeredShare = atM[which][2] < 0.0 ? 1.0 : 0.0;
    mount.DrivenShare = atM[which][2] > 0.0 ? 0.5 : 0.0;
    mount.BrakedShare = 0.25;
  }
  return out;
}

std::vector<Segment> Road() {
  return {{Curve::Straight, kEnterM, 0.0, 0.0},
          {Curve::Spiral, kSpiralM, 0.0, kCurvature},
          {Curve::Arc, kArcM, kCurvature, kCurvature},
          {Curve::Spiral, kSpiralM, kCurvature, 0.0},
          {Curve::Straight, kEnterM, 0.0, 0.0}};
}

void Unit(double v[3]) {
  const double length = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (length > 0.0) {
    for (int axis = 0; axis < 3; ++axis) { v[axis] /= length; }
  }
}

void Lie(Body &body, const Placed &on, const double normalM[3]) {
  double ahead[3] = {std::cos(on.HeadingRad), on.Slope, -std::sin(on.HeadingRad)};
  double up[3] = {normalM[0], normalM[1], normalM[2]};
  Unit(up);
  const double along = ahead[0] * up[0] + ahead[1] * up[1] + ahead[2] * up[2];
  for (int axis = 0; axis < 3; ++axis) { ahead[axis] -= along * up[axis]; }
  Unit(ahead);

  const double back[3] = {-ahead[0], -ahead[1], -ahead[2]};
  const double right[3] = {up[1] * back[2] - up[2] * back[1], up[2] * back[0] - up[0] * back[2],
                           up[0] * back[1] - up[1] * back[0]};

  const double m[3][3] = {{right[0], up[0], back[0]},
                          {right[1], up[1], back[1]},
                          {right[2], up[2], back[2]}};
  const double trace = m[0][0] + m[1][1] + m[2][2];
  double q[4];
  if (trace > 0.0) {
    const double root = std::sqrt(trace + 1.0) * 2.0;
    q[0] = 0.25 * root;
    q[1] = (m[2][1] - m[1][2]) / root;
    q[2] = (m[0][2] - m[2][0]) / root;
    q[3] = (m[1][0] - m[0][1]) / root;
  } else if (m[0][0] > m[1][1] && m[0][0] > m[2][2]) {
    const double root = std::sqrt(1.0 + m[0][0] - m[1][1] - m[2][2]) * 2.0;
    q[0] = (m[2][1] - m[1][2]) / root;
    q[1] = 0.25 * root;
    q[2] = (m[0][1] + m[1][0]) / root;
    q[3] = (m[0][2] + m[2][0]) / root;
  } else if (m[1][1] > m[2][2]) {
    const double root = std::sqrt(1.0 + m[1][1] - m[0][0] - m[2][2]) * 2.0;
    q[0] = (m[0][2] - m[2][0]) / root;
    q[1] = (m[0][1] + m[1][0]) / root;
    q[2] = 0.25 * root;
    q[3] = (m[1][2] + m[2][1]) / root;
  } else {
    const double root = std::sqrt(1.0 + m[2][2] - m[0][0] - m[1][1]) * 2.0;
    q[0] = (m[1][0] - m[0][1]) / root;
    q[1] = (m[0][2] + m[2][0]) / root;
    q[2] = (m[1][2] + m[2][1]) / root;
    q[3] = 0.25 * root;
  }
  for (int part = 0; part < 4; ++part) { body.OrientationQ[part] = q[part]; }
}

double HeadingOf(const Body &body) {
  const double aheadBody[3] = {0.0, 0.0, -1.0};
  double ahead[3];
  Turn(body.OrientationQ, aheadBody, ahead);
  return std::atan2(-ahead[2], ahead[0]);
}

struct Told {
  bool Lost = false;
  bool PastTravel = false;
  bool PastLimit = false;
  size_t MostAirborne = 0;
  double ReachedM = 0.0;
  double SimulatedS = 0.0;
  double WallS = 0.0;
  double WorstOffsetM = 0.0;
  double WorstOffsetAtM = 0.0;
  double WorstHeadingRad = 0.0;
  double WorstLateralMs2 = 0.0;
  double WorstVerticalMs2 = 0.0;
  double WorstJerkMs3 = 0.0;
  double WorstJerkAtM = 0.0;
  double WorstRatio = 0.0;
  double WorstPressedM = 0.0;
  double TopMs = 0.0;
  double LeastMs = 0.0;
};

constexpr double kFromM = 20.0;
constexpr double kEntryMs = 20.0;

Told Drove(const ReferenceLine &line, const SpeedProfile &profile, double capMs, double dtS,
           double forS) {
  Told out;
  Placed start;
  if (!line.At(0.0, start)) {
    out.Lost = true;
    return out;
  }

  Rig rig = F31Rig();
  const Envelope envelope = F31Envelope();
  Axles axles;
  axles.WheelbaseM = kWheelbaseM;
  axles.SteerLimitRad = kSteerLimitRad;

  Body body;
  body.MassKg = kMassKg;
  body.InertiaKgM2[0] = 540.0;
  body.InertiaKgM2[1] = 2400.0;
  body.InertiaKgM2[2] = 2600.0;
  body.PositionM[0] = start.EastM;
  body.PositionM[1] = start.HeightM + kCentreM;
  body.PositionM[2] = -start.NorthM;
  const Standing surface = Stand(line, start.EastM, start.NorthM, kHalfWidthM, 0.0, 10.0);
  const double up[3] = {surface.NormalM[0], surface.NormalM[1], -surface.NormalM[2]};
  Lie(body, start, up);
  double ahead[3];
  const double aheadBody[3] = {0.0, 0.0, -1.0};
  Turn(body.OrientationQ, aheadBody, ahead);
  for (int axis = 0; axis < 3; ++axis) { body.VelocityMs[axis] = kEntryMs * ahead[axis]; }

  const long steps = (long)std::llround(forS / dtS);
  double nearM = 0.0;
  double wasMs2[3] = {0.0, 0.0, 0.0};
  bool haveWas = false;
  out.LeastMs = 1.0e9;

  const auto began = std::chrono::steady_clock::now();
  for (long step = 0; step < steps; ++step) {
    const double eastM = body.PositionM[0];
    const double northM = -body.PositionM[2];
    const double headingRad = HeadingOf(body);

    const Placement at = Locate(line, eastM, northM, body.PositionM[1], headingRad, nearM, 60.0);
    if (!at.Found) {
      out.Lost = true;
      break;
    }
    nearM = at.AlongM;
    out.ReachedM = at.AlongM;

    const double speedMs = std::sqrt(body.VelocityMs[0] * body.VelocityMs[0] +
                                     body.VelocityMs[2] * body.VelocityMs[2]);

    Reins reins;
    reins.SettleS = kSettleS;
    reins.LeastReachM = kWheelbaseM;
    reins.TightestPerM = TightestPerM(axles, envelope, speedMs);
    const double aheadM =
        std::fmin(at.AlongM + ReachOf(reins, speedMs), line.LengthM());
    double wantedMs = profile.At(aheadM);
    if (capMs > 0.0 && wantedMs > capMs) { wantedMs = capMs; }
    const Demand asked = Hold(line, reins, at, speedMs, wantedMs);
    const Steering command = Drive(axles, envelope, asked);

    Controls controls;
    controls.SteerRad = command.SteerRad;
    controls.DriveN = command.DriveN;
    controls.BrakeN = command.BrakeN;

    Footing under[kCorners];
    for (size_t which = 0; which < kCorners; ++which) {
      double worldM[3];
      Place(body, rig.Mounts[which].AtM, worldM);
      const Standing on =
          Stand(line, worldM[0], -worldM[2], kHalfWidthM, at.AlongM, 60.0);
      under[which].Found = on.On;
      under[which].HeightM = on.HeightM;
      under[which].NormalM[0] = on.NormalM[0];
      under[which].NormalM[1] = on.NormalM[1];
      under[which].NormalM[2] = -on.NormalM[2];
    }

    Wrench wrench;
    Fall(wrench, body, kGravity);
    Resist(wrench, body, kDragCoefficient * kFrontalM2, 1.225);
    const Reading read = Bear(rig, body, under, controls, wrench, dtS);

    if (at.AlongM >= kFromM) {
      if (std::fabs(at.OffsetM) > std::fabs(out.WorstOffsetM)) {
        out.WorstOffsetM = at.OffsetM;
        out.WorstOffsetAtM = at.AlongM;
      }
      out.WorstHeadingRad = std::fmax(out.WorstHeadingRad, std::fabs(at.HeadingErrorRad));
      out.WorstRatio = std::fmax(out.WorstRatio, read.WorstRatio);
      out.PastTravel = out.PastTravel || read.PastTravel;
      out.PastLimit = out.PastLimit || read.PastLimit;
      out.MostAirborne = read.Airborne > out.MostAirborne ? read.Airborne : out.MostAirborne;
      out.TopMs = std::fmax(out.TopMs, speedMs);
      out.LeastMs = std::fmin(out.LeastMs, speedMs);
      for (size_t which = 0; which < kCorners; ++which) {
        out.WorstPressedM = std::fmax(out.WorstPressedM, read.PressedM[which]);
      }

      double isMs2[3];
      for (int axis = 0; axis < 3; ++axis) { isMs2[axis] = wrench.ForceN[axis] / body.MassKg; }
      const double lateral =
          std::fabs(isMs2[0] * -std::sin(at.HeadingErrorRad + headingRad) +
                    isMs2[2] * std::cos(at.HeadingErrorRad + headingRad));
      out.WorstLateralMs2 = std::fmax(out.WorstLateralMs2, lateral);
      out.WorstVerticalMs2 = std::fmax(out.WorstVerticalMs2, std::fabs(isMs2[1]));
      if (haveWas) {
        double jerk = 0.0;
        for (int axis = 0; axis < 3; ++axis) {
          const double rate = (isMs2[axis] - wasMs2[axis]) / dtS;
          jerk += rate * rate;
        }
        if (std::sqrt(jerk) > out.WorstJerkMs3) {
          out.WorstJerkMs3 = std::sqrt(jerk);
          out.WorstJerkAtM = at.AlongM;
        }
      }
      for (int axis = 0; axis < 3; ++axis) { wasMs2[axis] = isMs2[axis]; }
      haveWas = true;
    }

    Step(body, wrench, dtS);
    out.SimulatedS = (double)(step + 1) * dtS;
    if (at.AlongM >= line.LengthM() - 5.0) { break; }
  }
  out.WallS = std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
  if (out.LeastMs > 1.0e8) { out.LeastMs = 0.0; }
  return out;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  ReferenceLine line;
  std::string error;
  if (!line.Lay(Placed{}, Road(), error)) {
    std::printf("REFUSED %s\n", error.c_str());
    return Report();
  }
  const double lengthM = line.LengthM();
  const double summitM = 0.5 * lengthM;
  if (!line.Rise({{0.0, 0.0, kGrade}, {summitM, kCrestM, 0.0}, {lengthM, 0.0, -kGrade}}, error) ||
      !line.Bank({{kEnterM, 0.0, 0.0},
                  {kEnterM + kSpiralM, kBankRad, 0.0},
                  {kEnterM + kSpiralM + kArcM, kBankRad, 0.0},
                  {kEnterM + 2.0 * kSpiralM + kArcM, 0.0, 0.0}},
                 error)) {
    std::printf("REFUSED %s\n", error.c_str());
    return Report();
  }

  SpeedProfile profile;
  if (!profile.Over(line, F31Envelope(), 1.0, kEntryMs, error)) {
    std::printf("REFUSED %s\n", error.c_str());
    return Report();
  }

  const Told drove = Drove(line, profile, 0.0, 1.0e-3, 90.0);
  const Told paced = Drove(line, profile, 25.0, 1.0e-3, 120.0);

  Note("how long the road is", lengthM, "m");
  Note("the distance from the start the floor is measured over", lengthM - kFromM, "m");
  Note("how far the car got", drove.ReachedM, "m");
  Note("the speed it joined the road at", kEntryMs, "m/s");
  Note("the fastest it went", drove.TopMs, "m/s");
  Note("seconds simulated", drove.SimulatedS, "s");
  Note("seconds of wall clock it took", drove.WallS, "s");
  Note("how much faster than real time that is", drove.SimulatedS / drove.WallS, "x");

  CHECK(!drove.Lost && drove.ReachedM > lengthM - 10.0,
        "**THE NEGATIVE CONTROL: the F31 drives, on its own contacts, a road this engine built.** "
        "Four compliant contacts, tyre shear with a friction circle, weight transfer, drag, and a "
        "pilot that demands a curvature -- and no renderer linked at all. Every number this suite "
        "ever reports about real data is read against the ones below");
  CHECK(drove.SimulatedS / drove.WallS > 10.0,
        "and it does it far faster than real time, which is why the headless run links no device: a "
        "suite that drives 800 km in real time is a suite nobody runs");

  Note("worst share of a contact's grip used", drove.WorstRatio, "of it");
  Note("worst a contact was pressed", drove.WorstPressedM, "m");
  Note("the travel it has", 0.18, "m");
  Note("what fraction of its travel that is", drove.WorstPressedM / 0.18, "of it");
  Note("what a lateral transfer of m a h / track alone puts on the outer corner",
       (kMassKg * 9.80665 / 4.0 + 0.5 * kMassKg * drove.WorstLateralMs2 * kCentreM /
                                      (2.0 * kHalfTrackM)) /
           32000.0,
       "m");
  Note("most mounts off the ground at once", (double)drove.MostAirborne, "of 4");
  Note("worst lateral acceleration", drove.WorstLateralMs2, "m/s2");
  Note("what the tyres have", F31Envelope().LateralMs2(), "m/s2");

  CHECK(!drove.PastLimit && !drove.PastTravel,
        "**NOTHING BROKE AND NOTHING BOTTOMED OUT.** On a road that is smooth by construction the "
        "contacts stay inside their travel, so a contact past its travel on real data is the ROAD "
        "and never the suspension being too soft");
  CHECK(drove.MostAirborne == 0,
        "and no wheel ever left the ground, which the corridor test already predicted: this crest "
        "would need 235 m/s to launch the car");
  CHECK(drove.WorstPressedM / 0.18 > 0.85,
        "**AND HERE IS A FINDING IN THE FLOOR ITSELF: on a road with no defect in it the car already "
        "uses 93 % of its suspension travel.** It is not a mystery -- the lateral transfer at "
        "3.89 m/s2 alone puts 0.158 m of the 0.168 on the outer corner -- but it means the FIRST "
        "real bump bottoms out. Either the speed profile is too brave or the travel is too small, "
        "and which of the two is board:1522");
  CHECK(drove.WorstRatio < 1.0,
        "no contact ever asked for more grip than it had, so nothing here is a slide and the "
        "numbers below are the tyres working rather than failing");

  const double rateOfCurvature = kCurvature / kSpiralM;
  const double atPaceM = rateOfCurvature * std::pow(kSettleS * 25.0, 3.0) / 6.0;
  const double atSpeedM = rateOfCurvature * std::pow(kSettleS * drove.TopMs, 3.0) / 6.0;

  Note("worst deviation at the speed the profile asks for", drove.WorstOffsetM, "m");
  Note("where that happened", drove.WorstOffsetAtM, "m");
  Note("what the pursuit law's clothoid lag accounts for there", atSpeedM, "m");
  Note("worst deviation held to 25 m/s", paced.WorstOffsetM, "m");
  Note("what the clothoid lag accounts for there", atPaceM, "m");
  Note("what a kinematic bicycle did at 25 m/s", 0.056839, "m");
  Note("so what being a VEHICLE costs at 25 m/s",
       std::fabs(paced.WorstOffsetM) - 0.056839, "m");
  Note("worst heading error at 25 m/s", paced.WorstHeadingRad, "rad");

  CHECK(std::fabs(paced.WorstOffsetM) > 0.056839,
        "**A CAR HOLDS THE LINE LESS WELL THAN A BICYCLE MODEL, AND THE DIFFERENCE IS THE POINT.** "
        "Same road, same speed, same pilot, same look-ahead: what changed is mass, tyres that build "
        "shear over a relaxation length, weight that moves under braking and 2600 kg m2 of yaw "
        "inertia between a steering angle and a heading. **That difference is measured here rather "
        "than assumed, which is what makes it subtractable from a finding on real data**");
  CHECK(std::fabs(paced.WorstOffsetM) < 0.40,
        "and it is still well inside a lane, on a road with no defect in it -- so a deviation on "
        "real data is only a finding when it exceeds this");
  CHECK(std::fabs(drove.WorstOffsetM) > std::fabs(paced.WorstOffsetM),
        "and driving the road at the speed the profile actually asks for costs more, as the cube of "
        "the look-ahead distance says it must");
  Note("what the clothoid lag leaves unaccounted at the profile's speed",
       std::fabs(drove.WorstOffsetM) - atSpeedM, "m");
  Note("the share of a contact's grip in use there", drove.WorstRatio, "of it");
  CHECK(std::fabs(drove.WorstOffsetM) - atSpeedM > 0.5,
        "**AND AT THE PROFILE'S SPEED NEARLY A METRE OF IT IS STILL UNNAMED**, with 0.88 of a "
        "contact's grip already in use. The profile plans to the grip limit and leaves nothing for "
        "the tracking error, so the car cannot hold the line at the speed it was told to hold. That "
        "is a finding about the PILOT and the PLANNER and not about the road -- board:1522 -- and "
        "naming it is why the floor is measured at two speeds rather than one");

  Note("worst jerk at the profile's speed", drove.WorstJerkMs3, "m/s3");
  Note("where that happened", drove.WorstJerkAtM, "m");
  Note("worst jerk held to 25 m/s", paced.WorstJerkMs3, "m/s3");
  Note("where that happened", paced.WorstJerkAtM, "m");
  CHECK(paced.WorstJerkMs3 < 5.0 && drove.WorstJerkMs3 < 10.0,
        "**AND THE JERK FLOOR IS PUBLISHED WITH THE PLACE IT HAPPENED**, because on real data a step "
        "in the road shows in the jerk long before it shows in the deviation -- and a jerk floor "
        "without its station cannot be told apart from an artefact. **This number was 1133 m/s3 "
        "until its station was published**: it sat at 20.91 m in every run at every speed, which is "
        "not what a road does, and it was the resection stepping a whole metre at a time. "
        "board:1523");

  Covers("I.9.10 the drive suite's negative control: the F31 drives a road this engine built, on "
         "four compliant contacts with tyre shear and weight transfer, headless and far faster than "
         "real time -- and its deviation, jerk, vertical acceleration and grip usage are published "
         "as the floor every finding on real data is read against");
  return Report();
}
