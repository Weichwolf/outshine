#include "Body.h"

#include <cmath>

namespace outshine::Physics {

namespace {

void Cross(const double a[3], const double b[3], double out[3]) {
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}

} // namespace

void Turn(const double orientationQ[4], const double bodyV[3], double worldV[3]) {
  const double w = orientationQ[0];
  const double q[3] = {orientationQ[1], orientationQ[2], orientationQ[3]};
  double qv[3];
  Cross(q, bodyV, qv);
  double qqv[3];
  const double twice[3] = {2.0 * qv[0], 2.0 * qv[1], 2.0 * qv[2]};
  Cross(q, twice, qqv);
  for (int axis = 0; axis < 3; ++axis) {
    worldV[axis] = bodyV[axis] + w * twice[axis] + qqv[axis];
  }
}

void Unturn(const double orientationQ[4], const double worldV[3], double bodyV[3]) {
  const double back[4] = {orientationQ[0], -orientationQ[1], -orientationQ[2], -orientationQ[3]};
  Turn(back, worldV, bodyV);
}

void Place(const Body &body, const double atBodyM[3], double worldM[3]) {
  double turned[3];
  Turn(body.OrientationQ, atBodyM, turned);
  for (int axis = 0; axis < 3; ++axis) { worldM[axis] = body.PositionM[axis] + turned[axis]; }
}

void Carry(const Body &body, const double atBodyM[3], double worldMs[3]) {
  double spun[3];
  Cross(body.SpinBodyRadS, atBodyM, spun);
  double turned[3];
  Turn(body.OrientationQ, spun, turned);
  for (int axis = 0; axis < 3; ++axis) { worldMs[axis] = body.VelocityMs[axis] + turned[axis]; }
}

void Push(Wrench &wrench, const Body &body, const double atBodyM[3], const double forceN[3]) {
  double arm[3];
  Turn(body.OrientationQ, atBodyM, arm);
  double torque[3];
  Cross(arm, forceN, torque);
  for (int axis = 0; axis < 3; ++axis) {
    wrench.ForceN[axis] += forceN[axis];
    wrench.TorqueNm[axis] += torque[axis];
  }
}

void Fall(Wrench &wrench, const Body &body, const double gravityMs2[3]) {
  for (int axis = 0; axis < 3; ++axis) { wrench.ForceN[axis] += body.MassKg * gravityMs2[axis]; }
}

void Step(Body &body, const Wrench &wrench, double dtS) {
  if (!(body.MassKg > 0.0) || !(dtS > 0.0)) { return; }

  for (int axis = 0; axis < 3; ++axis) {
    body.VelocityMs[axis] += wrench.ForceN[axis] / body.MassKg * dtS;
    body.PositionM[axis] += body.VelocityMs[axis] * dtS;
  }

  double torqueBody[3];
  Unturn(body.OrientationQ, wrench.TorqueNm, torqueBody);
  double momentum[3];
  for (int axis = 0; axis < 3; ++axis) {
    momentum[axis] = body.InertiaKgM2[axis] * body.SpinBodyRadS[axis];
  }
  double gyroscopic[3];
  Cross(body.SpinBodyRadS, momentum, gyroscopic);
  for (int axis = 0; axis < 3; ++axis) {
    if (!(body.InertiaKgM2[axis] > 0.0)) { continue; }
    body.SpinBodyRadS[axis] += (torqueBody[axis] - gyroscopic[axis]) / body.InertiaKgM2[axis] * dtS;
  }

  const double *const q = body.OrientationQ;
  const double *const spin = body.SpinBodyRadS;
  const double rate[4] = {
      -0.5 * (q[1] * spin[0] + q[2] * spin[1] + q[3] * spin[2]),
      0.5 * (q[0] * spin[0] + q[2] * spin[2] - q[3] * spin[1]),
      0.5 * (q[0] * spin[1] + q[3] * spin[0] - q[1] * spin[2]),
      0.5 * (q[0] * spin[2] + q[1] * spin[1] - q[2] * spin[0])};

  double turned[4];
  double square = 0.0;
  for (int part = 0; part < 4; ++part) {
    turned[part] = body.OrientationQ[part] + rate[part] * dtS;
    square += turned[part] * turned[part];
  }
  const double length = std::sqrt(square);
  if (!(length > 0.0)) { return; }
  for (int part = 0; part < 4; ++part) { body.OrientationQ[part] = turned[part] / length; }
}

double EnergyJ(const Body &body, const double gravityMs2[3]) {
  double energy = 0.0;
  for (int axis = 0; axis < 3; ++axis) {
    energy += 0.5 * body.MassKg * body.VelocityMs[axis] * body.VelocityMs[axis];
    energy += 0.5 * body.InertiaKgM2[axis] * body.SpinBodyRadS[axis] * body.SpinBodyRadS[axis];
    energy -= body.MassKg * gravityMs2[axis] * body.PositionM[axis];
  }
  return energy;
}

} // namespace outshine::Physics
