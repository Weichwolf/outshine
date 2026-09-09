#include "SimulationState.h"
#include <cassert>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include <unordered_map>
#include <string_view>
#include <scene/Scene.h>
#include <scenario/Scenario.h>
#include "Rigid.h"
#include "math/Vec3.h"

namespace outshine {

namespace Says {
constexpr auto AmbiguousBody = "ambiguous body name: ";
constexpr auto PlacedBodyRequired = "audio source requires a placed body: ";
}

std::expected<std::vector<std::optional<size_t>>, std::string>
SimulationState::BindAudio(std::span<const Scenario::Sound> sounds) const {
  std::unordered_map<std::string_view, Entity> named;
  named.reserve(Stood.Bodies.size());
  for (const Entity entity : Stood.Bodies) {
    const auto *body = Bodies.Get(entity);
    if (body == nullptr || body->Name.empty()) { continue; }
    if (!named.emplace(body->Name, entity).second) {
      return std::unexpected(Says::AmbiguousBody + body->Name);
    }
  }
  std::vector<std::optional<size_t>> physical(Scene.capacity());
  for (size_t index = 0; index < DynamicBodies.size(); ++index) {
    const Entity owner = DynamicBodies[index].Owner;
    if (Scene.alive(owner)) { physical[owner.Index] = index; }
  }
  std::vector<std::optional<size_t>> bindings;
  bindings.reserve(sounds.size());
  for (const auto &sound : sounds) {
    if (sound.On.empty()) {
      bindings.emplace_back();
      continue;
    }
    const auto target = named.find(sound.On);
    if (target == named.end() || !physical[target->second.Index]) {
      return std::unexpected(Says::PlacedBodyRequired + sound.On);
    }
    bindings.push_back(physical[target->second.Index]);
  }
  return bindings;
}

void SimulationState::PrepareBodies() {
  DynamicBodies.reserve(Stood.Bodies.size());
  for (const Entity entity : Stood.Bodies) {
    const Scenario::Body *definition = Bodies.Get(entity);
    assert(definition != nullptr);
    if (!definition->Placed) { continue; }
    Physics::Rigid motion;
    motion.MassKg = definition->MassKg;
    motion.PositionM = definition->Stands.AtM;
    motion.InertiaKgM2 = definition->InertiaKgM2;
    motion.OrientationQ = definition->Stands.Facing;
    DynamicBodies.push_back({.Owner = entity, .Motion = motion});
  }
}

void SimulationState::Integrate(double stepSeconds, const Vec3 &gravityMs2) {
  for (auto &body : DynamicBodies) {
    Physics::Wrench force;
    Physics::Fall(force, body.Motion, gravityMs2);
    Physics::Step(body.Motion, force, stepSeconds);
  }
}

}
