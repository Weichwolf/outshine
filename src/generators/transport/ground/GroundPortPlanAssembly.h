#ifndef OUTSHINE_GENERATORS_TRANSPORT_GROUND_GROUNDPORTPLANASSEMBLY_H
#define OUTSHINE_GENERATORS_TRANSPORT_GROUND_GROUNDPORTPLANASSEMBLY_H
#include "GroundPortPlan.h"
#include <array>
#include <cstddef>
#include <expected>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Generators::GroundPlan {
template <class T> using Checked = std::expected<T, GroundPortPlanError>;

struct Part {
  size_t Body = 0;
  World::TransportEdgeId Source;
  TransportPath Path;
  TransportInputReference Input;
  std::array<TransportCoreEnd, 2> Ends{};
  std::array<uint64_t, 2> SourceNodes{};
  std::array<RoadPortFrameSample, 2> Samples{};
};

[[nodiscard]] Checked<std::vector<Part>>
PrepareParts(const GroundPortField &field,
             const GroundPortBodies &whole,
             const GroundPortBodies &sliced,
             const TransportCoreSelection &selection,
             std::span<const GroundPortPlanPathRecipe> recipes,
             GroundPortFieldQueryWork &sampleWork,
             GroundPortPlanBudget budget,
             const std::stop_token &stop);

[[nodiscard]] Checked<void> AppendProfiles(TransportPlan &plan,
                                           const GroundPortField &field,
                                           std::vector<Part> &parts,
                                           GroundPortBodyRecipe recipe,
                                           GroundPortPlanBudget budget);

[[nodiscard]] Checked<void> AppendSourceEnds(TransportPlan &plan,
                                             const GroundPortField &field,
                                             std::vector<Part> &parts,
                                             GroundPortBodyRecipe recipe,
                                             GroundPortPlanBudget budget);

[[nodiscard]] Checked<void> BindInputs(TransportPlan &plan,
                                       const GroundPortBodies &sliced,
                                       const std::vector<Part> &parts,
                                       const std::array<uint8_t, 32> &network,
                                       GroundPortPlanBudget budget);

[[nodiscard]] TransportInputReference PathInput(const GroundPortBodies &whole,
                                                const GroundPortPlanPathRecipe &recipe,
                                                std::string_view sourceId);
}
#endif
