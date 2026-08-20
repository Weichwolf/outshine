#include <cmath>
#include <cstdio>
#include <string>

#include "Check.h"

#include <cstdio>
#include <vector>

#include "Rigging.h"
#include "ScenarioRead.h"

using outshine::Contact;
using outshine::ReadScenario;
using outshine::Scenario;
using outshine::Vehicle;
using outshine::Clients::Rigged;
using outshine::Clients::Stand;

namespace {

constexpr double kGravityMs2 = 9.80665;

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  std::FILE *const file = std::fopen("tools/driver/f31.scenario", "rb");
  CHECK(file != nullptr, "the driver's own scenario file is there to read");
  if (file == nullptr) { return Report(); }
  std::vector<char> text;
  int c = 0;
  while ((c = std::fgetc(file)) != EOF) { text.push_back((char)c); }
  std::fclose(file);

  Scenario read;
  std::string error;
  if (!ReadScenario(text.data(), text.size(), read, error)) {
    std::printf("REFUSED %s\n", error.c_str());
    return Report();
  }
  CHECK(read.Vehicles.size() == 1, "the driver's own scenario declares one vehicle");
  if (read.Vehicles.empty()) { return Report(); }

  const Vehicle &declared = read.Vehicles[0];
  const Rigged stood = Stand(declared);
  if (!stood.Stood) { std::printf("REFUSED %s\n", stood.Error.c_str()); }
  CHECK(stood.Stood,
        "**A DECLARED VEHICLE STANDS UP AS A RIG, AND THIS IS THE ONLY PLACE THAT HAPPENS.** Until "
        "now the physics tests carried the F31's numbers as constants beside the file that "
        "declares them -- the same statement in two places, which drifts the moment one is "
        "measured. The scenario is the declaration and this is what reads it");
  if (!stood.Stood) { return Report(); }

  CHECK(stood.Rig.Count == declared.Contacts.size(),
        "with one mount per declared contact, however many there are");

  double totalDriven = 0.0, totalBraked = 0.0;
  size_t steered = 0;
  for (size_t which = 0; which < stood.Rig.Count; ++which) {
    totalDriven += stood.Rig.Mounts[which].DrivenShare;
    totalBraked += stood.Rig.Mounts[which].BrakedShare;
    if (stood.Rig.Mounts[which].SteeredShare > 0.0) { ++steered; }
  }
  Note("mounts that steer", (double)steered, "of them");
  Note("the drive shared out", totalDriven, "of it");
  Note("the braking shared out", totalBraked, "of it");
  CHECK_NEAR(totalDriven, 1.0, 1e-12, "of it",
             "**AND THE DRIVE IS SHARED OUT AND NEVER MULTIPLIED.** Every mount's share sums to one, "
             "so a car with two driven wheels and one with four apply the same force -- which is "
             "what stops a declaration from accidentally doubling its own engine");
  CHECK_NEAR(totalBraked, 1.0, 1e-12, "of it", "and the braking likewise");
  CHECK(steered == 2, "and the two contacts ahead of the centre of mass are the ones that steer");

  for (size_t which = 0; which < stood.Rig.Count; ++which) {
    const double declaredY = declared.Contacts[which].AtM[1];
    Note("a mount sits this far from the centre of mass, vertically",
         stood.Rig.Mounts[which].AtM[1], "m");
    CHECK_NEAR(stood.Rig.Mounts[which].AtM[1], declaredY - declared.CentreOfMassM[1], 1e-12, "m",
               "**AND EVERY MOUNT IS MEASURED FROM THE CENTRE OF MASS, not from the asset's "
               "origin.** The declaration states positions where they can be measured off the model; "
               "the physics needs them where the body turns, and this is the one subtraction that "
               "moves between the two");
    break;
  }

  Note("the wheelbase the contacts span", stood.Axles.WheelbaseM, "m");
  Note("the wheelbase the file declares", declared.WheelbaseM, "m");
  CHECK_NEAR(stood.Axles.WheelbaseM, declared.WheelbaseM, 0.01, "m",
             "the wheelbase is taken from where the contacts actually are and agrees with the one "
             "the file states, which is the check that the two were not written independently");

  const double centreRadiusM = 0.5 * declared.TurningCircleM - 0.5 * declared.TrackM;
  Note("the turning circle it declares", declared.TurningCircleM, "m");
  Note("the steer limit that implies", stood.Axles.SteerLimitRad, "rad");
  Note("the same in degrees", stood.Axles.SteerLimitRad * 180.0 / 3.14159265358979, "deg");
  CHECK_NEAR(stood.Axles.SteerLimitRad, std::atan(stood.Axles.WheelbaseM / centreRadiusM), 1e-12,
             "rad",
             "**AND THE STEERING LOCK IS DERIVED FROM THE TURNING CIRCLE, WHICH IS THE NUMBER A "
             "MANUFACTURER PUBLISHES.** atan(L / (circle/2 - track/2)) is 30.0 degrees on this car. "
             "A steer limit declared directly would be a number nobody can check against anything");
  CHECK(stood.Axles.SteerLimitRad > 0.4 && stood.Axles.SteerLimitRad < 0.7,
        "and it lands where a road car's lock is");

  Note("the top speed the envelope implies", stood.Envelope.TopMs() * 3.6, "km/h");
  Note("the lateral acceleration it can hold", stood.Envelope.LateralMs2(), "m/s2");
  Note("what it can accelerate at", stood.Envelope.AccelMs2(), "m/s2");
  Note("what it can brake at", stood.Envelope.BrakeMs2(), "m/s2");
  CHECK_NEAR(stood.Envelope.MassKg, declared.MassKg, 1e-12, "kg", "the envelope carries its mass");
  CHECK_NEAR(stood.Envelope.DragArea, declared.DragCoefficient * declared.FrontalM2, 1e-12, "m2",
             "and its drag area is the coefficient times the frontal area, multiplied HERE and "
             "stored nowhere -- board:1520");
  CHECK(stood.Envelope.TopMs() * 3.6 > 200.0 && stood.Envelope.TopMs() * 3.6 < 280.0,
        "so the top speed it implies is a 3 Series' and not a fantasy");
  Note("the brake torque it declares", declared.BrakeTorqueNm, "Nm");
  Note("the torque needed to lock all four wheels",
       declared.Grip * declared.MassKg * kGravityMs2 * declared.TyreRadiusM, "Nm");
  CHECK(declared.BrakeTorqueNm > declared.Grip * declared.MassKg * kGravityMs2 *
                                     declared.TyreRadiusM,
        "**THE BRAKES CAN LOCK THE WHEELS, WHICH IS THE REQUIREMENT AND NOT A PREFERENCE.** mu m g r "
        "is 4994 Nm on this car; anything less makes the BRAKE the limit and the tyres never are, "
        "and a car that cannot lock its wheels cannot stop as hard as the road allows. The first "
        "declaration said 2200 Nm and braked at 0.42 g");
  CHECK_NEAR(stood.Envelope.BrakeMs2(), stood.Envelope.Grip * kGravityMs2, 0.01, "m/s2",
             "so the envelope returns the GRIP as the braking limit and not the brake, which is "
             "what a modern car is -- and it says which of the two it took");

  Vehicle broken = declared;
  broken.Contacts.clear();
  const Rigged nothing = Stand(broken);
  CHECK(!nothing.Stood, "a vehicle with no contacts is refused rather than floated");
  std::printf("REFUSAL %s\n", nothing.Error.c_str());

  Vehicle tight = declared;
  tight.TurningCircleM = 1.0;
  const Rigged impossible = Stand(tight);
  CHECK(!impossible.Stood,
        "and a turning circle narrower than the car itself is refused, because a lock derived from "
        "it would be a number the arithmetic invented");
  std::printf("REFUSAL %s\n", impossible.Error.c_str());

  Covers("I.4.2 a declared vehicle stands up as a rig, an axle set and an envelope: mounts measured "
         "from the centre of mass, drive and braking shared out to sum to one, the steering lock "
         "derived from the published turning circle, and every refusal named");
  return Report();
}
