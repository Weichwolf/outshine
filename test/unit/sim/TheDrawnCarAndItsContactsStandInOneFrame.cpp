#include <cmath>
#include <cstdio>
#include <string>

#include "Check.h"

#include "Rigging.h"

using outshine::Contact;
using outshine::Vehicle;
using outshine::Sim::Rigged;
using outshine::Sim::Stand;

namespace {

[[nodiscard]] Vehicle F31() {
  Vehicle made;
  made.Name = "f31";
  made.Asset = "scene.gltf";
  made.MassKg = 1610.0;
  made.WidthM = 1.811;
  made.WheelbaseM = 2.810;
  made.AssetWheelbase = 180.71;
  made.AssetGround = -60.939;
  made.AssetCentreX = 60.104;
  made.AssetCentreZ = 22.847;
  made.CentreOfMassM[1] = 0.55;
  made.TyreRadiusM = 0.333;
  made.Grip = 1.0;
  made.TurningCircleM = 11.3;
  made.TrackM = 1.548;
  made.PeakTorqueNm = 400.0;
  made.FinalDrive = 3.15;
  made.BrakeTorqueNm = 3000.0;
  const double corners[4][3] = {{-0.774, 0.333, -1.405},
                                {0.774, 0.333, -1.405},
                                {-0.774, 0.333, 1.405},
                                {0.774, 0.333, 1.405}};
  for (const auto &at : corners) {
    Contact one;
    one.AtM[0] = at[0];
    one.AtM[1] = at[1];
    one.AtM[2] = at[2];
    one.ReachM = 0.45635;
    one.StiffnessNPerM = 32000.0;
    one.DampingNsPerM = 3400.0;
    one.TravelM = 0.18;
    one.StopNPerM = 450000.0;
    one.LimitN = 24000.0;
    made.Contacts.push_back(one);
  }
  return made;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const Rigged stood = Stand(F31(), 9.80665, 1.225);
  if (!stood.Stood) { std::printf("REFUSED %s\n", stood.Error.c_str()); }
  CHECK(stood.Stood, "the declared F31 stands");
  if (!stood.Stood) { return Report(); }

  // board:1551 derived the scale from the ONE dimension measured off the asset's own tyre
  // material: 2.810 m of wheelbase over 180.71 units.
  CHECK_NEAR(stood.MetresPerAssetUnit, 2.810 / 180.71, 1e-12, "m/unit",
             "the model carries no scale, so the engine derives it from the declared wheelbase");

  // the contacts define the ground the car stands on: a hub at 0.333 m over a tyre of
  // 0.333 m radius touches at 0.
  CHECK_NEAR(stood.StandsAtM, 0.0, 1e-12, "m",
             "**THE CONTACTS DEFINE THE STANDING PLANE**, never an assumed zero");

  // the shift carries the model's lowest point onto that plane: the body was placed by its
  // own origin and sat 0.398 m INSIDE the road, deeper than the carriageway is thick.
  const double lowestM =
      F31().CentreOfMassM[1] + F31().AssetGround * stood.MetresPerAssetUnit + stood.ModelShiftM[1];
  Note("where the body's lowest point is drawn", lowestM, "m");
  Note("how far the model must rise", stood.ModelShiftM[1], "m");
  CHECK_NEAR(lowestM, stood.StandsAtM, 1e-12, "m",
             "**THE DRAWN BODY AND THE CONTACTS STAND IN ONE FRAME**: the model's lowest "
             "point lands exactly on the plane its own contacts touch");
  CHECK_NEAR(stood.ModelShiftM[1], 0.397588, 1e-6, "m",
             "and the lift is the 0.398 m the body was sunk by (board:1554)");
  CHECK_NEAR(stood.ModelShiftM[0], -0.934604, 1e-6, "m",
             "the lateral shift carries the model's own centroid onto the reference point: "
             "the bounding box runs -0.097..+1.967 m about the origin, so the body lies "
             "almost entirely to ONE side of the point it was placed by");
  CHECK_NEAR(stood.ModelShiftM[2], -0.355266, 1e-6, "m",
             "and the longitudinal shift likewise");

  {
    Vehicle mute = F31();
    mute.AssetWheelbase = 0.0;
    const Rigged refused = Stand(mute, 9.80665, 1.225);
    CHECK(!refused.Stood && refused.Error.find("assetWheelbase") != std::string::npos,
          "**AN ASSET THAT DOES NOT SAY WHAT IT MEASURES CANNOT BE DRAWN**: no scale, no "
          "placement, and the refusal names the missing dimension");
  }
  {
    Vehicle unscaled = F31();
    unscaled.WheelbaseM = 0.0;
    const Rigged refused = Stand(unscaled, 9.80665, 1.225);
    CHECK(!refused.Stood && refused.Error.find("wheelbaseM") != std::string::npos,
          "**THE SCALE DIVIDES BY A DECLARED WHEELBASE, SO IT MUST BE DECLARED**: the "
          "physics reads the wheelbase off the contacts and would never notice, and a model "
          "drawn at a scale of zero is a car nobody can see (board:1771)");
  }
  {
    Vehicle blind = F31();
    blind.AssetGround = 0.0;
    const Rigged refused = Stand(blind, 9.80665, 1.225);
    CHECK(!refused.Stood && refused.Error.find("assetGround") != std::string::npos,
          "and an asset that does not say where its own ground is refuses too -- without it "
          "the body is placed by the wrong point and sinks into the road");
  }
  {
    Vehicle lifted = F31();
    for (Contact &one : lifted.Contacts) { one.AtM[1] = 0.5; }
    const Rigged stoodHigh = Stand(lifted, 9.80665, 1.225);
    CHECK(stoodHigh.Stood, "a vehicle whose hubs sit above the tyre radius still stands");
    CHECK_NEAR(stoodHigh.StandsAtM, 0.167, 1e-12, "m",
               "and its standing plane follows the contacts it declares");
    CHECK_NEAR(stoodHigh.ModelShiftM[1] - stood.ModelShiftM[1], 0.167, 1e-12, "m",
               "so the drawn body rises with them -- the frame is the contacts', never a "
               "constant the engine assumed");
  }

  Covers("V.7 the drawn body and the contacts stand in ONE frame: the engine derives the "
         "model's shift from the declaration and refuses an asset that does not say where "
         "its own ground is (board:1554)");
  return Report();
}
