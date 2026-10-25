#ifndef OUTSHINE_ENGINE_SCENARIO_BOOTSTRAPINPUTS_H
#define OUTSHINE_ENGINE_SCENARIO_BOOTSTRAPINPUTS_H

#include "Assembled.h"
#include "Column.h"
#include "EntityRegistry.h"
#include "Scenario.h"
#include "Traits.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

namespace outshine {
struct BootstrapInputs {
  World::EntityRegistry &Entities;
  Column<Scenario::Body> &Bodies;
  Column<Traits> &Attributes;
  Assembled &Output;
  std::unordered_map<std::string_view, size_t> Kinds;
  std::unordered_map<std::string_view, size_t> Instances;
  std::unordered_map<std::string_view, uint32_t> TraitKeys;
};

[[nodiscard]] bool ApplyBootstrapAttributes(std::span<const Scenario::Setting> attributes,
                                            Traits &values,
                                            BootstrapInputs &inputs,
                                            std::string_view owner,
                                            std::string &error);
[[nodiscard]] bool
ResolveBootstrapAttributes(size_t kind, const Assembled &scene, Traits &values, std::string &error);
[[nodiscard]] bool BuildBootstrapKinds(std::span<const Scenario::Kind> kinds,
                                       BootstrapInputs &inputs,
                                       std::string &error);
[[nodiscard]] bool BuildBootstrapInstances(std::span<const Scenario::Instance> instances,
                                           BootstrapInputs &inputs,
                                           std::string &error);
[[nodiscard]] bool LinkBootstrapInstances(std::span<const Scenario::Instance> instances,
                                          BootstrapInputs &inputs,
                                          std::string &error);
[[nodiscard]] bool BuildBootstrapBodies(std::span<const Scenario::Body> bodies,
                                        const Scenario::Player &player,
                                        BootstrapInputs &inputs,
                                        std::string &error);
}
#endif
