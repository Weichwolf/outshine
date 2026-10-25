#ifndef OUTSHINE_WORLD_ENTITY_RIGIDBODYSTATE_H
#define OUTSHINE_WORLD_ENTITY_RIGIDBODYSTATE_H

#include "Quat.h"
#include "Vec3.h"

namespace outshine::World {

struct RigidBodyState {
  double MassKg = 0.0;
  alignas(16) Vec3 InertiaKgM2;
  alignas(16) Vec3 PositionM;
  alignas(16) Quat OrientationQ = {.X = 0.0, .Y = 0.0, .Z = 0.0, .W = 1.0};
  alignas(16) Vec3 VelocityMs;
  alignas(16) Vec3 SpinBodyRadS;
};

}

#endif
