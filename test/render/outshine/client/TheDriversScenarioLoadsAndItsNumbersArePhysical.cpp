#include <cmath>
#include <cstdio>
#include <string>

#include <outshine/Outshine.h>

#include "Check.h"

namespace {

constexpr double kWheelbaseMm = 2810.0;
constexpr double kTrackFrontMm = 1543.0;
constexpr double kTyreRadiusMm = 328.0;
constexpr double kKerbKg = 1610.0;

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  outshine::Engine engine;
  const bool loaded = engine.Read("tools/driver/f31.scenario");
  if (!loaded) { std::printf("REFUSED %s\n", engine.Error().c_str()); }
  CHECK(loaded, "the driver's own scenario is read by the engine that will drive it -- this is the "
                "file, not a fixture that resembles it. **READING IS NOT STANDING UP**: a declaration "
                "can be checked without its 30 MB of geometry, which is what a scenario validator "
                "needs and what this test is");
  if (!loaded) { return Report(); }

  const outshine::Scenario &read = engine.Declared();
  CHECK(read.Vehicles.size() == 1, "it declares one vehicle");
  if (read.Vehicles.empty()) { return Report(); }
  const outshine::Vehicle &car = read.Vehicles[0];

  CHECK(car.Contacts.size() == 4, "a car has four contacts, and the shape is 0 or 1..N rather than "
                                  "four fields named for wheels");

  double frontZ = 0.0, rearZ = 0.0, leftX = 0.0, rightX = 0.0;
  size_t front = 0, rear = 0;
  for (const outshine::Contact &wheel : car.Contacts) {
    if (wheel.AtM[2] < 0.0) {
      frontZ += wheel.AtM[2];
      ++front;
    } else {
      rearZ += wheel.AtM[2];
      ++rear;
    }
    leftX = std::fmin(leftX, wheel.AtM[0]);
    rightX = std::fmax(rightX, wheel.AtM[0]);
  }
  CHECK(front == 2 && rear == 2, "two of them ahead of the centre of mass and two behind it");
  if (front != 2 || rear != 2) { return Report(); }

  const double wheelbaseM = std::fabs(rearZ / (double)rear - frontZ / (double)front);
  const double trackM = rightX - leftX;
  Note("wheelbase the contacts span", wheelbaseM, "m");
  Note("what the F31 publishes", kWheelbaseMm / 1000.0, "m");
  Note("track the contacts span", trackM, "m");
  Note("what the F31 publishes at the front", kTrackFrontMm / 1000.0, "m");

  CHECK_NEAR(wheelbaseM, kWheelbaseMm / 1000.0, 0.01, "m",
             "**THE CONTACTS ARE WHERE THE WHEELS ARE.** The wheelbase they span is the one the F31 "
             "publishes, because it was measured off the asset's own tyre geometry and the asset's "
             "scale was derived from that same dimension");
  CHECK_NEAR(trackM, kTrackFrontMm / 1000.0, 0.02, "m",
             "and the track agrees too, which is the independent check: the scale came from the "
             "wheelbase and the track was never used to set it");
  CHECK_NEAR(car.WheelbaseM, kWheelbaseMm / 1000.0, 1e-9, "m",
             "and the declaration states the dimension its scale was derived from, so the asset's "
             "units are a fact the file carries rather than a constant somewhere in the reader");

  CHECK_NEAR(car.TyreRadiusM, kTyreRadiusMm / 1000.0, 0.01, "m",
             "the tyre radius is a 225/50 R17's, within 1.5 % -- a fourth measurement nobody chose, "
             "and its agreeing is why the other three are believed");
  for (const outshine::Contact &wheel : car.Contacts) {
    CHECK_NEAR(wheel.AtM[1], car.TyreRadiusM, 1e-9, "m",
               "and every contact sits exactly a tyre radius above the ground, which is what a wheel "
               "standing still means");
  }

  CHECK(car.MassKg == kKerbKg && car.MassKg > 0.0, "the mass is the car's kerb weight");
  Note("the centre of mass above the ground", car.CentreOfMassM[1], "m");
  Note("how far above the wheel centres that is", car.CentreOfMassM[1] - car.TyreRadiusM, "m");
  CHECK(car.CentreOfMassM[1] > car.TyreRadiusM && car.CentreOfMassM[1] < 0.75,
        "**AND IT DECLARES WHERE ITS MASS IS, which is the number whose ABSENCE is invisible.** A "
        "car with no declared centre of mass settles at exactly the right height, stands level and "
        "transfers no weight at all under braking -- every static check passes and the dynamics are "
        "silently dead. It sits above the wheel centres and below the roof");
  CHECK(car.CentreOfMassM[0] == 0.0 && car.CentreOfMassM[2] == 0.0,
        "on the centreline and midway between the axles, which is the 50:50 distribution this car "
        "is built around -- so the two front contacts and the two rear ones carry the same load, "
        "and that is a consequence of where the mass is rather than of the springs");
  CHECK(car.InertiaKgM2[0] > 0.0 && car.InertiaKgM2[1] > 0.0 && car.InertiaKgM2[2] > 0.0,
        "and it carries an inertia tensor, because a body that resists rotation is not a point");
  CHECK(car.InertiaKgM2[1] > car.InertiaKgM2[0] && car.InertiaKgM2[2] > car.InertiaKgM2[0],
        "whose roll term is the smallest, which is what being long and narrow means");

  for (const outshine::Contact &wheel : car.Contacts) {
    CHECK(wheel.ReachM > 0.0 && wheel.StiffnessNPerM > 0.0 && wheel.DampingNsPerM > 0.0 &&
              wheel.TravelM > 0.0 && wheel.StopNPerM > wheel.StiffnessNPerM && wheel.LimitN > 0.0,
          "**A CONTACT IS COMPLIANT AND NOT A CONSTRAINT**: a reach, a stiffness, a damping, a travel, "
          "a stop far stiffer than the elastic stage, and a LIMIT -- which is what makes the bump that "
          "breaks it a derivation rather than a threshold somebody picked. *The physics knows no "
          "wheel and no strut; what this contact IS belongs to the declaration that placed it*");
  }

  const double corner = car.MassKg * 9.80665 / 4.0;
  for (const outshine::Contact &wheel : car.Contacts) {
    CHECK_NEAR(wheel.ReachM - corner / wheel.StiffnessNPerM, wheel.AtM[1], 1e-4, "m",
               "**THE REACH IS DERIVED AND NOT PICKED**: pressed by its share of the car's weight, a "
               "contact of this reach and this stiffness leaves its anchor exactly where the anchor "
               "says it stands. The front and the rear differ because their rates do -- so the car "
               "sits level as a CONSEQUENCE of four numbers rather than as a fifth one");
  }
  const double sagM = corner / car.Contacts[0].StiffnessNPerM;
  const double rideHz = std::sqrt(car.Contacts[0].StiffnessNPerM / (car.MassKg / 4.0)) / (2.0 * 3.14159265358979);
  Note("static sag at one corner", sagM, "m");
  Note("the ride frequency that implies", rideHz, "Hz");
  CHECK(sagM > 0.02 && sagM < car.Contacts[0].TravelM,
        "the car sits down on its springs by a sensible amount and does not use its whole travel "
        "standing still");
  CHECK(rideHz > 0.8 && rideHz < 2.0,
        "and the ride frequency it implies is a road car's rather than a race car's -- a number "
        "DERIVED from the mass and the rate rather than declared beside them");

  CHECK(car.Grip > 0.0 && car.DragCoefficient > 0.0 && car.FrontalM2 > 0.0 &&
            car.AirDensity > 0.0 && car.PeakTorqueNm > 0.0,
        "every quantity the speed profile needs is physical: a friction coefficient, a drag "
        "COEFFICIENT beside the frontal area it multiplies, an air density and a torque -- never a "
        "top speed or a lateral limit somebody tuned");
  const double dragAreaM2 = car.DragCoefficient * car.FrontalM2;
  const double driveN = car.PeakTorqueNm * car.FinalDrive / car.TyreRadiusM;
  Note("the drag area those two make", dragAreaM2, "m2");
  Note("the top speed that implies", std::sqrt(driveN / (0.5 * car.AirDensity * dragAreaM2)) * 3.6,
       "km/h");
  CHECK(std::sqrt(driveN / (0.5 * car.AirDensity * dragAreaM2)) * 3.6 > 200.0 &&
            std::sqrt(driveN / (0.5 * car.AirDensity * dragAreaM2)) * 3.6 < 280.0,
        "**AND THE TWO MUST BE MULTIPLIED, WHICH IS WHY THE COEFFICIENT IS NAMED AS ONE.** Read "
        "0.66 as a drag AREA and this car does 344 km/h; read it as the coefficient it is and "
        "multiply by 2.19 m2 and it does 233 -- which is what an F31 does. A name that lies about a "
        "unit is a defect that every other check passes over");

  CHECK(read.Views.size() == 2 && read.Views[0].Person == "first",
        "it declares a first-person view and a chase view");
  CHECK_NEAR(read.Views[0].OffsetM[1], car.SeatM[1], 1e-9, "m",
             "and the eye sits where the seat says, so the camera is not a second opinion about "
             "where the driver is");
  CHECK(read.Views[0].OffsetM[0] < 0.0,
        "**LEFT OF THE CENTRELINE**, which the asset itself decided: its front-left seat carries "
        "13 791 points against the right's 6 442, so the steering wheel is on the left");
  Note("the eye above the ground", read.Views[0].OffsetM[1], "m");
  CHECK(read.Views[0].OffsetM[1] > 1.0 && read.Views[0].OffsetM[1] < 1.4,
        "at a height a saloon's driver has -- and this one is an ESTIMATE that is settled by sitting "
        "in it and looking, which is what board:1511 says about it");

  CHECK(read.Input.size() >= 4, "and the actions a driver needs are bound by name");

  Covers("I.4.1 a vehicle is declared in the scenario the way JSBSim declares an aircraft: every "
         "quantity physical, every position measured off the asset, and the limits derived rather "
         "than tuned");
  return Report();
}
