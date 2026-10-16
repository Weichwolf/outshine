#include "LowCloud.h"
#include "math/RenderFrame.h"

#include <cmath>
#include <expected>
#include <optional>

namespace outshine::Generators::Weather {
namespace {
bool Valid(const LowCloudRecipe &recipe) noexcept {
  return std::isfinite(recipe.BaseAslM) && recipe.BaseAslM >= 0.0 &&
         std::isfinite(recipe.ThicknessM) && recipe.ThicknessM > 0.0 &&
         std::isfinite(recipe.ExtinctionPerM) && recipe.ExtinctionPerM >= 0.0 &&
         std::isfinite(recipe.AdvectionEastMs) && std::isfinite(recipe.AdvectionNorthMs) &&
         (recipe.Profile == CloudProfile::Stratus || recipe.Profile == CloudProfile::CumulusV1);
}
}

std::expected<std::optional<LowCloudState>, LowCloudFailure>
MakeLowCloud(const WeatherSnapshot &snapshot,
             const std::optional<LowCloudRecipe> &recipe) noexcept {
  const auto cover = snapshot.Values.CloudLow;
  if (!cover) { return std::nullopt; }
  if (!std::isfinite(*cover) || *cover < 0.0 || *cover > 1.0) {
    return std::unexpected(LowCloudFailure::InvalidCover);
  }
  LowCloudState state{.ValidUtcS = snapshot.Values.ValidUtcS};
  if (*cover == 0.0) { return state; }
  if (!recipe) { return std::unexpected(LowCloudFailure::MissingRecipe); }
  if (!Valid(*recipe)) { return std::unexpected(LowCloudFailure::InvalidRecipe); }
  state.Layer.BaseM = recipe->BaseAslM;
  state.Layer.ThicknessM = recipe->ThicknessM;
  state.Layer.ExtinctionPerM = recipe->ExtinctionPerM;
  state.Layer.Cover = *cover;
  state.Layer.Profile = recipe->Profile;
  state.Layer.WindMs = Vec3{RenderFrame::Of(
      {.EastM = recipe->AdvectionEastMs, .NorthM = recipe->AdvectionNorthMs, .UpM = 0.0})};
  state.ProceduralGeometry = true;
  state.ProceduralAdvection = true;
  return state;
}

}
