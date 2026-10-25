#include "BootstrapInputs.h"
#include "Assembled.h"
#include "Entity.h"
#include "Scenario.h"
#include "Traits.h"
#include <cstddef>
#include <optional>
#include <span>
#include <string>

namespace outshine {
bool BuildBootstrapInstances(std::span<const Scenario::Instance> instances,
                             BootstrapInputs &inputs,
                             std::string &error) {
  if (!instances.empty()) { inputs.Instances.reserve(instances.size()); }
  inputs.Output.Instances.reserve(instances.size());
  for (const auto &instance : instances) {
    if (instance.Id.empty() || inputs.Instances.contains(instance.Id)) {
      error = "scenario instance names must be nonempty and unique";
      return false;
    }
    const auto kind = inputs.Kinds.find(instance.Of);
    if (kind == inputs.Kinds.end()) {
      error = "the instance '" + instance.Id + "' requires a declared scenario kind";
      return false;
    }
    Traits values;
    if (!ResolveBootstrapAttributes(kind->second, inputs.Output, values, error) ||
        !ApplyBootstrapAttributes(instance.Attributes, values, inputs, instance.Id, error)) {
      return false;
    }
    const auto entity = inputs.Entities.addEntity();
    if (!entity) {
      error = entity.error().Message;
      return false;
    }
    if (values.Count > 0 && !inputs.Attributes.Put(*entity, values)) {
      error = "could not install scenario instance attributes";
      return false;
    }
    inputs.Instances.emplace(instance.Id, inputs.Output.Instances.size());
    inputs.Output.Instances.push_back({.Name = instance.Id, .Value = *entity});
  }
  return true;
}

namespace {
bool LinkHolder(size_t child, size_t holder, Assembled &scene, std::string &error) {
  if (scene.Instances[child].Holder) {
    error = "a scenario instance can have only one declared holder";
    return false;
  }
  std::optional<size_t> at = holder;
  while (at) {
    if (*at == child) {
      error = "scenario containment cannot close a cycle";
      return false;
    }
    at = scene.Instances[*at].Holder;
  }
  scene.Instances[child].Holder = holder;
  return true;
}
}

bool LinkBootstrapInstances(std::span<const Scenario::Instance> instances,
                            BootstrapInputs &inputs,
                            std::string &error) {
  for (size_t at = 0; at < instances.size(); ++at) {
    const auto &instance = instances[at];
    for (const auto &name : instance.Holds) {
      const auto child = inputs.Instances.find(name);
      if (child == inputs.Instances.end()) {
        error = "the scenario holder '" + instance.Id + "' names an unknown instance";
        return false;
      }
      if (!LinkHolder(child->second, at, inputs.Output, error)) { return false; }
    }
    if (instance.In.empty()) { continue; }
    const auto holder = inputs.Instances.find(instance.In);
    if (holder == inputs.Instances.end()) {
      error = "the scenario instance '" + instance.Id + "' names an unknown holder";
      return false;
    }
    if (!LinkHolder(at, holder->second, inputs.Output, error)) { return false; }
  }
  return true;
}
}
