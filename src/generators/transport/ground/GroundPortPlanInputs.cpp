#include "GroundPortPlanAssembly.h"
#include "TransportNetworkSnapshot.h"
#include "ByteArchive.h"
#include "Sha256.h"
#include <algorithm>
#include <array>
#include <utility>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <tuple>

namespace outshine::Generators::GroundPlan {
namespace {
template <class T> void Word(Sha256Prefix &hash, T value) {
  const auto bytes = LittleEndian(value);
  hash.Append(&bytes, sizeof(bytes));
}

void Text(Sha256Prefix &hash, std::string_view value) {
  Word(hash, static_cast<uint64_t>(value.size()));
  hash.Append(value.data(), value.size());
}

auto Key(const TransportInputReference &input) {
  return std::tie(input.DatasetId, input.Version, input.Revision, input.ItemId);
}

TransportInputReference ScopeInput(const TransportPlan &plan,
                                   const GroundPortBodies &sliced,
                                   const std::vector<Part> &parts,
                                   const std::array<uint8_t, 32> &network) {
  constexpr std::string_view version = "outshine/functional-ground-partial-plan/1";
  Sha256Prefix hash(version.data(), version.size());
  hash.Append(sliced.ContentBinding.data(), sliced.ContentBinding.size());
  hash.Append(network.data(), network.size());
  Word(hash, static_cast<uint64_t>(parts.size()));
  for (const auto &part : parts) {
    hash.Append(part.Input.Content.data(), part.Input.Content.size());
  }
  Word(hash, static_cast<uint64_t>(plan.Omissions.size()));
  for (const auto &omission : plan.Omissions) {
    Text(hash, omission.SourceId);
    Word(hash, static_cast<uint32_t>(omission.Reason));
  }
  const auto &core = plan.Definition.Core;
  return {.DatasetId = "outshine/functional-ground-partial-plan",
          .Version = "1",
          .Revision = "1",
          .ItemId = std::format("core/{}/{}/{}", core.Level, core.X, core.Y),
          .Content = hash.Digest(nullptr, 0)};
}
}

TransportInputReference PathInput(const GroundPortBodies &whole,
                                  const GroundPortPlanPathRecipe &recipe,
                                  std::string_view sourceId) {
  constexpr std::string_view version = "outshine/ground-port-path/1";
  Sha256Prefix hash(version.data(), version.size());
  hash.Append(whole.ContentBinding.data(), whole.ContentBinding.size());
  Word(hash, recipe.PathId);
  Text(hash, recipe.Material);
  Word(hash, static_cast<uint32_t>(recipe.Use));
  Word(hash, recipe.SpeedMps);
  Word(hash, recipe.Priority);
  Word(hash, static_cast<uint32_t>(RoadProfile::Simple));
  return {.DatasetId = "outshine/ground-port-path",
          .Version = "1",
          .Revision = "1",
          .ItemId = std::string(sourceId),
          .Content = hash.Digest(nullptr, 0)};
}

Checked<void> BindInputs(TransportPlan &plan,
                         const GroundPortBodies &sliced,
                         const std::vector<Part> &parts,
                         const std::array<uint8_t, 32> &network,
                         GroundPortPlanBudget budget) {
  plan.Inputs.reserve(sliced.Inputs.size() + parts.size() +
                      plan.Network->Provenance().Sources.size() + plan.SourceEnds.size() + 1);
  for (const auto &input : sliced.Inputs) { plan.Inputs.push_back(input); }
  for (const auto &part : parts) { plan.Inputs.push_back(part.Input); }
  for (const auto &end : plan.SourceEnds) { plan.Inputs.push_back(end.Input); }
  for (const auto &source : plan.Network->Provenance().Sources) {
    plan.Inputs.push_back({.DatasetId = source.Identity.DatasetId,
                           .Version = "native-source/1",
                           .Revision = source.Identity.Revision,
                           .ItemId = source.Item,
                           .Content = source.Content});
  }
  plan.Inputs.push_back(ScopeInput(plan, sliced, parts, network));
  std::ranges::sort(plan.Inputs, {}, Key);
  size_t kept = 0;
  for (auto &input : plan.Inputs) {
    if (kept != 0 && Key(plan.Inputs[kept - 1]) == Key(input)) {
      if (plan.Inputs[kept - 1].Content != input.Content) {
        return std::unexpected(TransportPlanCodecError::InvalidInputs);
      }
      continue;
    }
    if (&input != &plan.Inputs[kept]) { plan.Inputs[kept] = std::move(input); }
    ++kept;
  }
  plan.Inputs.resize(kept);
  const auto binding = TransportPlanInputBinding(plan.Inputs, budget.BindingBytes);
  if (!binding) { return std::unexpected(binding.error()); }
  plan.InputBinding = *binding;
  return {};
}
}
