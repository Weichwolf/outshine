#include "Rig.h"

#include <cmath>

namespace outshine::Physics {

namespace {

void Cross(const double a[3], const double b[3], double out[3]) {
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}

double Dot(const double a[3], const double b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

bool Unit(double v[3]) {
  const double length = std::sqrt(Dot(v, v));
  if (!(length > 0.0)) { return false; }
  for (int axis = 0; axis < 3; ++axis) { v[axis] /= length; }
  return true;
}

} // namespace

Reading Bear(Rig &of, const Body &body, const Footing *under, const Controls &with, Wrench &into,
             double dtS) {
  Reading out;
  out.Count = of.Count < kMaxMounts ? of.Count : kMaxMounts;

  for (size_t which = 0; which < out.Count; ++which) {
    const Mount &mount = of.Mounts[which];
    const Footing &ground = under[which];
    if (!ground.Found) {
      ++out.Airborne;
      of.HeldSlipRad[which] = 0.0;
      continue;
    }

    double normal[3] = {ground.NormalM[0], ground.NormalM[1], ground.NormalM[2]};
    if (!Unit(normal)) { continue; }

    double worldM[3], worldMs[3];
    Place(body, mount.AtM, worldM);
    Carry(body, mount.AtM, worldMs);

    const double clearanceM = (worldM[1] - ground.HeightM) * normal[1];
    const double closingMs = -Dot(worldMs, normal);
    const Reaction against = Press(mount.Touches, clearanceM, closingMs);

    out.Touching[which] = against.Touching;
    out.LoadN[which] = against.LoadN;
    out.PressedM[which] = against.PressedM;
    out.PastTravel = out.PastTravel || against.PastTravel;
    out.PastLimit = out.PastLimit || against.PastLimit;
    out.HeaviestN = std::fmax(out.HeaviestN, against.LoadN);
    if (!against.Touching) {
      ++out.Airborne;
      of.HeldSlipRad[which] = 0.0;
      continue;
    }

    const double steerRad = with.SteerRad * mount.SteeredShare;
    const double aheadBody[3] = {-std::sin(steerRad), 0.0, -std::cos(steerRad)};
    double ahead[3];
    Turn(body.OrientationQ, aheadBody, ahead);
    const double onNormal = Dot(ahead, normal);
    for (int axis = 0; axis < 3; ++axis) { ahead[axis] -= onNormal * normal[axis]; }
    if (!Unit(ahead)) { continue; }
    double across[3];
    Cross(normal, ahead, across);
    if (!Unit(across)) { continue; }

    const double alongMs = Dot(worldMs, ahead);
    const double acrossMs = Dot(worldMs, across);
    const double rollingMs = std::fabs(alongMs);
    const double rawSlipRad = rollingMs > 0.0 ? std::atan2(-acrossMs, rollingMs) : 0.0;
    const double rolledM = rollingMs * dtS;
    of.HeldSlipRad[which] =
        Relaxed(mount.Sheds, of.HeldSlipRad[which], rawSlipRad, rolledM);

    double askedAlongN = with.DriveN * mount.DrivenShare;
    const double brakingN = with.BrakeN * mount.BrakedShare;
    if (brakingN > 0.0) { askedAlongN -= alongMs >= 0.0 ? brakingN : -brakingN; }

    const Shear shed = ShedAt(mount.Sheds, against.LoadN, of.HeldSlipRad[which], askedAlongN);
    out.SlipRad[which] = of.HeldSlipRad[which];
    out.RatioOfHold[which] = shed.Ratio;
    out.Sliding = out.Sliding || shed.Sliding;
    out.WorstSlipRad = std::fmax(out.WorstSlipRad, std::fabs(of.HeldSlipRad[which]));
    out.WorstRatio = std::fmax(out.WorstRatio, shed.Ratio);

    double forceN[3];
    for (int axis = 0; axis < 3; ++axis) {
      forceN[axis] = normal[axis] * against.LoadN + ahead[axis] * shed.AlongN +
                     across[axis] * shed.AcrossN;
    }

    double normalBody[3];
    Unturn(body.OrientationQ, normal, normalBody);
    const double standOffM = mount.Touches.ReachM - against.PressedM;
    double patchM[3];
    for (int axis = 0; axis < 3; ++axis) {
      patchM[axis] = mount.AtM[axis] - normalBody[axis] * standOffM;
    }
    Push(into, body, patchM, forceN);
  }
  return out;
}

void Resist(Wrench &into, const Body &body, double dragArea, double mediumDensity) {
  const double speedMs = std::sqrt(Dot(body.VelocityMs, body.VelocityMs));
  if (!(speedMs > 0.0) || !(dragArea > 0.0) || !(mediumDensity > 0.0)) { return; }
  const double dragN = 0.5 * mediumDensity * dragArea * speedMs * speedMs;
  for (int axis = 0; axis < 3; ++axis) {
    into.ForceN[axis] -= dragN * body.VelocityMs[axis] / speedMs;
  }
}

} // namespace outshine::Physics
