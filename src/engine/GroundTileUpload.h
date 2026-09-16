#ifndef OUTSHINE_ENGINE_GROUNDTILEUPLOAD_H
#define OUTSHINE_ENGINE_GROUNDTILEUPLOAD_H

#include "GroundTile.h"
#include "SubjectTypes.h"
#include <expected>
#include <string_view>

namespace outshine::Core {
namespace Says {
inline constexpr std::string_view GroundPageEncoding =
    "GPU height-page address cannot be encoded exactly";
}

[[nodiscard]] constexpr std::expected<Render::GroundTile, std::string_view>
EncodeGroundTile(const GroundTile &tile, Render::PageId resident) noexcept {
  const auto encoded = static_cast<float>(resident);
  if (resident == Render::kNoPage ||
      static_cast<double>(encoded) != static_cast<double>(resident)) {
    return std::unexpected(Says::GroundPageEncoding);
  }
  return Render::GroundTile{.Instance = {.Row = tile.Row,
                                         .Corners = tile.Corners,
                                         .Page = encoded,
                                         .SagInv = tile.SagInv,
                                         .StepE = tile.StepE,
                                         .StepN = tile.StepN},
                            .LowM = tile.LowM,
                            .HighM = tile.HighM};
}
}
#endif
