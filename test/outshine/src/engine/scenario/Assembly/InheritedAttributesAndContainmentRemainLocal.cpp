#include "Assembly.h"
#include "Assembled.h"
#include "Column.h"
#include "EntityRegistry.h"
#include "Scenario.h"
#include "Traits.h"
#include "Check.h"
#include <string>

namespace {
using namespace outshine;
using namespace outshine::Test;

Scenario::Document Input() {
  Scenario::Document source;
  Scenario::Kind root;
  root.Name = "root";
  root.Attributes = {{.Name = "health", .Value = "125"}, {.Name = "speed", .Value = "2.5"}};
  Scenario::Kind actor;
  actor.Name = "actor";
  actor.Inherits = "root";
  actor.Attributes = {{.Name = "health", .Value = "42"}};
  source.Kinds = {root, actor};
  Scenario::Instance room;
  room.Of = "actor";
  room.Id = "room";
  Scenario::Instance player;
  player.Of = "actor";
  player.Id = "player";
  player.In = "room";
  player.Attributes = {{.Name = "health", .Value = "73.5"}};
  source.Instances = {room, player};
  return source;
}

void Check(const Scenario::Document &source, bool accepted) {
  const auto capacity = RequiredEntityCapacity(source);
  CHECK(capacity && *capacity == 2, "only actual instances need world identities");
  if (!capacity) { return; }
  World::EntityRegistry identities;
  Column<Scenario::Body> bodies;
  Column<Traits> attributes;
  CHECK(identities.open(*capacity) && bodies.Open(identities) && attributes.Open(identities),
        "one world identity authority owns the scenario components");
  Assembled assembled;
  std::string error;
  const bool built = Assemble(source, identities, bodies, attributes, assembled, error);
  CHECK(built == accepted && (built || !error.empty()), "invalid ownership fails explicitly");
  if (!built) { return; }
  const Entity player = assembled.InstanceNamed("player");
  const auto *values = attributes.Get(player);
  const auto healthKey = assembled.TraitKey("health");
  const auto speedKey = assembled.TraitKey("speed");
  constexpr double kExpectedHealth = 73.5;
  constexpr double kExpectedSpeed = 2.5;
  CHECK(values && values->Named(healthKey) && *values->Named(healthKey) == kExpectedHealth &&
            values->Named(speedKey) && *values->Named(speedKey) == kExpectedSpeed,
        "instance overrides and inherited defaults retain the current scalar trait behavior");
  CHECK(assembled.Instances.size() == 2 && !assembled.Instances[0].Holder &&
            assembled.Instances[1].Holder == 0 && identities.alive(player),
        "scenario containment is resolved data outside the world identity store");
}
}

int main() {
  const auto source = Input();
  Check(source, true);
  auto cycle = source;
  cycle.Instances[0].In = "player";
  Check(cycle, false);
  auto duplicate = source;
  duplicate.Instances[0].Holds = {"player"};
  Check(duplicate, false);
  auto missing = source;
  missing.Instances[1].In = "absent";
  Check(missing, false);
  return Report();
}
