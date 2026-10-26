#ifndef OUTSHINE_ENGINE_SCENARIO_ASSEMBLED_H
#define OUTSHINE_ENGINE_SCENARIO_ASSEMBLED_H

#include "Entity.h"
#include "Traits.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace outshine {
struct BootstrapKind {
  std::string Name;
  std::optional<size_t> Parent = std::nullopt;
  Traits Attributes{};
};

struct BootstrapInstance {
  std::string Name;
  Entity Value = kNoEntity;
  std::optional<size_t> Holder = std::nullopt;
};

struct Assembled {
  std::vector<Entity> Bodies;
  Entity PlayerBody = kNoEntity;
  std::vector<BootstrapKind> Kinds;
  std::vector<BootstrapInstance> Instances;
  std::vector<std::string> TraitNames;

  [[nodiscard]] Entity InstanceNamed(std::string_view name) const noexcept {
    for (const auto &instance : Instances) {
      if (instance.Name == name) { return instance.Value; }
    }
    return kNoEntity;
  }

  [[nodiscard]] uint32_t TraitKey(std::string_view name) const noexcept {
    for (size_t at = 0; at < TraitNames.size(); ++at) {
      if (TraitNames[at] == name) { return static_cast<uint32_t>(at + 1); }
    }
    return 0;
  }
};
}
#endif
