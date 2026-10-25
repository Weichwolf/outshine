#include "BootstrapInputs.h"
#include "Assembled.h"
#include "Scenario.h"
#include <cstddef>
#include <span>
#include <string>
#include <utility>

namespace outshine {
bool BuildBootstrapKinds(std::span<const Scenario::Kind> kinds,
                         BootstrapInputs &inputs,
                         std::string &error) {
  if (!kinds.empty()) { inputs.Kinds.reserve(kinds.size()); }
  inputs.Output.Kinds.reserve(kinds.size());
  for (const auto &kind : kinds) {
    if (kind.Name.empty() || inputs.Kinds.contains(kind.Name)) {
      error = "scenario kind names must be nonempty and unique";
      return false;
    }
    BootstrapKind definition{.Name = kind.Name};
    if (!kind.Inherits.empty()) {
      const auto parent = inputs.Kinds.find(kind.Inherits);
      if (parent == inputs.Kinds.end()) {
        error = "the scenario kind '" + kind.Name + "' requires an earlier parent definition";
        return false;
      }
      definition.Parent = parent->second;
    }
    for (const auto &capability : kind.Capabilities) {
      if (capability.empty()) {
        error = "scenario capability names cannot be empty";
        return false;
      }
    }
    if (!ApplyBootstrapAttributes(
            kind.Attributes, definition.Attributes, inputs, kind.Name, error)) {
      return false;
    }
    inputs.Kinds.emplace(kind.Name, inputs.Output.Kinds.size());
    inputs.Output.Kinds.push_back(std::move(definition));
  }
  return true;
}
}
