#include "StoredTransportRoadPageProducer.h"
#include "TransportRoadPageProducer.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <stop_token>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace outshine::Generators {
std::expected<StoredTransportRoadPageProducer, std::string>
StoredTransportRoadPageProducer::Bind(std::shared_ptr<PreparedTransportPlans> plans,
                                      const TransportPlanDefinition &definition,
                                      const std::array<uint8_t, 32> &inputs,
                                      const RoadPageAddress &address,
                                      std::vector<RoadPagePartAppearance> parts,
                                      StoredTransportRoadPageLimits limits) {
  const auto planKey = TransportPlanContentKey(definition, inputs);
  const auto key = RoadPageKey(address);
  const auto *appearance = std::get_if<RoadPageAppearanceV2>(&address.Appearance);
  if (!plans || limits.Plan.PackageBytesMost == 0 || limits.Plan.DecodedCapacityMost == 0 ||
      limits.ReadBufferBytesMost == 0 || address.Level != 0 || address.MaterialIndex != 0 ||
      !planKey || *planKey != address.PlanKey || key.empty() || appearance == nullptr ||
      appearance->Key != RoadPageAppearanceKey(parts) ||
      !std::ranges::equal(address.Parts, parts, {}, {}, &RoadPagePartAppearance::Part)) {
    return std::unexpected("stored road producer needs exact plan, appearance and decode budgets");
  }
  size_t bytes = sizeof(StoredTransportRoadPageProducer) + sizeof(TransportRoadPageProducer);
  const std::array capacities{limits.ReadBufferBytesMost,
                              limits.Plan.DecodedCapacityMost,
                              key.capacity(),
                              key.capacity(),
                              parts.capacity() * sizeof(RoadPagePartAppearance),
                              parts.capacity() * sizeof(RoadPagePartAppearance)};
  for (const auto capacity : capacities) {
    if (capacity > std::numeric_limits<size_t>::max() - bytes) {
      return std::unexpected("stored road producer input capacity overflows");
    }
    bytes += capacity;
  }
  return StoredTransportRoadPageProducer(
      std::move(plans), definition, inputs, key, std::move(parts), limits, bytes);
}

StoredTransportRoadPageProducer::StoredTransportRoadPageProducer(
    std::shared_ptr<PreparedTransportPlans> plans,
    TransportPlanDefinition definition,
    std::array<uint8_t, 32> inputs,
    std::string key,
    std::vector<RoadPagePartAppearance> parts,
    StoredTransportRoadPageLimits limits,
    size_t inputBytes)
    : Plans_(std::move(plans)),
      Definition_(definition),
      Inputs_(inputs),
      ProductKey_(std::move(key)),
      Parts_(std::move(parts)),
      Limits_(limits),
      InputBytes_(inputBytes) {}

std::expected<RoadPageGeometry, std::string>
StoredTransportRoadPageProducer::Produce(const RoadPageRequest &request,
                                         std::stop_token stop) const {
  if (stop.stop_requested()) { return std::unexpected("stored road page generation canceled"); }
  if (RoadPageKey(request.Address) != ProductKey_) {
    return std::unexpected("stored road producer does not own the requested product");
  }
  if (request.Address.PositionErrorM && !request.SweepLimits) {
    return std::unexpected("road display generation needs an explicit sampling budget");
  }
  const auto loaded =
      Plans_->LoadProduct(Definition_, Inputs_, Limits_.Plan, Limits_.ReadBufferBytesMost);
  if (stop.stop_requested()) { return std::unexpected("stored road page generation canceled"); }
  if (!loaded) {
    if (loaded.error() == TransportPlanAssetError::CapacityExceeded) {
      return std::unexpected("stored road plan exceeds its input capacity");
    }
    return std::unexpected("stored road plan could not be read");
  }
  if (!*loaded) { return std::unexpected("stored road plan is missing or invalid"); }
  const auto producer = TransportRoadPageProducer::Bind((**loaded).Plan, request.Address, Parts_);
  if (!producer) { return std::unexpected(producer.error()); }
  return producer->Produce(request, stop);
}
}
