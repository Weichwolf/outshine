#ifndef OUTSHINE_RENDER_SCENE_TERRAINTILEUPLOAD_H
#define OUTSHINE_RENDER_SCENE_TERRAINTILEUPLOAD_H

#include "SubjectTypes.h"
#include "TerrainTile.h"

#include <expected>
#include <string_view>

namespace outshine::Render {
namespace Says {
inline constexpr std::string_view GroundPageUnavailable =
    "GPU height-page handle has no resident page";
inline constexpr std::string_view GroundPageEncoding =
    "GPU height-page address cannot be encoded exactly";
}

[[nodiscard]] constexpr std::expected<GroundTile, std::string_view>
EncodeTerrainTile(const TerrainTile &tile, PageId resident) noexcept {
  const auto encoded = static_cast<float>(resident);
  if (resident == kNoPage) { return std::unexpected(Says::GroundPageUnavailable); }
  if (static_cast<double>(encoded) != static_cast<double>(resident)) {
    return std::unexpected(Says::GroundPageEncoding);
  }
  return GroundTile{.Instance = {.Row = tile.Row,
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
