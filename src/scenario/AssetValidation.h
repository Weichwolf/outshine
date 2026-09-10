#ifndef OUTSHINE_SCENARIO_ASSETVALIDATION_H
#define OUTSHINE_SCENARIO_ASSETVALIDATION_H
#include <scenario/Scenario.h>
#include <expected>
#include <string_view>

namespace outshine {
namespace Says {
inline constexpr std::string_view kNegativeAssetClip = "asset clip must be nonnegative";
inline constexpr std::string_view kInvalidAssetAnimation = "asset animation mode is invalid";
}

[[nodiscard]] inline std::expected<void, std::string_view>
ValidateAssetPlayback(const Scenario::Asset &asset) noexcept {
  if (asset.Clip < 0) { return std::unexpected(Says::kNegativeAssetClip); }
  switch (asset.Animation) {
    case Scenario::AssetAnimation::Play:
    case Scenario::AssetAnimation::Loop:
    case Scenario::AssetAnimation::Ignore:
    case Scenario::AssetAnimation::Driven: return {};
  }
  return std::unexpected(Says::kInvalidAssetAnimation);
}
}
#endif
