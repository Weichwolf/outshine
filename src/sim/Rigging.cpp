#include "Rigging.h"

#include <cmath>
#include <string>
#include <cstddef>

namespace outshine::Sim {

namespace {}

namespace {

bool Refuse(Rigged &out, const std::string &why) {
  out.Stood = false;
  out.Error = why;
  return false;
}

} // namespace

Rigged Stand(const Scenario::Body &declared, double gravityMs2, double airDensityKgM3) {
  Rigged out;

  if (airDensityKgM3 < 0.0) {
    Refuse(out,
           "a world's air has a density of 0 (vacuum) or more, and this one declares " +
               std::to_string(airDensityKgM3) + " kg/m3");
    return out;
  }
  if (!(gravityMs2 > 0.0)) {
    Refuse(out,
           "a rig stands in a declared world, and this world declares a gravity of " +
               std::to_string(gravityMs2) + " m/s2 -- nothing holds a wheel to the ground");
    return out;
  }
  if (!(declared.MassKg > 0.0)) {
    Refuse(out,
           "a body with no mass cannot be pushed by anything, and every force this engine "
           "applies divides by it");
    return out;
  }
  if (declared.Contacts.empty()) {
    Refuse(out,
           "a body stands on 1..N contacts and this one declares none, so there is nothing "
           "for the ground to push on");
    return out;
  }
  if (declared.Contacts.size() > Physics::kMaxMounts) {
    Refuse(out,
           "a body of " + std::to_string(declared.Contacts.size()) +
               " contacts reaches the bound of " + std::to_string(Physics::kMaxMounts));
    return out;
  }
  if (!(declared.Contacts.front().Touches.RadiusM > 0.0)) {
    Refuse(out, "a drive torque becomes a force at a radius, and this body declares none");
    return out;
  }

  for (int axis = 0; axis < 3; ++axis) {
    out.CentreM[axis] = declared.CentreOfMassM[axis];
    out.SeatM[axis] = declared.Slots.empty() ? 0.0 : declared.Slots.front().AtM[axis];
  }

  out.Axles.WheelbaseM = declared.spanM();
  if (!(out.Axles.WheelbaseM > 0.0)) {
    Refuse(out,
           "the body '" + declared.Name +
               "' has no wheelbase: its contacts stand on one side of the centre of "
               "mass, and a steering angle is the arctangent of a span over a radius, "
               "so a span of zero is a rig that cannot turn");
    return out;
  }

  if (!declared.Asset.empty()) {
    if (!(declared.AssetSpanM > 0.0)) {
      Refuse(out,
             "the body '" + declared.Name + "' draws '" + declared.Asset +
                 "' and declares no assetSpanM -- a model carries no scale, so the "
                 "one dimension it is measured against must be declared beside the "
                 "dimension it is measured with");
      return out;
    }
    if (!(declared.AssetGround < 0.0)) {
      Refuse(out,
             "the body '" + declared.Name + "' draws '" + declared.Asset +
                 "' and declares assetGround " + std::to_string(declared.AssetGround) +
                 " -- a model's lowest point stands BELOW its own origin, and without "
                 "that measurement the body is placed by the wrong point and sinks "
                 "into the ground it stands on");
      return out;
    }
    double standsAt = declared.Contacts[0].AtM[1];
    for (const Scenario::Contact &one : declared.Contacts) {
      standsAt = std::fmin(standsAt, one.AtM[1]);
    }
    standsAt -= declared.Contacts.front().Touches.RadiusM;

    out.StandsAtM = standsAt;
    out.MetresPerAssetUnit = out.Axles.WheelbaseM / declared.AssetSpanM;
    out.ModelShiftM[0] = -declared.AssetCentreX * out.MetresPerAssetUnit;
    out.ModelShiftM[1] =
        standsAt - declared.AssetGround * out.MetresPerAssetUnit - declared.CentreOfMassM[1];
    out.ModelShiftM[2] = -declared.AssetCentreZ * out.MetresPerAssetUnit;
  }

  double driven = 0.0;
  double frontMounts = 0.0;
  double rearMounts = 0.0;
  double frontArmM = 0.0;
  double rearArmM = 0.0;
  for (const Scenario::Contact &one : declared.Contacts) {
    const double armM = one.AtM[2] - out.CentreM[2];
    driven += armM > 0.0 ? 1.0 : 0.0;
    if (armM < 0.0) {
      frontMounts += 1.0;
      frontArmM += -armM;
    } else if (armM > 0.0) {
      rearMounts += 1.0;
      rearArmM += armM;
    }
  }
  if (frontMounts > 0.0) { frontArmM /= frontMounts; }
  if (rearMounts > 0.0) { rearArmM /= rearMounts; }
  const double axleSpanM = frontArmM + rearArmM;
  if (!(driven > 0.0)) {
    Refuse(out,
           "no contact stands behind the centre of mass, so nothing can be driven -- a "
           "contact exactly on the centre plane belongs to no axle, and the declaration "
           "must place its drive axle");
    return out;
  }
  if (!(axleSpanM > 0.0) || !(frontMounts > 0.0)) {
    Refuse(out,
           "no contact stands ahead of the centre of mass, so the declaration names no "
           "front axle and the static load it carries cannot be found");
    return out;
  }
  const double frontLoadShare = rearArmM / axleSpanM;

  out.Rig.Count = declared.Contacts.size();
  for (size_t which = 0; which < out.Rig.Count; ++which) {
    const Scenario::Contact &one = declared.Contacts[which];
    Physics::Mount &mount = out.Rig.Mounts[which];
    for (int axis = 0; axis < 3; ++axis) { mount.AtM[axis] = one.AtM[axis] - out.CentreM[axis]; }
    mount.Strut.ReachM = one.Strut.ReachM;
    mount.Strut.StiffnessNPerM = one.Strut.StiffnessNPerM;
    mount.Strut.DampingNsPerM = one.Strut.DampingNsPerM;
    mount.Strut.TravelM = one.Strut.TravelM;
    mount.Strut.StopNPerM = one.Strut.StopNPerM;
    mount.Strut.LimitN = one.Strut.LimitN;
    mount.Sheds.CorneringNPerRad = one.Touches.CorneringNPerRad;
    mount.Sheds.RelaxationM = one.Touches.RelaxationM;
    mount.Sheds.Grip = one.Touches.Grip;
    mount.Sheds.LoadFalloff = one.Touches.LoadFalloff;
    const double armM = one.AtM[2] - out.CentreM[2];
    const double staticShare =
        armM < 0.0 ? frontLoadShare / frontMounts : (1.0 - frontLoadShare) / rearMounts;
    mount.Sheds.FrictionAtLoadN = declared.MassKg * gravityMs2 * staticShare;
    mount.Steering.Applied.Ratio = one.AtM[2] < out.CentreM[2] ? 1.0 : 0.0;
    mount.Spin.Applied.Ratio = one.AtM[2] > out.CentreM[2] ? 1.0 / driven : 0.0;
    mount.Spin.Resisting.Ratio = staticShare;
  }

  const double acrossM = declared.acrossM();
  const Scenario::Drive *const steers = declared.can(Scenario::Drives::Motion);
  const double circleM = steers != nullptr ? steers->CircleM : 0.0;
  if (!(circleM > acrossM)) {
    Refuse(out,
           "a steering lock is what a turning circle MEANS, and this body declares a circle "
           "of " +
               std::to_string(circleM) + " m against a stance of " + std::to_string(acrossM) +
               " m -- a circle no wider than the body is not one "
               "it can turn in");
    return out;
  }
  out.Axles.SteerLimitRad = std::atan(out.Axles.WheelbaseM / (0.5 * circleM - 0.5 * acrossM));

  const double outerM = 0.5 * circleM;
  if (!(outerM > out.Axles.WheelbaseM)) {
    Refuse(out,
           "the turning circle's half of " + std::to_string(outerM) +
               " m is no longer than the span of " + std::to_string(out.Axles.WheelbaseM) +
               " m -- the rear axle would stand outside its own circle, and the tightest "
               "centreline radius that geometry implies is not a number");
    return out;
  }
  out.TightestM = std::sqrt(outerM * outerM - out.Axles.WheelbaseM * out.Axles.WheelbaseM);

  {
    const double heaviestN = declared.MassKg * gravityMs2 *
                             (frontLoadShare / frontMounts > (1.0 - frontLoadShare) / rearMounts
                                  ? frontLoadShare / frontMounts
                                  : (1.0 - frontLoadShare) / rearMounts);
    Physics::Shearing planning;
    planning.Grip = declared.Contacts.front().Touches.Grip;
    planning.LoadFalloff = declared.Contacts.front().Touches.LoadFalloff;
    planning.FrictionAtLoadN = heaviestN;
    out.Envelope.Grip = Physics::FrictionAt(planning, heaviestN);
  }
  out.Envelope.GravityMs2 = gravityMs2;
  out.Envelope.MassKg = declared.MassKg;
  const Scenario::Drive *const drives = declared.efforts(false);
  const Scenario::Drive *const brakes = declared.efforts(true);
  out.Envelope.DriveN = drives == nullptr ? 0.0
                                          : drives->PeakNm * drives->Ratio /
                                                declared.Contacts.front().Touches.RadiusM;
  out.Envelope.BrakeN =
      brakes == nullptr ? 0.0 : brakes->PeakNm / declared.Contacts.front().Touches.RadiusM;
  if (!(declared.DragCoefficient > 0.0) || !(declared.FrontalM2 > 0.0)) {
    Refuse(out,
           "the body '" + declared.Name + "' declares a drag coefficient of " +
               std::to_string(declared.DragCoefficient) + " over a frontal area of " +
               std::to_string(declared.FrontalM2) +
               " m2 -- a body moving through declared air has a shape, and a speed plan "
               "solved without one has no top speed to be bounded by");
    return out;
  }
  out.Envelope.DragArea = declared.DragCoefficient * declared.FrontalM2;
  out.Envelope.AirDensity = airDensityKgM3;

  if (!(out.Envelope.BrakeMs2() > 0.0)) {
    Refuse(out,
           "a body that cannot slow cannot drive a corridor -- brakes and grip "
           "together yield " +
               std::to_string(out.Envelope.BrakeMs2()) + " m/s2, and the tick would divide by it");
    return out;
  }

  out.Stood = true;
  return out;
}

} // namespace outshine::Sim
