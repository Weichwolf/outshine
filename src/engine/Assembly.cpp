#include "Assembly.h"

#include <cstdlib>

namespace outshine {

size_t AssembledCapacity(const Scenario &declared) {
  size_t instanced = 0;
  for (const Instance &one : declared.Instances) {
    (void)one;
    ++instanced;
  }
  return declared.Vehicles.size() + (declared.Played.Is.empty() ? 0u : 1u) +
         (declared.Driven.Declared ? 2u : 0u) + declared.Kinds.size() + instanced;
}

namespace {

[[nodiscard]] bool CapabilityNamed(const std::string &name, Tag &out) {
  if (name == "steer") { out = tags::DoesSteer; return true; }
  if (name == "drive") { out = tags::DoesDrive; return true; }
  if (name == "brake") { out = tags::DoesBrake; return true; }
  if (name == "lamp") { out = tags::DoesLamp; return true; }
  return false;
}

[[nodiscard]] uint32_t Interned(std::vector<std::string> &names, const std::string &name) {
  for (size_t at = 0; at < names.size(); ++at) {
    if (names[at] == name) { return (uint32_t)(at + 1); }
  }
  names.push_back(name);
  return (uint32_t)names.size();
}

[[nodiscard]] bool Numbered(const Attribute &attribute, const std::string &on, double &value,
                            std::string &error) {
  char *end = nullptr;
  value = std::strtod(attribute.Value.c_str(), &end);
  if (end == attribute.Value.c_str() || (end != nullptr && *end != 0)) {
    error = "the attribute '" + attribute.Name + "' on '" + on + "' declares '" +
            attribute.Value + "', which is not a number -- a tick carries no string";
    return false;
  }
  return true;
}

}

bool Assemble(const Scenario &declared, Store &into, Column<Vehicle> &vehicles,
              Column<Drive> &driven, Column<Traits> &traits, Assembled &out,
              std::string &error) {
  out = Assembled{};

  for (const Kind &kind : declared.Kinds) {
    if (!(out.PrefabNamed(kind.Name) == kNoEntity)) {
      error = "the kind '" + kind.Name + "' is declared twice, and a default has one spelling";
      return false;
    }
    const Entity prefab = into.Add(Role::Body);
    if (!into.Alive(prefab)) {
      error = into.Error();
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
      if (!into.Link(prefab, Relation::IsA, parent)) {
        error = into.Error();
        return false;
      }
    }
    for (const std::string &capability : kind.Capabilities) {
      Tag doing = tags::Does;
      if (!CapabilityNamed(capability, doing)) {
        error = "the kind '" + kind.Name + "' may '" + capability +
                "', which the catalogue does not offer";
        return false;
      }
      if (!into.Give(prefab, doing)) {
        error = into.Error();
        return false;
      }
    }
    Traits given;
    for (const Attribute &attribute : kind.Attributes) {
      double value = 0.0;
      if (!Numbered(attribute, kind.Name, value, error)) { return false; }
      if (!given.Put(Interned(out.TraitNames, attribute.Name), value)) {
        error = "the kind '" + kind.Name + "' declares more than " +
                std::to_string(Traits::kMost) + " attributes, and the budget is declared";
        return false;
      }
    }
    if (given.Count > 0 && !traits.Put(prefab, given)) {
      error = "the kind's defaults found no column seat for '" + kind.Name + "'";
      return false;
    }
    out.Prefabs.push_back({kind.Name, prefab});
  }

  for (const Instance &instance : declared.Instances) {
    const Entity prefab = out.PrefabNamed(instance.Of);
    if (prefab == kNoEntity) {
      error = "the instance '" + instance.Id + "' is of '" + instance.Of +
              "', which no kind declares";
      return false;
    }
    const Entity stood = into.Instantiate(prefab);
    if (!into.Alive(stood)) {
      error = into.Error();
      return false;
    }
    Traits resolved;
    {
      constexpr size_t kDeepest = 8;
      Entity chain[kDeepest];
      size_t depth = 0;
      Entity at = prefab;
      for (; !(at == kNoEntity) && depth < kDeepest; ++depth) {
        chain[depth] = at;
        at = into.TargetOf(at, Relation::IsA);
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
    for (const Attribute &attribute : instance.Attributes) {
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
    out.Instances.push_back({instance.Id, stood});
  }

  for (const Instance &instance : declared.Instances) {
    const Entity holder = out.InstanceNamed(instance.Id);
    for (const std::string &what : instance.Holds) {
      const Entity held = out.InstanceNamed(what);
      if (held == kNoEntity) {
        error = "the instance '" + instance.Id + "' holds '" + what +
                "', which nothing declares";
        return false;
      }
      if (!into.Link(held, Relation::HeldBy, holder)) {
        error = into.Error();
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
      if (!into.Link(self, Relation::HeldBy, room)) {
        error = into.Error();
        return false;
      }
    }
  }

  for (const Vehicle &vehicle : declared.Vehicles) {
    const Entity body = into.Add(Role::Body);
    if (!into.Alive(body)) {
      error = into.Error();
      return false;
    }
    const bool steers = vehicle.TurningCircleM > 0.0;
    const bool drives = vehicle.PeakTorqueNm > 0.0 && vehicle.FinalDrive > 0.0;
    const bool brakes = vehicle.BrakeTorqueNm > 0.0;
    if ((steers && !into.Give(body, tags::DoesSteer)) ||
        (drives && !into.Give(body, tags::DoesDrive)) ||
        (brakes && !into.Give(body, tags::DoesBrake))) {
      error = into.Error();
      return false;
    }
    if (!vehicles.Put(body, vehicle)) {
      error = "the vehicle's numbers found no column seat for '" + vehicle.Name + "'";
      return false;
    }
    out.Bodies.push_back(body);
    if (!declared.Played.Is.empty() && declared.Played.Is == vehicle.Name) {
      out.PlayerBody = body;
    }
  }
  if (!declared.Played.Is.empty()) {
    if (out.PlayerBody == kNoEntity) {
      error = "the player is '" + declared.Played.Is + "', which no vehicle declares";
      return false;
    }
    out.PlayerMind = into.Add(Role::Mind);
    if (!into.Alive(out.PlayerMind) ||
        !into.Link(out.PlayerBody, Relation::DrivenBy, out.PlayerMind)) {
      error = into.Error();
      return false;
    }
  }
  if (declared.Driven.Declared) {
    if (declared.Driven.FromLatDeg == declared.Driven.ToLatDeg &&
        declared.Driven.FromLonDeg == declared.Driven.ToLonDeg) {
      error = "the drive's ends coincide at (" + std::to_string(declared.Driven.FromLatDeg) +
              ", " + std::to_string(declared.Driven.FromLonDeg) +
              "), which declares no route -- a zoom without a base route is a layer over "
              "nothing";
      return false;
    }
    if (!into.Alive(out.PlayerMind)) {
      error = "a drive is declared and no mind stands to take it -- declare a player";
      return false;
    }
    out.Nav = into.Add(Role::Tool);
    out.Assignment = into.Add(Role::Assignment);
    if (!into.Alive(out.Nav) || !into.Alive(out.Assignment) ||
        !into.Link(out.PlayerMind, Relation::Uses, out.Nav) ||
        !into.Link(out.PlayerMind, Relation::Assigned, out.Assignment) ||
        !driven.Put(out.Assignment, declared.Driven)) {
      error = into.Error().empty() ? "the drive's numbers found no column seat" : into.Error();
      return false;
    }
  }
  return true;
}

}
