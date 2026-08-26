#include <cmath>
#include <cstdio>

#include <Scenario.h>

#include "Check.h"
#include "Rigging.h"

namespace {

// The oracle is that a seat is INSIDE the car, and it does not depend on our design.
//
// A declaration measures its vehicle from the ground the wheels stand on: the contacts sit at
// y = 0.333 because that is a wheel centre's height, the centre of mass at y = 0.550, the seat
// at y = 1.220. Every one of those is a height above the road.
//
// The BODY, though, has its origin at the centre of mass -- Rigging subtracts `CentreM` from
// every mount for exactly that reason. So a point declared from the ground reaches the body
// frame only after the same subtraction:
//
//   seat in the body frame = seat as declared - centre of mass as declared
//
// Skip it and the camera rides `CentreM[1]` too high. For the F31 that is 0.550 m, which puts
// the eye above a roof that stands 1.45 m off the road -- and the picture is the car's roof
// from outside instead of its cabin from within.
//
// The check is the physical statement and not the arithmetic: the seat, expressed in the frame
// the body actually uses, must lie inside the box the declaration draws around itself.
constexpr double kGravityMs2 = 9.80665;
constexpr double kAirDensityKgM3 = 1.225;
constexpr double kRoofM = 1.45;

[[nodiscard]] outshine::Contact Standing(double xM, double zM) {
  outshine::Contact one;
  one.At = zM < 0.0 ? "front" : "rear";
  one.AtM[0] = xM;
  one.AtM[1] = 0.333;
  one.AtM[2] = zM;
  one.ReachM = 0.45635;
  one.StiffnessNPerM = 32000.0;
  one.DampingNsPerM = 3400.0;
  one.TravelM = 0.18;
  one.StopNPerM = 450000.0;
  one.LimitN = 24000.0;
  return one;
}

[[nodiscard]] outshine::Vehicle F31(void) {
  outshine::Vehicle made;
  made.Name = "f31";
  made.MassKg = 1610.0;
  made.WidthM = 1.811;
  made.CentreOfMassM[1] = 0.55;
  made.InertiaKgM2[0] = 540.0;
  made.InertiaKgM2[1] = 2400.0;
  made.InertiaKgM2[2] = 2600.0;
  made.Contacts.push_back(Standing(-0.774, -1.405));
  made.Contacts.push_back(Standing(0.774, -1.405));
  made.Contacts.push_back(Standing(-0.774, 1.405));
  made.Contacts.push_back(Standing(0.774, 1.405));
  made.Grip = 0.95;
  made.TyreRadiusM = 0.333;
  made.CorneringNPerRad = 55000.0;
  made.RelaxationM = 0.4;
  made.TurningCircleM = 11.3;
  made.PeakTorqueNm = 400.0;
  made.FinalDrive = 3.08;
  made.BrakeTorqueNm = 5500.0;
  made.DragCoefficient = 0.66;
  made.FrontalM2 = 2.19;
  made.AssetWheelbase = 180.71;
  made.SeatAt = "driver";
  made.SeatM[0] = -0.494;
  made.SeatM[1] = 1.220;
  made.SeatM[2] = 0.003;
  return made;
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const outshine::Vehicle declared = F31();
  const outshine::Sim::Rigged stood =
      outshine::Sim::Stand(declared, kGravityMs2, kAirDensityKgM3);
  CHECK(stood.Stood, "the declaration stands, so its frames can be compared");
  if (!stood.Stood) { return Report(); }

  const double halfWidthM = 0.5 * declared.WidthM;
  const double halfLengthM = 0.5 * stood.Axles.WheelbaseM;

  const double asDeclared[3] = {declared.SeatM[0], declared.SeatM[1], declared.SeatM[2]};
  const double inTheBody[3] = {asDeclared[0] - stood.CentreM[0], asDeclared[1] - stood.CentreM[1],
                               asDeclared[2] - stood.CentreM[2]};

  std::printf("THE CENTRE OF MASS the declaration puts at (%.3f, %.3f, %.3f) m\n", stood.CentreM[0],
              stood.CentreM[1], stood.CentreM[2]);
  std::printf("THE SEAT as declared, from the road:  (%.3f, %.3f, %.3f) m\n", asDeclared[0],
              asDeclared[1], asDeclared[2]);
  std::printf("THE SEAT in the body's own frame:     (%.3f, %.3f, %.3f) m\n", inTheBody[0],
              inTheBody[1], inTheBody[2]);
  std::printf("THE ROOF stands %.3f m off the road, so %.3f m above the body's origin\n", kRoofM,
              kRoofM - stood.CentreM[1]);

  CHECK(inTheBody[1] < kRoofM - stood.CentreM[1],
        "**A SEAT IS UNDER THE ROOF**: the seat expressed in the frame the body actually uses "
        "must lie below the roof in that same frame, or a camera declared at the seat looks "
        "down on the car it is supposed to be sitting in");
  CHECK(std::fabs(inTheBody[0]) < halfWidthM,
        "and inside the car's width, because a seat is not outside the door");
  CHECK(std::fabs(inTheBody[2]) < halfLengthM,
        "and between the axles, because a seat is not on the boot lid");

  // The control, and it is the defect this case was written against: WITHOUT the subtraction the
  // very same seat is above the roof, and every one of the three statements above is false of it.
  CHECK(asDeclared[1] > kRoofM - stood.CentreM[1],
        "and the control is a control: the seat taken RAW into the body frame -- the centre of "
        "mass not subtracted -- stands ABOVE the roof in that frame, so this case can tell the "
        "two readings apart rather than passing on either");

  Note("how far the raw reading puts the eye above the roof", asDeclared[1] - (kRoofM - stood.CentreM[1]),
       "m");
  Note("and the subtraction that fixes it", stood.CentreM[1], "m");

  Covers("physics: a point a declaration measures from the road reaches the body's frame only "
         "after its centre of mass is subtracted, so a seat stays inside the car it is in");
  return Report();
}
