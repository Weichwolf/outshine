#include "PreparedTransportPlans.h"
#include <array>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

namespace outshine::Generators {
std::expected<std::optional<TransportPlanAsset>, TransportPlanAssetError>
PreparedTransportPlans::LoadRequest(const TransportPlanAddress &address,
                                    TransportPlanCodecLimits limits) {
  const auto key = TransportPlanRequestKey(address);
  if (!key) { return std::unexpected(TransportPlanAssetError::InvalidRequest); }
  auto cached = [&] {
    const std::scoped_lock lock(Lock_);
    return Cache_->LoadRequest(*key, limits.PackageBytesMost);
  }();
  return Decode(std::move(cached), address.Definition, limits);
}

std::expected<std::optional<TransportPlanAsset>, TransportPlanAssetError>
PreparedTransportPlans::LoadProduct(const TransportPlanDefinition &definition,
                                    const std::array<uint8_t, 32> &inputBinding,
                                    TransportPlanCodecLimits limits) {
  const auto key = TransportPlanContentKey(definition, inputBinding);
  if (!key) { return std::unexpected(TransportPlanAssetError::InvalidRequest); }
  auto cached = [&] {
    const std::scoped_lock lock(Lock_);
    return Cache_->Load(*key, limits.PackageBytesMost);
  }();
  return Decode(std::move(cached), definition, limits);
}

std::expected<std::optional<TransportPlanAsset>, TransportPlanAssetError>
PreparedTransportPlans::Decode(std::expected<std::optional<CachedAsset>, AssetCacheError> cached,
                               const TransportPlanDefinition &definition,
                               TransportPlanCodecLimits limits) {
  if (!cached) {
    return std::unexpected(cached.error() == AssetCacheError::CapacityExceeded
                               ? TransportPlanAssetError::CapacityExceeded
                               : TransportPlanAssetError::Storage);
  }
  if (!*cached) { return std::nullopt; }
  const auto &package = **cached;
  const auto &record = package.Record();
  if (record.Kind != "functional-transport-plan") {
    return std::unexpected(TransportPlanAssetError::WrongProduct);
  }
  auto plan = DecodeTransportPlan(package.Bytes(), limits);
  if (!plan && plan.error() == TransportPlanCodecError::CapacityExceeded) {
    return std::unexpected(TransportPlanAssetError::CapacityExceeded);
  }
  if (!plan || !Admits(*plan)) { return std::nullopt; }
  const auto key = TransportPlanContentKey(plan->Definition, plan->InputBinding);
  if (!key || *key != record.Key || std::cmp_not_equal(record.Level, plan->Definition.Core.Level)) {
    return std::nullopt;
  }
  if (plan->Definition != definition) {
    return std::unexpected(TransportPlanAssetError::WrongProduct);
  }
  return TransportPlanAsset{.Plan = std::make_shared<const TransportPlan>(std::move(*plan)),
                            .Record = record};
}

}
