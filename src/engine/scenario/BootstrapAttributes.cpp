#include "BootstrapInputs.h"
#include "Assembled.h"
#include "Scenario.h"
#include "Traits.h"
#include "format/Number.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace outshine {
bool ApplyBootstrapAttributes(std::span<const Scenario::Setting> attributes,
                              Traits &values,
                              BootstrapInputs &inputs,
                              std::string_view owner,
                              std::string &error) {
  for (const auto &attribute : attributes) {
    const auto value = ParseFiniteNumber(attribute.Value);
    if (!value) {
      error = "the attribute '" + attribute.Name + "' on '" + std::string(owner) +
              "' requires a finite decimal number";
      return false;
    }
    auto found = inputs.TraitKeys.find(attribute.Name);
    if (found == inputs.TraitKeys.end()) {
      if (inputs.Output.TraitNames.size() >= std::numeric_limits<uint32_t>::max()) {
        error = "scenario trait identifiers are exhausted";
        return false;
      }
      const auto key = static_cast<uint32_t>(inputs.Output.TraitNames.size() + 1);
      found = inputs.TraitKeys.emplace(attribute.Name, key).first;
      inputs.Output.TraitNames.push_back(attribute.Name);
    }
    if (!values.Put({.Key = found->second, .Value = *value})) {
      error = "the attributes of '" + std::string(owner) + "' exceed the scenario trait budget";
      return false;
    }
  }
  return true;
}

bool ResolveBootstrapAttributes(size_t kind,
                                const Assembled &scene,
                                Traits &values,
                                std::string &error) {
  constexpr size_t kDeepest = 8;
  std::array<size_t, kDeepest> chain{};
  size_t depth = 0;
  std::optional<size_t> at = kind;
  while (at && depth < chain.size()) {
    chain[depth++] = *at;
    at = scene.Kinds[*at].Parent;
  }
  if (at) {
    error = "the scenario kind chain exceeds its depth budget";
    return false;
  }
  for (size_t remaining = depth; remaining > 0; --remaining) {
    const auto &attributes = scene.Kinds[chain[remaining - 1]].Attributes;
    for (size_t row = 0; row < attributes.Count; ++row) {
      if (!values.Put({.Key = attributes.Keys[row], .Value = attributes.Values[row]})) {
        error = "inherited scenario attributes exceed the trait budget";
        return false;
      }
    }
  }
  return true;
}
}
