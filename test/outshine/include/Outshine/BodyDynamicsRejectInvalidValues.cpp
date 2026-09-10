#include <Outshine.h>
#include "Check.h"
#include <array>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document source;
  source.Room = 1;
  Scenario::Body body;
  body.Name = "kept";
  body.Placed = true;
  body.MassKg = 1;
  source.Bodies.push_back(body);
  Engine engine;
  CHECK(engine.declare(source).has_value() && engine.assemble().has_value(),
        "valid dynamic body assembled");
  auto *registry = &engine.entities();
  const auto marker = registry->addEntity(Role::Tool);
  const auto reject = [&](Scenario::Body invalid) {
    auto candidate = source;
    invalid.Name = "invalid";
    candidate.Bodies[0] = invalid;
    CHECK(!engine.declare(candidate), "invalid dynamics rejected before publication");
    CHECK(engine.declaration().Bodies[0].Name == "kept", "body declaration preserved");
    CHECK(&engine.entities() == registry && registry->alive(marker), "live simulation preserved");
  };
  for (double invalid :
       {-1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
    auto broken = body;
    broken.MassKg = invalid;
    reject(broken);
    for (size_t axis = 0; axis < 3; ++axis) {
      broken = body;
      broken.InertiaKgM2[axis] = invalid;
      reject(broken);
    }
  }
  for (double invalid :
       {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
    for (size_t axis = 0; axis < 3; ++axis) {
      auto broken = body;
      broken.Stands.AtM[axis] = invalid;
      reject(broken);
    }
    for (const auto member : std::array{&Quat::X, &Quat::Y, &Quat::Z, &Quat::W}) {
      auto broken = body;
      broken.Stands.Facing.*member = invalid;
      reject(broken);
    }
  }
  for (double scalar : {0.0, 2.0}) {
    auto broken = body;
    broken.Stands.Facing.W = scalar;
    reject(broken);
  }
  source.Bodies[0].MassKg = 0;
  source.Bodies[0].Stands.Facing.W = -1;
  CHECK(engine.declare(source).has_value() && engine.assemble().has_value(),
        "zero mass and equivalent negative unit quaternion accepted");
  return Report();
}
