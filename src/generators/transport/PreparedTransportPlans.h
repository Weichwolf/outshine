#ifndef OUTSHINE_GENERATORS_TRANSPORT_PREPAREDTRANSPORTPLANS_H
#define OUTSHINE_GENERATORS_TRANSPORT_PREPAREDTRANSPORTPLANS_H

#include "AssetCache.h"
#include "TransportPlanCodec.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>

namespace outshine::Generators {
enum class TransportPlanAssetError {
  InvalidRequest,
  InvalidPlan,
  CapacityExceeded,
  Storage,
  Canceled,
  WrongProduct
};

struct TransportPlanAsset {
  TransportPlanLease Plan;
  AssetRecord Record;
};

class PreparedTransportPlans {
public:
  [[nodiscard]] static std::expected<std::unique_ptr<PreparedTransportPlans>,
                                     TransportPlanAssetError>
  Open(std::shared_ptr<AssetCache> cache);

  [[nodiscard]] std::expected<std::optional<TransportPlanAsset>, TransportPlanAssetError>
  LoadRequest(const TransportPlanAddress &address, TransportPlanCodecLimits limits);

  [[nodiscard]] std::expected<std::optional<TransportPlanAsset>, TransportPlanAssetError>
  LoadProduct(const TransportPlanDefinition &definition,
              const std::array<uint8_t, 32> &inputBinding,
              TransportPlanCodecLimits limits);
  [[nodiscard]] std::expected<std::optional<TransportPlanAsset>, TransportPlanAssetError>
  LoadProduct(const TransportPlanDefinition &definition,
              const std::array<uint8_t, 32> &inputBinding,
              TransportPlanCodecLimits limits,
              size_t readBufferBytesMost);

  [[nodiscard]] std::expected<AssetRecord, TransportPlanAssetError>
  StoreComplete(const TransportPlan &plan,
                const Box &bounds,
                TransportPlanCodecLimits limits,
                const std::stop_token &stop = {});

  [[nodiscard]] std::expected<void, TransportPlanAssetError>
  BindRequest(const TransportPlanAddress &address, const TransportPlanAsset &product);

private:
  explicit PreparedTransportPlans(std::shared_ptr<AssetCache> cache);
  [[nodiscard]] static std::expected<std::optional<TransportPlanAsset>, TransportPlanAssetError>
  Decode(std::expected<std::optional<CachedAsset>, AssetCacheError> cached,
         const TransportPlanDefinition &definition,
         TransportPlanCodecLimits limits);
  std::shared_ptr<AssetCache> Cache_;
  std::mutex Lock_;
};
}
#endif
