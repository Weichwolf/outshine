#include "Rigid.h"

#include <cmath>

#include "Vec3.h"

namespace outshine::Physics {

using outshine::Cross;

void Turn(const double orientationQ[4], const double bodyV[3], double worldV[3]) {
  const double w = orientationQ[0];
  const double q[3] = {orientationQ[1], orientationQ[2], orientationQ[3]};
  double qv[3];
  Cross(q, bodyV, qv);
  double qqv[3];
  const double twice[3] = {2.0 * qv[0], 2.0 * qv[1], 2.0 * qv[2]};
  Cross(q, twice, qqv);
  for (int axis = 0; axis < 3; ++axis) { worldV[axis] = bodyV[axis] + w * twice[axis] + qqv[axis]; }
}

void Unturn(const double orientationQ[4], const double worldV[3], double bodyV[3]) {
  const double back[4] = {orientationQ[0], -orientationQ[1], -orientationQ[2], -orientationQ[3]};
  Turn(back, worldV, bodyV);
}

void Place(const Rigid &body, const double atBodyM[3], double worldM[3]) {
  double turned[3];
  Turn(body.OrientationQ, atBodyM, turned);
  for (int axis = 0; axis < 3; ++axis) { worldM[axis] = body.PositionM[axis] + turned[axis]; }
}

void Carry(const Rigid &body, const double atBodyM[3], double worldMs[3]) {
  double spun[3];
  Cross(body.SpinBodyRadS, atBodyM, spun);
  double turned[3];
  Turn(body.OrientationQ, spun, turned);
  for (int axis = 0; axis < 3; ++axis) { worldMs[axis] = body.VelocityMs[axis] + turned[axis]; }
}

void Push(Wrench &wrench, const Rigid &body, const double atBodyM[3], const double forceN[3]) {
  double arm[3];
  Turn(body.OrientationQ, atBodyM, arm);
  double torque[3];
  Cross(arm, forceN, torque);
  for (int axis = 0; axis < 3; ++axis) {
    wrench.ForceN[axis] += forceN[axis];
    wrench.TorqueNm[axis] += torque[axis];
  }
}

void Fall(Wrench &wrench, const Rigid &body, const double gravityMs2[3]) {
  for (int axis = 0; axis < 3; ++axis) { wrench.ForceN[axis] += body.MassKg * gravityMs2[axis]; }
}

void Step(Rigid &body, const Wrench &wrench, double dtS) {
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
  const double rate[4] = {-0.5 * (q[1] * spin[0] + q[2] * spin[1] + q[3] * spin[2]),
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

double EnergyJ(const Rigid &body, const double gravityMs2[3]) {
  double energy = 0.0;
  for (int axis = 0; axis < 3; ++axis) {
    energy += 0.5 * body.MassKg * body.VelocityMs[axis] * body.VelocityMs[axis];
    energy += 0.5 * body.InertiaKgM2[axis] * body.SpinBodyRadS[axis] * body.SpinBodyRadS[axis];
    energy -= body.MassKg * gravityMs2[axis] * body.PositionM[axis];
  }
  return energy;
}

namespace {

void Unit(double v[3]) {
  const double length = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (length > 0.0) {
    for (int axis = 0; axis < 3; ++axis) { v[axis] /= length; }
  }
}

} // namespace

void Lie(Rigid &body, const double aheadM[3], const double upM[3]) {
  double ahead[3] = {aheadM[0], aheadM[1], aheadM[2]};
  double up[3] = {upM[0], upM[1], upM[2]};
  Unit(up);
  const double along = ahead[0] * up[0] + ahead[1] * up[1] + ahead[2] * up[2];
  for (int axis = 0; axis < 3; ++axis) { ahead[axis] -= along * up[axis]; }
  Unit(ahead);

  const double back[3] = {-ahead[0], -ahead[1], -ahead[2]};
  const double right[3] = {up[1] * back[2] - up[2] * back[1],
                           up[2] * back[0] - up[0] * back[2],
                           up[0] * back[1] - up[1] * back[0]};

  const double m[3][3] = {
      {right[0], up[0], back[0]}, {right[1], up[1], back[1]}, {right[2], up[2], back[2]}};
  const double trace = m[0][0] + m[1][1] + m[2][2];
  double q[4];
  if (trace > 0.0) {
    const double root = std::sqrt(trace + 1.0) * 2.0;
    q[0] = 0.25 * root;
    q[1] = (m[2][1] - m[1][2]) / root;
    q[2] = (m[0][2] - m[2][0]) / root;
    q[3] = (m[1][0] - m[0][1]) / root;
  } else if (m[0][0] > m[1][1] && m[0][0] > m[2][2]) {
    const double root = std::sqrt(1.0 + m[0][0] - m[1][1] - m[2][2]) * 2.0;
    q[0] = (m[2][1] - m[1][2]) / root;
    q[1] = 0.25 * root;
    q[2] = (m[0][1] + m[1][0]) / root;
    q[3] = (m[0][2] + m[2][0]) / root;
  } else if (m[1][1] > m[2][2]) {
    const double root = std::sqrt(1.0 + m[1][1] - m[0][0] - m[2][2]) * 2.0;
    q[0] = (m[0][2] - m[2][0]) / root;
    q[1] = (m[0][1] + m[1][0]) / root;
    q[2] = 0.25 * root;
    q[3] = (m[1][2] + m[2][1]) / root;
  } else {
    const double root = std::sqrt(1.0 + m[2][2] - m[0][0] - m[1][1]) * 2.0;
    q[0] = (m[1][0] - m[0][1]) / root;
    q[1] = (m[0][2] + m[2][0]) / root;
    q[2] = (m[1][2] + m[2][1]) / root;
    q[3] = 0.25 * root;
  }
  for (int part = 0; part < 4; ++part) { body.OrientationQ[part] = q[part]; }
}

} // namespace outshine::Physics
