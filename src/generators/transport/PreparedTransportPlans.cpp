#include "PreparedTransportPlans.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <utility>

namespace outshine::Generators {
PreparedTransportPlans::PreparedTransportPlans(std::unique_ptr<AssetCache> cache)
    : Cache_(std::move(cache)) {}

bool PreparedTransportPlans::Admits(const TransportPlan &plan) noexcept {
  if (!ValidTransportRepresentation(plan.Definition)) { return false; }
  const auto coverage = plan.Definition.Representation == TransportRepresentation::FunctionalSubset
                            ? TransportCoverage::Partial
                            : TransportCoverage::Complete;
  return plan.Coverage == coverage;
}

std::expected<std::unique_ptr<PreparedTransportPlans>, TransportPlanAssetError>
PreparedTransportPlans::Open(const std::string &directory) {
  auto cache = AssetCache::Open((std::filesystem::path(directory) / "assets.sqlite").string());
  if (!cache) { return std::unexpected(TransportPlanAssetError::Storage); }
  return std::unique_ptr<PreparedTransportPlans>(new PreparedTransportPlans(std::move(*cache)));
}

std::expected<AssetRecord, TransportPlanAssetError>
PreparedTransportPlans::Store(const TransportPlan &plan,
                              const Box &bounds,
                              TransportPlanCodecLimits limits,
                              const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(TransportPlanAssetError::Canceled); }
  if (!Admits(plan)) { return std::unexpected(TransportPlanAssetError::InvalidPlan); }
  const auto key = TransportPlanContentKey(plan.Definition, plan.InputBinding);
  if (!key) { return std::unexpected(TransportPlanAssetError::InvalidPlan); }
  const auto bytes = EncodeTransportPlan(plan, limits);
  if (!bytes) {
    return std::unexpected(bytes.error() == TransportPlanCodecError::CapacityExceeded
                               ? TransportPlanAssetError::CapacityExceeded
                               : TransportPlanAssetError::InvalidPlan);
  }
  AssetRecord record{.Key = *key,
                     .Kind = "functional-transport-plan",
                     .Bounds = bounds,
                     .Package = {},
                     .ByteCount = bytes->size(),
                     .Level = static_cast<uint32_t>(plan.Definition.Core.Level),
                     .Parent = {}};
  const std::scoped_lock lock(Lock_);
  if (stop.stop_requested()) { return std::unexpected(TransportPlanAssetError::Canceled); }
  if (!Cache_->Publish(std::span{&record, size_t{1}}, *bytes)) {
    return std::unexpected(TransportPlanAssetError::Storage);
  }
  auto published = Cache_->Find(*key);
  if (!published || !*published) { return std::unexpected(TransportPlanAssetError::Storage); }
  return std::move(**published);
}

std::expected<void, TransportPlanAssetError>
PreparedTransportPlans::BindRequest(const TransportPlanAddress &address,
                                    const TransportPlanAsset &product) {
  const auto request = TransportPlanRequestKey(address);
  if (!request) { return std::unexpected(TransportPlanAssetError::InvalidRequest); }
  if (!product.Plan || !Admits(*product.Plan) || product.Plan->Definition != address.Definition ||
      product.Record.Kind != "functional-transport-plan" ||
      std::cmp_not_equal(product.Record.Level, address.Definition.Core.Level)) {
    return std::unexpected(TransportPlanAssetError::WrongProduct);
  }
  const auto key = TransportPlanContentKey(product.Plan->Definition, product.Plan->InputBinding);
  if (!key || *key != product.Record.Key) {
    return std::unexpected(TransportPlanAssetError::WrongProduct);
  }
  const std::scoped_lock lock(Lock_);
  const auto stored = Cache_->Find(*key);
  if (!stored) { return std::unexpected(TransportPlanAssetError::Storage); }
  if (!*stored || (**stored).Kind != product.Record.Kind ||
      (**stored).Package != product.Record.Package ||
      (**stored).OffsetBytes != product.Record.OffsetBytes ||
      (**stored).ByteCount != product.Record.ByteCount ||
      (**stored).Level != product.Record.Level) {
    return std::unexpected(TransportPlanAssetError::WrongProduct);
  }
  const auto bound = Cache_->BindRequest(*key, *request);
  if (!bound) { return std::unexpected(TransportPlanAssetError::Storage); }
  if (!*bound) { return std::unexpected(TransportPlanAssetError::WrongProduct); }
  return {};
}
}
