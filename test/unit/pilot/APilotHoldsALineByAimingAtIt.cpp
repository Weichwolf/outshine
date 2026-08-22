#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include "Drive.h"

using outshine::Curve;
using outshine::Envelope;
using outshine::Placed;
using outshine::ReferenceLine;
using outshine::Segment;
using outshine::SpeedProfile;
using outshine::Pilot::Axles;
using outshine::Pilot::Demand;
using outshine::Pilot::Drive;
using outshine::Pilot::Hold;
using outshine::Pilot::Locate;
using outshine::Pilot::Placement;
using outshine::Pilot::Reins;
using outshine::Pilot::Sight;
using outshine::Pilot::Sighting;
using outshine::Pilot::Steering;
using outshine::Pilot::TightestPerM;

namespace {

constexpr double kRadiusM = 400.0;
constexpr double kCurvature = 1.0 / kRadiusM;
constexpr double kEnterM = 150.0;
constexpr double kSpiralM = 120.0;
constexpr double kArcM = 300.0;
constexpr double kWheelbaseM = 2.810;
constexpr double kSettleS = 1.0;
constexpr double kSteerLimitRad = 0.55;
constexpr double kWindowM = 40.0;
constexpr double kCruiseMs = 25.0;

std::vector<Segment> Road() {
  return {{Curve::Straight, kEnterM, 0.0, 0.0},
          {Curve::Spiral, kSpiralM, 0.0, kCurvature},
          {Curve::Arc, kArcM, kCurvature, kCurvature},
          {Curve::Spiral, kSpiralM, kCurvature, 0.0},
          {Curve::Straight, kEnterM, 0.0, 0.0}};
}

Envelope F31() {
  Envelope out;
  out.GravityMs2 = 9.80665;
  out.Grip = 0.95;
  out.MassKg = 1610.0;
  out.DriveN = 400.0 * 3.08 / 0.333;
  out.BrakeN = 2200.0 / 0.333;
  out.DragArea = 0.66 * 2.19;
  out.AirDensity = 1.225;
  return out;
}

Axles F31Axles() {
  Axles out;
  out.WheelbaseM = kWheelbaseM;
  out.SteerLimitRad = kSteerLimitRad;
  return out;
}

Reins Hands(double speedMs, double settleS) {
  Reins out;
  out.SettleS = settleS;
  out.LeastReachM = kWheelbaseM;
  out.TightestPerM = TightestPerM(F31Axles(), F31(), speedMs);
  return out;
}

struct Bicycle {
  double EastM = 0.0;
  double NorthM = 0.0;
  double HeadingRad = 0.0;
  double SpeedMs = 0.0;
};

struct Drove {
  double WorstOffsetM = 0.0;
  double WorstOffsetAtM = 0.0;
  double WorstHeadingRad = 0.0;
  double WorstLateralMs2 = 0.0;
  double WorstSteerRateRadS = 0.0;
  double ReachedM = 0.0;
  double SettledAtS = -1.0;
  bool Lost = false;
};

Drove Along(const ReferenceLine &line, const SpeedProfile &profile, double fromOffsetM,
            double atSpeedMs, double settleS, double dtS, double forS) {
  Placed start;
  if (!line.At(0.0, start)) { return Drove{}; }

  Bicycle car;
  car.EastM = start.EastM - std::sin(start.HeadingRad) * fromOffsetM;
  car.NorthM = start.NorthM + std::cos(start.HeadingRad) * fromOffsetM;
  car.HeadingRad = start.HeadingRad;
  car.SpeedMs = atSpeedMs > 0.0 ? atSpeedMs : profile.At(0.0);

  Drove out;
  const Envelope envelope = F31();
  const Axles axles = F31Axles();
  const long steps = (long)std::llround(forS / dtS);
  double nearM = 0.0;
  double lastSteer = 0.0;
  bool settled = false;

  for (long step = 0; step < steps; ++step) {
    const Placement at =
        Locate(line, car.EastM, car.NorthM, 0.0, car.HeadingRad, nearM, kWindowM);
    if (!at.Found) {
      out.Lost = true;
      break;
    }
    nearM = at.AlongM;
    out.ReachedM = at.AlongM;
    if (at.AlongM >= line.LengthM() - 2.0 * settleS * car.SpeedMs) { break; }

    const double wantedMs = atSpeedMs > 0.0 ? atSpeedMs : profile.At(at.AlongM);
    const Demand asked = Hold(line, Hands(car.SpeedMs, settleS), at, car.SpeedMs, wantedMs);
    const Steering command = Drive(axles, envelope, asked);

    if (std::fabs(at.OffsetM) > std::fabs(out.WorstOffsetM)) {
      out.WorstOffsetM = at.OffsetM;
      out.WorstOffsetAtM = at.AlongM;
    }
    out.WorstHeadingRad = std::fmax(out.WorstHeadingRad, std::fabs(at.HeadingErrorRad));
    out.WorstLateralMs2 =
        std::fmax(out.WorstLateralMs2, std::fabs(car.SpeedMs * car.SpeedMs * asked.CurvaturePerM));
    out.WorstSteerRateRadS =
        std::fmax(out.WorstSteerRateRadS, std::fabs(command.SteerRad - lastSteer) / dtS);
    lastSteer = command.SteerRad;
    if (!settled && step > 0 && std::fabs(at.OffsetM) < 0.05) {
      settled = true;
      out.SettledAtS = (double)step * dtS;
    }

    car.SpeedMs += (command.DriveN - command.BrakeN) / envelope.MassKg * dtS;
    car.EastM += car.SpeedMs * std::cos(car.HeadingRad) * dtS;
    car.NorthM += car.SpeedMs * std::sin(car.HeadingRad) * dtS;
    car.HeadingRad += car.SpeedMs * std::tan(command.SteerRad) / kWheelbaseM * dtS;
  }
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

  const double middleM = kEnterM + kSpiralM + 0.5 * kArcM;
  Placed onArc;
  const bool located = line.At(middleM, onArc);
  const double offsetM = 2.0;
  const Placement beside =
      Locate(line, onArc.EastM - std::sin(onArc.HeadingRad) * offsetM,
             onArc.NorthM + std::cos(onArc.HeadingRad) * offsetM, 0.0, onArc.HeadingRad + 0.03,
             middleM, kWindowM);
  Note("the station the resection found", beside.AlongM, "m");
  Note("the offset it found", beside.OffsetM, "m");
  Note("the heading error it found", beside.HeadingErrorRad, "rad");

  CHECK(located && beside.Found,
        "**RESECTION IS THE FIRST THING AND IT IS THE SAME FOR EVERY WAY OF MOVING.** A walker, a "
        "car, a train and an aircraft all answer where am I on this line, and none of them answers "
        "it differently -- so it lives once, in the course, and no mode owns it");
  CHECK_NEAR(beside.AlongM, middleM, 0.01, "m", "at the station it stands square to");
  CHECK_NEAR(beside.OffsetM, offsetM, 0.01, "m",
             "with the offset signed and positive to the left, because a tracker whose error has no "
             "side can only know it is wrong");
  CHECK_NEAR(beside.HeadingErrorRad, 0.03, 1.0e-9, "rad", "and the heading error it was given");

  Placement onLine;
  onLine.Found = true;
  onLine.EastM = onArc.EastM;
  onLine.NorthM = onArc.NorthM;
  onLine.HeadingRad = onArc.HeadingRad;
  onLine.AlongM = middleM;
  onLine.CurvaturePerM = kCurvature;

  const Sighting ahead = Sight(line, onLine, kSettleS * kCruiseMs);
  Note("the chord the sighting actually reached", ahead.ChordM, "m");
  CHECK_NEAR(ahead.ChordM, kSettleS * kCruiseMs, 1.0e-5, "m",
             "**A SIGHTING IS A CHORD AND NOT A STATION.** Aiming a chord ahead is what makes the "
             "law exact on a circle; aiming an arc length ahead is what makes it cut corners");

  const Demand onCircle = Hold(line, Hands(kCruiseMs, kSettleS), onLine, kCruiseMs, kCruiseMs);
  Note("the curvature demanded by a vehicle exactly on a 400 m curve", onCircle.CurvaturePerM,
       "1/m");
  Note("the curve's own curvature", kCurvature, "1/m");
  CHECK_NEAR(onCircle.CurvaturePerM, kCurvature, 1.0e-9, "1/m",
             "**A VEHICLE EXACTLY ON A CIRCLE DEMANDS EXACTLY THAT CIRCLE, AT ANY LOOK-AHEAD.** "
             "2 sin(alpha) / chord is 1/R identically when both ends of the chord lie on the "
             "circle -- so pure pursuit needs NO feedforward term beside it, and there is nothing "
             "to double count. That is why the demand is a CURVATURE: it is the one quantity a "
             "walker, a car, a train and an aircraft all mean the same thing by");

  const Steering wheel = Drive(F31Axles(), F31(), onCircle);
  Note("the steer angle that becomes", wheel.SteerRad, "rad");
  CHECK_NEAR(wheel.SteerRad, std::atan(kWheelbaseM * kCurvature), 1.0e-9, "rad",
             "and the CAR turns atan(L kappa), which is the whole of what being a car adds -- the "
             "mode converts one number and owns nothing else");

  ReferenceLine straight;
  if (!straight.Lay(Placed{}, {{Curve::Straight, 1000.0, 0.0, 0.0}}, error)) {
    std::printf("REFUSED %s\n", error.c_str());
    return Report();
  }
  Placement offLine;
  offLine.Found = true;
  offLine.EastM = 100.0;
  offLine.NorthM = 1.0;
  offLine.HeadingRad = 0.0;
  offLine.AlongM = 100.0;
  offLine.OffsetM = 1.0;

  const double reachM = kSettleS * kCruiseMs;
  const Demand back = Hold(straight, Hands(kCruiseMs, kSettleS), offLine, kCruiseMs, kCruiseMs);
  Note("the curvature demanded one metre left of a straight line", back.CurvaturePerM, "1/m");
  Note("what 2 e / d^2 says it is", -2.0 * 1.0 / (reachM * reachM), "1/m");
  CHECK_NEAR(back.CurvaturePerM, -2.0 * 1.0 / (reachM * reachM), 1.0e-9, "1/m",
             "**AND OFF THE LINE IT DEMANDS 2 e / d SQUARED, WHICH IS PURE PURSUIT WRITTEN OUT.** "
             "The one number this pilot carries is a look-ahead TIME; the distance is that time "
             "times the speed, and the gain falls out of it. There is no tuned constant anywhere in "
             "this file");

  Placement sideways = offLine;
  sideways.NorthM = 0.0;
  sideways.OffsetM = 0.0;
  sideways.HeadingRad = 0.5 * 3.14159265358979;
  const Demand spun = Hold(straight, Hands(kCruiseMs, kSettleS), sideways, kCruiseMs, kCruiseMs);
  Note("the curvature a car pointing square across the road asks for", spun.AskedPerM, "1/m");
  Note("the curvature it settles for", spun.CurvaturePerM, "1/m");
  Note("the tightest this car can turn at 25 m/s", TightestPerM(F31Axles(), F31(), kCruiseMs),
       "1/m");
  Note("the tightest its steering lock allows at all", std::tan(kSteerLimitRad) / kWheelbaseM,
       "1/m");
  CHECK_NEAR(spun.AskedPerM, -2.0 / reachM, 1.0e-9, "1/m",
             "a car square across the line asks to turn on a circle of half the look-ahead, because "
             "the chord it is aiming along is a diameter");
  CHECK(spun.Saturated &&
            std::fabs(spun.CurvaturePerM) <= TightestPerM(F31Axles(), F31(), kCruiseMs) + 1e-12,
        "**AND IT SAYS SO WHEN IT CANNOT DO WHAT IT ASKED.** The limit is the lesser of the "
        "steering lock and the grip at this speed, both derived; the demand publishes what it "
        "wanted beside what it took, because a controller that silently clips is a controller "
        "nobody can debug from a log");
  CHECK(TightestPerM(F31Axles(), F31(), kCruiseMs) < std::tan(kSteerLimitRad) / kWheelbaseM,
        "and at 25 m/s it is the TYRES and not the rack that decide, which is a fact about the "
        "speed rather than about the car");

  Placement wayOff = offLine;
  wayOff.NorthM = 400.0;
  wayOff.OffsetM = 400.0;
  const Demand far = Hold(straight, Hands(kCruiseMs, kSettleS), wayOff, kCruiseMs, kCruiseMs);
  Note("the chord it could actually reach 400 m off the line", far.ReachM, "m");
  CHECK(far.OutOfReach,
        "**AND WHEN NO POINT ON THE LINE IS WITHIN THE LOOK-AHEAD AT ALL, IT SAYS THAT TOO.** A "
        "pursuit circle that does not meet the path has no aim point; this one turns square onto "
        "the nearest point and publishes that it is doing so, which is the difference between a "
        "declared behaviour and an iteration that happened to converge");
  CHECK_NEAR(far.CurvaturePerM, -2.0 / 400.0, 1.0e-9, "1/m",
             "turning onto a circle whose diameter is the distance to the line, which is the "
             "tightest arc that arrives there square");

  SpeedProfile profile;
  if (!profile.Over(line, F31(), 1.0, 30.0, error)) {
    std::printf("REFUSED %s\n", error.c_str());
    return Report();
  }

  const double rateOfCurvature = kCurvature / kSpiralM;
  const Drove clean = Along(line, profile, 0.0, kCruiseMs, kSettleS, 1.0e-3, 40.0);
  const Drove brisk = Along(line, profile, 0.0, kCruiseMs, 0.5 * kSettleS, 1.0e-3, 40.0);

  Note("worst deviation driving the road from exactly on it", clean.WorstOffsetM, "m");
  Note("where that happened", clean.WorstOffsetAtM, "m");
  Note("worst heading error", clean.WorstHeadingRad, "rad");
  Note("worst lateral acceleration asked of the tyres", clean.WorstLateralMs2, "m/s2");
  Note("what the tyres have", F31().LateralMs2(), "m/s2");
  Note("worst rate the wheel was turned", clean.WorstSteerRateRadS, "rad/s");
  Note("how far along it got", clean.ReachedM, "m");
  Note("how long the road is", line.LengthM(), "m");

  const double reachedM = kSettleS * kCruiseMs;
  const double predictedM = rateOfCurvature * reachedM * reachedM * reachedM / 6.0;
  Note("the spiral's rate of curvature", rateOfCurvature, "1/m2");
  Note("what a pursuit lag of c d^3 / 6 predicts at a 1 s look-ahead", predictedM, "m");
  Note("worst deviation at a 0.5 s look-ahead", brisk.WorstOffsetM, "m");
  Note("what the same term predicts there", predictedM / 8.0, "m");

  CHECK(!clean.Lost, "**THE NEGATIVE CONTROL: a car started exactly on a road this engine built "
                     "drives it.** Every number after this is read against these, and without them "
                     "a deviation on real data has three candidate causes and names none");
  CHECK_NEAR(std::fabs(clean.WorstOffsetM), predictedM, 0.1 * predictedM, "m",
             "**AND THE RESIDUAL IS NOT NOISE, IT IS A TERM WITH A NAME.** Pure pursuit is exact on "
             "a circle and lags a CLOTHOID, where the curvature ramps at c per metre: aiming a "
             "chord d ahead leaves the vehicle c d^3 / 6 inside the turn, and that is what is "
             "measured. A floor that lands on a term already named is a floor; a floor that is "
             "merely small is a threshold");
  CHECK_NEAR(std::fabs(clean.WorstOffsetM / brisk.WorstOffsetM), 8.0, 1.0, "x",
             "and halving the look-ahead divides it by eight, which is the CUBE saying so out loud. "
             "**This is the trade the pilot actually has**: tracking improves as the cube of a "
             "shorter look-ahead and stability leaves with it, so the settling time is a decision "
             "somebody makes rather than a constant somebody tunes");
  CHECK(std::fabs(clean.WorstOffsetM) < 0.06 && std::fabs(brisk.WorstOffsetM) < 0.01,
        "and at both look-aheads it stays inside a tyre's width of the line, on a road that has no "
        "defect in it at all -- **a swerve on real data is anything this cannot account for**");
  CHECK(clean.WorstLateralMs2 < F31().LateralMs2(),
        "asking the tyres for less than they have the whole way, so nothing here is a slide");

  const Drove recovering = Along(line, profile, 2.0, kCruiseMs, kSettleS, 1.0e-3, 40.0);
  Note("worst deviation starting 2 m off the line", recovering.WorstOffsetM, "m");
  Note("when it was back within 50 mm", recovering.SettledAtS, "s");
  Note("the look-ahead time it was given", kSettleS, "s");
  Note("worst lateral acceleration on the way back", recovering.WorstLateralMs2, "m/s2");

  CHECK(recovering.SettledAtS > 0.0 && recovering.SettledAtS < 6.0 * kSettleS,
        "**AND A CAR PUT 2 M OFF THE LINE COMES BACK** within a few look-ahead times, so the one "
        "number is the one that decides and convergence is a measurement rather than a hope");
  CHECK(std::fabs(recovering.WorstOffsetM) <= 2.0 + 1.0e-6,
        "without ever going further out than it started, which is what makes the return itself not "
        "a swerve");
  CHECK(recovering.WorstLateralMs2 < F31().LateralMs2(),
        "and it comes back inside the grip it has, because the demand was limited before it became "
        "a steering angle rather than after");

  Covers("I.9.5 a pilot holds a reference line by aiming a chord ahead of itself: the demand is a "
         "CURVATURE, exact on a circle, 2e/d^2 off a straight one, limited by what the vehicle can "
         "do and published in both forms -- and a car converts it with atan(L kappa) and nothing "
         "else");
  return Report();
}
