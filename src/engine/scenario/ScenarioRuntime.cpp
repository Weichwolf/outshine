#include "ScenarioRuntime.h"
#include <cassert>
#include <utility>
#include <algorithm>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include <unordered_map>
#include <string_view>
#include <EntityRegistry.h>
#include <Scenario.h>
#include "RigidBodyState.h"
#include "Vec3.h"

namespace outshine {

namespace Says {
constexpr auto AmbiguousBody = "ambiguous body name: ";
constexpr auto PlacedBodyRequired = "binding requires a placed body: ";
}

std::expected<std::vector<std::optional<Entity>>, std::string>
ScenarioRuntime::BindAudio(std::span<const Audio::SoundSource> sounds) const {
  std::vector<std::string_view> names;
  names.reserve(sounds.size());
  for (const Audio::SoundSource &sound : sounds) { names.push_back(sound.Body); }
  return BindBodies(names);
}

std::expected<std::vector<std::optional<Entity>>, std::string>
ScenarioRuntime::BindBodies(std::span<const std::string_view> names) const {
  if (std::ranges::all_of(names, [](std::string_view name) { return name.empty(); })) {
    return std::vector<std::optional<Entity>>(names.size());
  }
  std::unordered_map<std::string_view, Entity> named;
  named.reserve(Stood.Bodies.size());
  for (const Entity entity : Stood.Bodies) {
    const auto *body = Definitions.Get(entity);
    if (body == nullptr || body->Name.empty()) { continue; }
    if (!named.emplace(body->Name, entity).second) {
      return std::unexpected(Says::AmbiguousBody + body->Name);
    }
  }
  std::vector<std::optional<Entity>> bindings;
  bindings.reserve(names.size());
  for (const auto name : names) {
    if (name.empty()) {
      bindings.emplace_back();
      continue;
    }
    const auto target = named.find(name);
    if (target == named.end() || Entities.Body(target->second) == nullptr) {
      return std::unexpected(Says::PlacedBodyRequired + std::string(name));
    }
    bindings.emplace_back(target->second);
  }
  return bindings;
}

std::expected<void, std::string> ScenarioRuntime::PrepareBodies() {
  std::vector<World::EntityBodyState> bodies;
  bodies.reserve(Stood.Bodies.size());
  for (const Entity entity : Stood.Bodies) {
    const Scenario::Body *definition = Definitions.Get(entity);
    assert(definition != nullptr);
    if (!definition->Placed) { continue; }
    World::RigidBodyState motion;
    motion.MassKg = definition->MassKg;
    motion.PositionM = definition->Stands.AtM;
    motion.InertiaKgM2 = definition->InertiaKgM2;
    motion.OrientationQ = definition->Stands.Facing;
    bodies.push_back({.Owner = entity, .Motion = motion});
  }
  const auto installed = Entities.SetBodies(std::move(bodies));
  if (!installed) { return std::unexpected("could not publish native entity bodies"); }
  return {};
}

}
