#include "Rigging.h"

#include <cmath>

namespace outshine::Sim {

namespace {
constexpr double kContactResolutionM = 0.001;
}

namespace {

bool Refuse(Rigged &out, const std::string &why) {
  out.Stood = false;
  out.Error = why;
  return false;
}

}

Rigged Stand(const Vehicle &declared, double gravityMs2, double airDensityKgM3) {
  Rigged out;

  if (airDensityKgM3 < 0.0) {
    Refuse(out, "a world's air has a density of 0 (vacuum) or more, and this one declares " +
                    std::to_string(airDensityKgM3) + " kg/m3");
    return out;
  }
  if (!(gravityMs2 > 0.0)) {
    Refuse(out, "a rig stands in a declared world, and this world declares a gravity of " +
                    std::to_string(gravityMs2) + " m/s2 -- nothing holds a wheel to the ground");
    return out;
  }
  if (!(declared.MassKg > 0.0)) {
    Refuse(out, "a vehicle with no mass cannot be pushed by anything, and every force this engine "
                "applies divides by it");
    return out;
  }
  if (declared.Contacts.empty()) {
    Refuse(out, "a vehicle stands on 1..N contacts and this one declares none, so there is nothing "
                "for the ground to push on");
    return out;
  }
  if (declared.Contacts.size() > Physics::kMaxMounts) {
    Refuse(out, "a vehicle of " + std::to_string(declared.Contacts.size()) +
                    " contacts reaches the bound of " + std::to_string(Physics::kMaxMounts));
    return out;
  }
  if (!(declared.TyreRadiusM > 0.0)) {
    Refuse(out, "a drive torque becomes a force at a radius, and this vehicle declares none");
    return out;
  }

  for (int axis = 0; axis < 3; ++axis) {
    out.CentreM[axis] = declared.CentreOfMassM[axis];
    out.SeatM[axis] = declared.SeatM[axis];
  }

  double frontZ = 0.0, rearZ = 0.0;
  int front = 0, rear = 0;
  for (const Contact &one : declared.Contacts) {
    if (one.AtM[2] < out.CentreM[2]) {
      frontZ += one.AtM[2];
      ++front;
    } else if (one.AtM[2] > out.CentreM[2]) {
      rearZ += one.AtM[2];
      ++rear;
    }
  }
  out.Axles.WheelbaseM = front > 0 && rear > 0
                             ? std::fabs(rearZ / (double)rear - frontZ / (double)front)
                             : declared.WheelbaseM;
  if (!(out.Axles.WheelbaseM > 0.0)) {
    Refuse(out, "the vehicle '" + declared.Name +
                    "' has no wheelbase: its contacts stand on one side of the centre of "
                    "mass and it declares no wheelbaseM either -- a steering angle is the "
                    "arctangent of a wheelbase over a radius, and a wheelbase of zero is a "
                    "rig that cannot turn");
    return out;
  }
  if (declared.WheelbaseM > 0.0 &&
      std::fabs(declared.WheelbaseM - out.Axles.WheelbaseM) > kContactResolutionM) {
    Refuse(out, "the vehicle '" + declared.Name + "' declares a wheelbase of " +
                    std::to_string(declared.WheelbaseM) +
                    " m while its own contacts stand " + std::to_string(out.Axles.WheelbaseM) +
                    " m apart -- one dimension has one spelling, and the contacts are the "
                    "one the physics reads");
    return out;
  }

  if (!declared.Asset.empty()) {
    if (!(declared.AssetWheelbase > 0.0)) {
      Refuse(out, "the vehicle '" + declared.Name + "' draws '" + declared.Asset +
                      "' and declares no assetWheelbase -- a model carries no scale, so the "
                      "one dimension it is measured against must be declared beside the "
                      "dimension it is measured with");
      return out;
    }
    if (!(declared.AssetGround < 0.0)) {
      Refuse(out, "the vehicle '" + declared.Name + "' draws '" + declared.Asset +
                      "' and declares assetGround " + std::to_string(declared.AssetGround) +
                      " -- a model's lowest point stands BELOW its own origin, and without "
                      "that measurement the body is placed by the wrong point and sinks "
                      "into the ground it stands on");
      return out;
    }
    double standsAt = declared.Contacts[0].AtM[1];
    for (const Contact &one : declared.Contacts) { standsAt = std::fmin(standsAt, one.AtM[1]); }
    standsAt -= declared.TyreRadiusM;

    out.StandsAtM = standsAt;
    out.MetresPerAssetUnit = out.Axles.WheelbaseM / declared.AssetWheelbase;
    out.ModelShiftM[0] = -declared.AssetCentreX * out.MetresPerAssetUnit;
    out.ModelShiftM[1] =
        standsAt - declared.AssetGround * out.MetresPerAssetUnit - declared.CentreOfMassM[1];
    out.ModelShiftM[2] = -declared.AssetCentreZ * out.MetresPerAssetUnit;
  }

  double driven = 0.0, braked = 0.0;
  for (const Contact &one : declared.Contacts) {
    driven += one.AtM[2] > out.CentreM[2] ? 1.0 : 0.0;
    braked += 1.0;
  }
  if (!(driven > 0.0)) {
    Refuse(out, "no contact stands behind the centre of mass, so nothing can be driven -- a "
                "contact exactly on the centre plane belongs to no axle, and the declaration "
                "must place its drive axle");
    return out;
  }

  out.Rig.Count = declared.Contacts.size();
  for (size_t which = 0; which < out.Rig.Count; ++which) {
    const Contact &one = declared.Contacts[which];
    Physics::Mount &mount = out.Rig.Mounts[which];
    for (int axis = 0; axis < 3; ++axis) {
      mount.AtM[axis] = one.AtM[axis] - out.CentreM[axis];
    }
    mount.Touches.ReachM = one.ReachM;
    mount.Touches.StiffnessNPerM = one.StiffnessNPerM;
    mount.Touches.DampingNsPerM = one.DampingNsPerM;
    mount.Touches.TravelM = one.TravelM;
    mount.Touches.StopNPerM = one.StopNPerM;
    mount.Touches.LimitN = one.LimitN;
    mount.Sheds.StiffnessNPerRad = declared.CorneringNPerRad;
    mount.Sheds.RelaxationM = declared.RelaxationM;
    mount.Sheds.Friction = declared.Grip;
    mount.SteeredShare = one.AtM[2] < out.CentreM[2] ? 1.0 : 0.0;
    mount.DrivenShare = one.AtM[2] > out.CentreM[2] ? 1.0 / driven : 0.0;
    mount.BrakedShare = 1.0 / braked;
  }

  double trackM = declared.TrackM;
  if (!(trackM > 0.0)) {
    double leftX = 0.0, rightX = 0.0;
    for (const Contact &one : declared.Contacts) {
      leftX = std::fmin(leftX, one.AtM[0]);
      rightX = std::fmax(rightX, one.AtM[0]);
    }
    trackM = rightX - leftX;
  }
  if (!(declared.TurningCircleM > trackM)) {
    Refuse(out, "a steering lock is what a turning circle MEANS, and this vehicle declares a circle "
                "of " + std::to_string(declared.TurningCircleM) +
                    " m against a track of " + std::to_string(trackM) +
                    " m -- a circle no wider than the car is not one it can drive");
    return out;
  }
  out.Axles.SteerLimitRad =
      std::atan(out.Axles.WheelbaseM / (0.5 * declared.TurningCircleM - 0.5 * trackM));

  const double outerM = 0.5 * declared.TurningCircleM;
  if (!(outerM > out.Axles.WheelbaseM)) {
    Refuse(out, "the turning circle's half of " + std::to_string(outerM) +
                    " m is no longer than the wheelbase of " +
                    std::to_string(out.Axles.WheelbaseM) +
                    " m -- the rear axle would stand outside its own circle, and the "
                    "tightest centreline radius that geometry implies is not a number");
    return out;
  }
  out.TightestM =
      std::sqrt(outerM * outerM - out.Axles.WheelbaseM * out.Axles.WheelbaseM);

  out.Envelope.Grip = declared.Grip;
  out.Envelope.GravityMs2 = gravityMs2;
  out.Envelope.MassKg = declared.MassKg;
  out.Envelope.DriveN = declared.PeakTorqueNm * declared.FinalDrive / declared.TyreRadiusM;
  out.Envelope.BrakeN = declared.BrakeTorqueNm / declared.TyreRadiusM;
  if (!(declared.DragCoefficient > 0.0) || !(declared.FrontalM2 > 0.0)) {
    Refuse(out, "the vehicle '" + declared.Name + "' declares a drag coefficient of " +
                    std::to_string(declared.DragCoefficient) + " over a frontal area of " +
                    std::to_string(declared.FrontalM2) +
                    " m2 -- a body moving through declared air has a shape, and a speed plan "
                    "solved without one has no top speed to be bounded by");
    return out;
  }
  out.Envelope.DragArea = declared.DragCoefficient * declared.FrontalM2;
  out.Envelope.AirDensity = airDensityKgM3;

  if (!(out.Envelope.BrakeMs2() > 0.0)) {
    Refuse(out, "a vehicle that cannot slow cannot drive a corridor -- brakes and grip "
                "together yield " + std::to_string(out.Envelope.BrakeMs2()) +
                " m/s2, and the tick would divide by it");
    return out;
  }

  out.Stood = true;
  return out;
}

}
