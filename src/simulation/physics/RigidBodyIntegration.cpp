#include "RigidBodyDynamics.h"
#include "EntityBodyState.h"
#include <span>

namespace outshine::Physics {

void IntegrateBodies(std::span<World::EntityBodyState> bodies,
                     double stepSeconds,
                     const Vec3 &gravityMs2) {
  for (auto &body : bodies) {
    Wrench force;
    AccumulateGravity(force, body.Motion, gravityMs2);
    IntegrateBody(body.Motion, force, stepSeconds);
  }
}

}
