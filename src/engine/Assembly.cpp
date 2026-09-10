#include "Assembly.h"
#include "BodyValidation.h"

#include <array>
#include <algorithm>
#include <string_view>
#include <expected>
#include <cstddef>
#include <limits>
#include <cstdint>
#include "format/Number.h"
#include <vector>
#include <string>

namespace outshine {

namespace Says {
constexpr auto EmptyName = " requires a nonempty name";
constexpr auto DuplicateName = " is declared more than once";
constexpr auto CapabilityBudget = "capability catalogue exceeds its supported identifier range";
constexpr auto EntityBudget = "simulation entity capacity exceeds 65536 slots";
}

namespace {
constexpr size_t kMaximumSimulationEntities = 65536;
static_assert(kMaximumSimulationEntities < std::numeric_limits<uint32_t>::max());
}

std::expected<size_t, std::string> RequiredEntityCapacity(const Scenario::Document &declared) {
  const std::array counts{declared.Room,
                          declared.Bodies.size(),
                          size_t{declared.Played.Is.empty() ? 0u : 1u},
                          declared.Kinds.size(),
                          declared.Instances.size()};
  size_t capacity = 0;
  for (const size_t count : counts) {
    if (count > kMaximumSimulationEntities - capacity) {
      return std::unexpected(Says::EntityBudget);
    }
    capacity += count;
  }
  return capacity;
}

namespace {

[[nodiscard]] uint32_t Interned(std::vector<std::string> &names, const std::string &name) {
  for (size_t at = 0; at < names.size(); ++at) {
    if (names[at] == name) { return static_cast<uint32_t>(at + 1); }
  }
  names.push_back(name);
  return static_cast<uint32_t>(names.size());
}

[[nodiscard]] std::expected<void, std::string>
GiveCapability(EntityRegistry &registry, Entity owner, Assembled &scene, const std::string &name) {
  const auto tag = TagCatalogue::under(tags::Does, Interned(scene.TagNames, name));
  if (!tag) { return std::unexpected(Says::CapabilityBudget); }
  if (!registry.giveTag(owner, *tag)) { return std::unexpected(std::string(registry.error())); }
  return {};
}

[[nodiscard]] bool Numbered(const Scenario::Setting &attribute,
                            const std::string &on,
                            double &value,
                            std::string &error) {
  const auto parsed = ParseFiniteNumber(attribute.Value);
  if (!parsed) {
    error = "the attribute '" + attribute.Name + "' on '" + on + "' declares '" + attribute.Value +
            "', which is not a finite decimal number";
    return false;
  }
  value = *parsed;
  return true;
}

}

namespace {

template <typename Row>
[[nodiscard]] std::expected<void, std::string> ValidateNames(const std::vector<Row> &rows,
                                                             const std::string Row::*name,
                                                             std::string_view category) {
  std::vector<std::string_view> names;
  names.reserve(rows.size());
  for (const auto &row : rows) {
    if ((row.*name).empty()) { return std::unexpected(std::string(category) + Says::EmptyName); }
    names.push_back(row.*name);
  }
  std::ranges::sort(names);
  const auto repeated = std::ranges::adjacent_find(names);
  if (repeated != names.end()) {
    return std::unexpected(std::string(category) + " name '" + std::string(*repeated) + "'" +
                           Says::DuplicateName);
  }
  return {};
}

[[nodiscard]] bool BuildPrefab(const Scenario::Kind &kind,
                               EntityRegistry &into,
                               Column<Traits> &traits,
                               Assembled &out,
                               std::string &error) {
  const Entity prefab = into.addEntity(Role::Body);
  if (!into.alive(prefab)) {
    error = into.error();
    return false;
  }
  if (!kind.Inherits.empty()) {
    const Entity parent = out.PrefabNamed(kind.Inherits);
    if (parent == kNoEntity) {
      error = "the kind '" + kind.Name + "' inherits '" + kind.Inherits +
              "', which is not declared before it -- the order is the declaration's, and a "
              "cycle cannot even be spelled";
      return false;
    }
    if (!into.link(prefab, Relation::IsA, parent)) {
      error = into.error();
      return false;
    }
  }
  for (const std::string &capability : kind.Capabilities) {
    if (capability.empty()) {
      error = "the kind '" + kind.Name +
              "' may '', and a capability without a name is a "
              "declaration that names nothing";
      return false;
    }
    const auto tagged = GiveCapability(into, prefab, out, capability);
    if (!tagged) {
      error = tagged.error();
      return false;
    }
  }
  Traits given;
  for (const Scenario::Setting &attribute : kind.Attributes) {
    double value = 0.0;
    if (!Numbered(attribute, kind.Name, value, error)) { return false; }
    if (!given.Put({.Key = Interned(out.TraitNames, attribute.Name), .Value = value})) {
      error = "the kind '" + kind.Name + "' declares more than " + std::to_string(Traits::kMost) +
              " attributes, and the budget is declared";
      return false;
    }
  }
  if (given.Count > 0 && !traits.Put(prefab, given)) {
    error = "the kind's defaults found no column seat for '" + kind.Name + "'";
    return false;
  }
  out.Prefabs.emplace_back(kind.Name, prefab);
  return true;
}

[[nodiscard]] bool ResolveInheritedTraits(const Scenario::Instance &instance,
                                          Entity prefab,
                                          const EntityRegistry &into,
                                          const Column<Traits> &traits,
                                          Traits &resolved,
                                          std::string &error) {
  constexpr size_t kDeepest = 8;
  std::array<Entity, kDeepest> chain{};
  size_t depth = 0;
  Entity at = prefab;
  for (; !(at == kNoEntity) && depth < kDeepest; ++depth) {
    chain[depth] = at;
    at = into.targetOf(at, Relation::IsA);
  }
  if (!(at == kNoEntity)) {
    error = "the kind chain under '" + instance.Of + "' is deeper than " +
            std::to_string(kDeepest) + ", and a silent cut would lose the root's defaults";
    return false;
  }
  for (size_t up = depth; up > 0; --up) {
    if (const Traits *held = traits.Get(chain[up - 1])) {
      for (size_t attr = 0; attr < held->Count; ++attr) {
        if (!resolved.Put({.Key = held->Keys[attr], .Value = held->Values[attr]})) {
          error = "the resolved attributes of '" +
                  (instance.Id.empty() ? instance.Of : instance.Id) +
                  "' overflow the declared budget of " + std::to_string(Traits::kMost) +
                  " -- the kind chain's union is too wide";
          return false;
        }
      }
    }
  }
  return true;
}

[[nodiscard]] bool BuildInstance(const Scenario::Instance &instance,
                                 EntityRegistry &into,
                                 Column<Traits> &traits,
                                 Assembled &out,
                                 std::string &error) {
  const Entity prefab = out.PrefabNamed(instance.Of);
  if (prefab == kNoEntity) {
    error =
        "the instance '" + instance.Id + "' is of '" + instance.Of + "', which no kind declares";
    return false;
  }
  const Entity stood = into.instantiate(prefab);
  if (!into.alive(stood)) {
    error = into.error();
    return false;
  }
  Traits resolved;
  if (!ResolveInheritedTraits(instance, prefab, into, traits, resolved, error)) { return false; }
  for (const Scenario::Setting &attribute : instance.Attributes) {
    double value = 0.0;
    if (!Numbered(attribute, instance.Id.empty() ? instance.Of : instance.Id, value, error)) {
      return false;
    }
    if (!resolved.Put({.Key = Interned(out.TraitNames, attribute.Name), .Value = value})) {
      error = "the instance '" + instance.Id + "' overflows the declared attribute budget of " +
              std::to_string(Traits::kMost);
      return false;
    }
  }
  if (resolved.Count > 0 && !traits.Put(stood, resolved)) {
    error = "the instance's traits found no column seat for '" + instance.Id + "'";
    return false;
  }
  out.Instances.emplace_back(instance.Id, stood);
  return true;
}

[[nodiscard]] bool LinkInstance(const Scenario::Instance &instance,
                                EntityRegistry &into,
                                const Assembled &out,
                                std::string &error) {
  const Entity holder = out.InstanceNamed(instance.Id);
  for (const std::string &what : instance.Holds) {
    const Entity held = out.InstanceNamed(what);
    if (held == kNoEntity) {
      error = "the instance '" + instance.Id + "' holds '" + what + "', which nothing declares";
      return false;
    }
    if (!into.link(held, Relation::HeldBy, holder)) {
      error = into.error();
      return false;
    }
  }
  if (!instance.In.empty()) {
    const Entity room = out.InstanceNamed(instance.In);
    if (room == kNoEntity) {
      error = "the instance '" + instance.Id + "' stands in '" + instance.In +
              "', which nothing declares";
      return false;
    }
    const Entity self = out.InstanceNamed(instance.Id);
    if (!into.link(self, Relation::HeldBy, room)) {
      error = into.error();
      return false;
    }
  }
  return true;
}

[[nodiscard]] bool BuildBody(const Scenario::Body &declaredBody,
                             const Scenario::Player &player,
                             EntityRegistry &into,
                             Column<Scenario::Body> &bodies,
                             Assembled &out,
                             std::string &error) {
  const Entity body = into.addEntity(Role::Body);
  if (!into.alive(body)) {
    error = into.error();
    return false;
  }
  for (const Scenario::Drive &does : declaredBody.Driven) {
    const char *const opposing = does.Opposes ? "torque-opposing" : "torque";
    const char *const named = does.Does == Scenario::Drives::Motion ? "steer" : opposing;
    const auto tagged = GiveCapability(into, body, out, named);
    if (!tagged) {
      error = tagged.error();
      return false;
    }
  }
  if (!bodies.Put(body, declaredBody)) {
    error = "the body's numbers found no column seat for '" + declaredBody.Name + "'";
    return false;
  }
  out.Bodies.push_back(body);
  if (!player.Is.empty() && player.Is == declaredBody.Name) { out.PlayerBody = body; }
  return true;
}

}

bool Assemble(const Scenario::Document &declared,
              EntityRegistry &into,
              Column<Scenario::Body> &bodies,
              Column<Traits> &traits,
              Assembled &out,
              std::string &error) {
  if (const auto valid = ValidateBodyDynamics(declared.Bodies); !valid) {
    error = valid.error();
    return false;
  }
  if (const auto valid = ValidateNames(declared.Kinds, &Scenario::Kind::Name, "kind"); !valid) {
    error = valid.error();
    return false;
  }
  if (const auto valid = ValidateNames(declared.Instances, &Scenario::Instance::Id, "instance");
      !valid) {
    error = valid.error();
    return false;
  }
  out = Assembled{};

  for (const Scenario::Kind &kind : declared.Kinds) {
    if (!BuildPrefab(kind, into, traits, out, error)) { return false; }
  }

  for (const Scenario::Instance &instance : declared.Instances) {
    if (!BuildInstance(instance, into, traits, out, error)) { return false; }
  }

  for (const Scenario::Instance &instance : declared.Instances) {
    if (!LinkInstance(instance, into, out, error)) { return false; }
  }

  for (const Scenario::Body &declaredBody : declared.Bodies) {
    if (!BuildBody(declaredBody, declared.Played, into, bodies, out, error)) { return false; }
  }
  if (!declared.Played.Is.empty()) {
    if (out.PlayerBody == kNoEntity) {
      error = "the player is '" + declared.Played.Is + "', which no body declares";
      return false;
    }
    out.PlayerMind = into.addEntity(Role::Mind);
    if (!into.alive(out.PlayerMind) ||
        !into.link(out.PlayerBody, Relation::DrivenBy, out.PlayerMind)) {
      error = into.error();
      return false;
    }
  }
  return true;
}

}
