#include "RigidBodyDynamics.h"
#include "EntityBodyState.h"
#include "EntityState.h"

namespace outshine::Physics {

void IntegrateBodies(World::EntityState &bodies, double stepSeconds, const Vec3 &gravityMs2) {
  if (!(stepSeconds > 0.0)) { return; }
  for (const auto &body : bodies.Bodies()) {
    if (!(body.Motion.MassKg > 0.0)) { continue; }
    (void)bodies.EditBody(body.Owner, [&](World::RigidBodyState &motion) {
      Wrench force;
      AccumulateGravity(force, motion, gravityMs2);
      IntegrateBody(motion, force, stepSeconds);
    });
  }
}
}
