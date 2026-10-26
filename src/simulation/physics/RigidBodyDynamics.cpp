#include "Quat.h"
#include "RigidBodyDynamics.h"

#include <cmath>

#include "Vec3.h"

namespace outshine::Physics {

using outshine::Cross;

void RotateVector(const Quat &orientationQ, const Vec3 &bodyV, Vec3 &worldV) {
  const Vec3 q = {{orientationQ.X, orientationQ.Y, orientationQ.Z}};
  const Vec3 qv = Cross(q, bodyV);
  const Vec3 twice = qv * 2.0;
  const Vec3 qqv = Cross(q, twice);
  worldV = bodyV + twice * orientationQ.W + qqv;
}

void InverseRotateVector(const Quat &orientationQ, const Vec3 &worldV, Vec3 &bodyV) {
  const Quat back = {
      .X = -orientationQ.X, .Y = -orientationQ.Y, .Z = -orientationQ.Z, .W = orientationQ.W};
  RotateVector(back, worldV, bodyV);
}

void AccumulateForce(Wrench &wrench, const World::RigidBodyState &body, const Force &applied) {
  Vec3 arm;
  RotateVector(body.OrientationQ, applied.AtBodyM, arm);
  const Vec3 torque = Cross(arm, applied.Newtons);
  wrench.ForceN = wrench.ForceN + applied.Newtons;
  wrench.TorqueNm = wrench.TorqueNm + torque;
}

void AccumulateGravity(Wrench &wrench, const World::RigidBodyState &body, const Vec3 &gravityMs2) {
  for (int axis = 0; axis < 3; ++axis) { wrench.ForceN[axis] += body.MassKg * gravityMs2[axis]; }
}

void IntegrateBody(World::RigidBodyState &body, const Wrench &wrench, double dtS) {
  if (!(body.MassKg > 0.0) || !(dtS > 0.0)) { return; }

  for (int axis = 0; axis < 3; ++axis) {
    body.VelocityMs[axis] += wrench.ForceN[axis] / body.MassKg * dtS;
    body.PositionM[axis] += body.VelocityMs[axis] * dtS;
  }

  Vec3 torqueBody;
  InverseRotateVector(body.OrientationQ, wrench.TorqueNm, torqueBody);
  Vec3 momentum;
  for (int axis = 0; axis < 3; ++axis) {
    momentum[axis] = body.InertiaKgM2[axis] * body.SpinBodyRadS[axis];
  }
  const Vec3 gyroscopic = Cross(body.SpinBodyRadS, momentum);
  for (int axis = 0; axis < 3; ++axis) {
    if (!(body.InertiaKgM2[axis] > 0.0)) { continue; }
    body.SpinBodyRadS[axis] += (torqueBody[axis] - gyroscopic[axis]) / body.InertiaKgM2[axis] * dtS;
  }

  const Quat &q = body.OrientationQ;
  const Vec3 &spin = body.SpinBodyRadS;
  const Quat rate = {.X = 0.5 * (q.W * spin[0] + q.Y * spin[2] - q.Z * spin[1]),
                     .Y = 0.5 * (q.W * spin[1] + q.Z * spin[0] - q.X * spin[2]),
                     .Z = 0.5 * (q.W * spin[2] + q.X * spin[1] - q.Y * spin[0]),
                     .W = -0.5 * (q.X * spin[0] + q.Y * spin[1] + q.Z * spin[2])};

  const Quat turned = {.X = q.X + rate.X * dtS,
                       .Y = q.Y + rate.Y * dtS,
                       .Z = q.Z + rate.Z * dtS,
                       .W = q.W + rate.W * dtS};
  const double square =
      turned.W * turned.W + turned.X * turned.X + turned.Y * turned.Y + turned.Z * turned.Z;
  const double length = std::sqrt(square);
  if (!(length > 0.0)) { return; }
  body.OrientationQ = {.X = turned.X / length,
                       .Y = turned.Y / length,
                       .Z = turned.Z / length,
                       .W = turned.W / length};
}

double MechanicalEnergyJ(const World::RigidBodyState &body, const Vec3 &gravityMs2) {
  double energy = 0.0;
  for (int axis = 0; axis < 3; ++axis) {
    energy += 0.5 * body.MassKg * body.VelocityMs[axis] * body.VelocityMs[axis];
    energy += 0.5 * body.InertiaKgM2[axis] * body.SpinBodyRadS[axis] * body.SpinBodyRadS[axis];
    energy -= body.MassKg * gravityMs2[axis] * body.PositionM[axis];
  }
  return energy;
}

}
