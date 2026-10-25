#ifndef OUTSHINE_CONTENT_ASSETS_ASSETPUBLICATION_H
#define OUTSHINE_CONTENT_ASSETS_ASSETPUBLICATION_H
#include "AssetCache.h"
#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace outshine {
struct AssetProductKey {
  std::array<uint8_t, 32> Digest{};
  [[nodiscard]] static std::optional<AssetProductKey> Parse(std::string_view hex) noexcept;
  [[nodiscard]] auto operator<=>(const AssetProductKey &) const = default;
};

struct AssetConsumerId {
  uint64_t Value = 0;
  auto operator<=>(const AssetConsumerId &) const = default;
};

struct AssetConsumerGeneration {
  uint64_t Value = 1;
  auto operator<=>(const AssetConsumerGeneration &) const = default;
};
enum class AssetPublicationError : uint8_t {
  InvalidRequest,
  ResourceDeferred,
  StaleGeneration,
  InvalidReceipt,
  ProducerFailed,
  DispatcherStopped
};
enum class AssetPublicationStatus : uint8_t { Pending, Ready, Failed, Cancelled };

struct AssetPublication {
  AssetPublicationStatus Status = AssetPublicationStatus::Pending;
  std::shared_ptr<const AssetRecord> Receipt;
  AssetPublicationError Error = AssetPublicationError::InvalidRequest;
  std::string Message;
};
}
#endif
