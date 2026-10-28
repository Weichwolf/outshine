#include "ImpostorSurface.h"
#include "Check.h"
#include "HalfTexels.h"
#include "SurfaceState.h"
#include <cstddef>
#include <optional>
#include <string>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  ImpostorAtlas::View view{.TowardEye = {{0, 0, 1}}, .Texels = {}, .Materials = {}};
  view.Texels.resize(16);
  view.Texels[0] = {.Normal = {{0, 0, 1}}, .Depth = .25f, .Surface = 1};
  view.Texels[15] = {.Normal = {{1, 0, 0}}, .Depth = .75f, .Surface = 2};
  Material first;
  first.Roughness = .5f;
  Material second;
  second.Roughness = .75f;
  std::string error;
  const auto atlas = ImpostorAtlas::Create(4, {}, 2, {first, second}, {view}, error);
  CHECK(atlas.has_value(), "two sparse opaque sources form a valid atlas");
  if (!atlas) { return Report(); }
  CHECK(!Render::BuildFractionalImpostorSurface(*atlas, 1),
        "a missing source view produces no invented card");
  auto card = Render::BuildFractionalImpostorSurface(*atlas, 0);
  CHECK(card && card->wellFormed() && card->parts() == 1 && card->images() == 5,
        "fractional coverage owns only its five joint moment images");
  if (!card) { return Report(); }
  CHECK(card->positionsOf(0).size() == 12 && card->trianglesOf(0).size() == 6,
        "sparse sources retain closed four-corner support without depth patches");
  const auto &material = card->surfaceAt(card->materialOf(0));
  CHECK(material.Alpha == AlphaMode::Blended &&
            material.Scattering == ScatteringModel::AreaMixture && !StateOf(material).WritesDepth(),
        "fractional support is an optical mixture without opaque depth authority");
  size_t bytes = 0;
  for (int at = 0; at < card->images(); ++at) {
    const auto image = card->imageAt(at);
    bytes += image.Rgba.size();
    for (const auto &mip : image.LowerMips) {
      if (mip) { bytes += mip->size(); }
    }
  }
  CHECK(bytes == (size_t{16} + 4 + 1) * 24,
        "four unorm maps and one half-float map include every mip exactly once");
  const auto colour = card->imageAt(material.BaseColourMap.Image);
  const auto &colourMips = colour.LowerMips[static_cast<size_t>(ImageMipKind::Colour)];
  CHECK(colourMips && colourMips->size() == 20 && colourMips->back() == 32,
        "two occupied texels retain their one-eighth area in the final colour mip");
  const auto moments = card->imageAt(material.MetalRoughMap.Image);
  const auto &momentMips = moments.LowerMips[static_cast<size_t>(ImageMipKind::Linear)];
  CHECK(momentMips && momentMips->size() == 40, "all material moment mips are retained");
  if (momentMips && momentMips->size() == 40) {
    const auto value = Core::UnpackHalfTexels({.WidthPx = 1, .HeightPx = 1}, momentMips->last(8));
    CHECK(value && (*value)[3] == .125f && (*value)[1] == .023681640625f,
          "covered area and the two roughness fourth moments are jointly filtered");
  }
  ImpostorCards cards;
  cards.Centre = atlas->CentreM();
  cards.HalfExtentM = atlas->HalfExtentM();
  cards.Views.push_back({.Direction = view.TowardEye, .Surface = std::move(*card)});
  constexpr size_t limit = size_t{1024} * 1024;
  const auto encoded = cards.Encode("fractional-card-1", limit);
  const auto decoded =
      encoded ? ImpostorCards::Decode(*encoded, "fractional-card-1", limit) : std::nullopt;
  CHECK(decoded &&
            decoded->Views[0].Surface.surfaceAt(MaterialInstance(0)).Alpha == AlphaMode::Blended &&
            decoded->Views[0].Surface.images() == 5,
        "cached cards retain the fractional contract and native moment images");
  return Report();
}
