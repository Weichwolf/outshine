#include "Assembly.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <string>

namespace outshine {

size_t AssembledCapacity(const Scenario::Document &declared) {
  size_t instanced = 0;
  for (const Scenario::Instance &one : declared.Instances) {
    (void)one;
    ++instanced;
  }
  return declared.Room + declared.Bodies.size() + (declared.Played.Is.empty() ? 0u : 1u) +
         (declared.Routed.Declared ? 2u : 0u) + declared.Kinds.size() + instanced;
}

namespace {

[[nodiscard]] uint32_t Interned(std::vector<std::string> &names, const std::string &name) {
  for (size_t at = 0; at < names.size(); ++at) {
    if (names[at] == name) { return static_cast<uint32_t>(at + 1); }
  }
  names.push_back(name);
  return static_cast<uint32_t>(names.size());
}

[[nodiscard]] bool Numbered(const Scenario::Setting &attribute,
                            const std::string &on,
                            double &value,
                            std::string &error) {
  char *end = nullptr;
  value = std::strtod(attribute.Value.c_str(), &end);
  if (end == attribute.Value.c_str() || (end != nullptr && *end != 0)) {
    error = "the attribute '" + attribute.Name + "' on '" + on + "' declares '" + attribute.Value +
            "', which is not a number -- a tick carries no string";
    return false;
  }
  return true;
}

} // namespace

bool Assemble(const Scenario::Document &declared,
              Scene &into,
              Column<Scenario::Body> &bodies,
              Column<Scenario::Journey> &driven,
              Column<Traits> &traits,
              Assembled &out,
              std::string &error) {
  out = Assembled{};

  for (const Scenario::Kind &kind : declared.Kinds) {
    if (!(out.PrefabNamed(kind.Name) == kNoEntity)) {
      error = "the kind '" + kind.Name + "' is declared twice, and a default has one spelling";
      return false;
    }
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
      const Tag doing = TagCatalogue::under(tags::Does, Interned(out.TagNames, capability));
      if (!into.giveTag(prefab, doing)) {
        error = into.error();
        return false;
      }
    }
    Traits given;
    for (const Scenario::Setting &attribute : kind.Attributes) {
      double value = 0.0;
      if (!Numbered(attribute, kind.Name, value, error)) { return false; }
      if (!given.Put(Interned(out.TraitNames, attribute.Name), value)) {
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
  }

  for (const Scenario::Instance &instance : declared.Instances) {
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
    {
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
            if (!resolved.Put(held->Keys[attr], held->Values[attr])) {
              error = "the resolved attributes of '" +
                      (instance.Id.empty() ? instance.Of : instance.Id) +
                      "' overflow the declared budget of " + std::to_string(Traits::kMost) +
                      " -- the kind chain's union is too wide";
              return false;
            }
          }
        }
      }
    }
    for (const Scenario::Setting &attribute : instance.Attributes) {
      double value = 0.0;
      if (!Numbered(attribute, instance.Id.empty() ? instance.Of : instance.Id, value, error)) {
        return false;
      }
      if (!resolved.Put(Interned(out.TraitNames, attribute.Name), value)) {
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
  }

  for (const Scenario::Instance &instance : declared.Instances) {
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
  }

  for (const Scenario::Body &declaredBody : declared.Bodies) {
    const Entity body = into.addEntity(Role::Body);
    if (!into.alive(body)) {
      error = into.error();
      return false;
    }
    for (const Scenario::Drive &does : declaredBody.Driven) {
      const char *const named = does.Does == Scenario::Drives::Motion
                                    ? "steer"
                                    : (does.Opposes ? "torque-opposing" : "torque");
      if (!into.giveTag(body, TagCatalogue::under(tags::Does, Interned(out.TagNames, named)))) {
        error = into.error();
        return false;
      }
    }
    if (!bodies.Put(body, declaredBody)) {
      error = "the body's numbers found no column seat for '" + declaredBody.Name + "'";
      return false;
    }
    out.Bodies.push_back(body);
    if (!declared.Played.Is.empty() && declared.Played.Is == declaredBody.Name) {
      out.PlayerBody = body;
    }
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
  if (declared.Routed.Declared) {
    if (declared.Routed.FromLatDeg == declared.Routed.ToLatDeg &&
        declared.Routed.FromLonDeg == declared.Routed.ToLonDeg) {
      error = "the drive's ends coincide at (" + std::to_string(declared.Routed.FromLatDeg) + ", " +
              std::to_string(declared.Routed.FromLonDeg) +
              "), which declares no route -- a zoom without a base route is a layer over "
              "nothing";
      return false;
    }
    if (!into.alive(out.PlayerMind)) {
      error = "a drive is declared and no mind stands to take it -- declare a player";
      return false;
    }
    out.Nav = into.addEntity(Role::Tool);
    out.Assignment = into.addEntity(Role::Assignment);
    if (!into.alive(out.Nav) || !into.alive(out.Assignment) ||
        !into.link(out.PlayerMind, Relation::Uses, out.Nav) ||
        !into.link(out.PlayerMind, Relation::Assigned, out.Assignment) ||
        !driven.Put(out.Assignment, declared.Routed)) {
      error = into.error().empty() ? "the drive's numbers found no column seat" : into.error();
      return false;
    }
  }
  return true;
}

} // namespace outshine
