#include "TransportPlanFormat.h"
#include "TransportNetworkSnapshot.h"
#include "TransportProvenance.h"
#include <algorithm>
#include <span>
#include <tuple>

namespace outshine::Generators::TransportFormat {
namespace {
bool SourceBound(const World::TransportSource &source,
                 std::span<const TransportInputReference> inputs) {
  const auto dataset = std::ranges::equal_range(
      inputs, source.Identity.DatasetId, {}, &TransportInputReference::DatasetId);
  auto first = dataset.begin();
  while (first != dataset.end()) {
    const auto end = std::ranges::upper_bound(
        first, dataset.end(), first->Version, {}, &TransportInputReference::Version);
    const auto key = std::tie(source.Identity.Revision, source.Item);
    const auto match = std::ranges::lower_bound(first, end, key, {}, [](const auto &input) {
      return std::tie(input.Revision, input.ItemId);
    });
    if (match != end && match->Revision == source.Identity.Revision &&
        match->ItemId == source.Item && match->Content == source.Content) {
      return true;
    }
    first = end;
  }
  return false;
}
}

bool NetworkInputsBound(const TransportPlan &plan) {
  return !plan.Network || std::ranges::all_of(plan.Network->Provenance().Sources,
                                              [&plan](const World::TransportSource &source) {
                                                return SourceBound(source, plan.Inputs);
                                              });
}
}
