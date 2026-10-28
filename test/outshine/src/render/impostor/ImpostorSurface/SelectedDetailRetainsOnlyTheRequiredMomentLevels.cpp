#include "ImpostorSurface.h"
#include "Check.h"
#include "Material.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <utility>

namespace {
void CheckImages(const outshine::Geometry &full, const outshine::Geometry &coarse) {
  using namespace outshine;
  using namespace outshine::Test;
  for (int image = 0; image < full.images(); ++image) {
    const auto original = full.imageAt(image);
    const auto selected = coarse.imageAt(image);
    const int width = selected.WidthPx;
    const int pixels = original.WidthPx;
    CHECK(selected.HeightPx == width && selected.Format == original.Format,
          "selected images preserve square dimensions and pixel encoding");
    const size_t stride = original.Format == ImagePixelFormat::Rgba16Float ? 8 : 4;
    const size_t baseBytes = static_cast<size_t>(width) * width * stride;
    for (size_t kind = 0; kind < original.LowerMips.size(); ++kind) {
      const auto &originalLower = original.LowerMips[kind];
      if (!originalLower) { continue; }
      auto remainder = *originalLower;
      for (int skip = pixels / 2; skip > width; skip /= 2) {
        remainder = remainder.subspan(static_cast<size_t>(skip) * skip * stride);
      }
      CHECK(std::ranges::equal(selected.Rgba, remainder.first(baseBytes)),
            "selected base equals the reference filtered moment level byte for byte");
      const auto tail = remainder.subspan(baseBytes);
      const auto selectedLower = selected.LowerMips[kind].value_or(std::span<const uint8_t>{});
      CHECK(std::ranges::equal(selectedLower, tail),
            "remaining lower levels preserve coverage, normals and optical moments exactly");
    }
  }
}

void CheckLevels(int pixels) {
  using namespace outshine;
  using namespace outshine::Test;
  ImpostorAtlas::View view{.TowardEye = {{0, 0, 1}}, .Texels = {}, .Materials = {}};
  view.Texels.resize(static_cast<size_t>(pixels) * pixels);
  for (size_t at = 0; at < view.Texels.size(); ++at) {
    if (at % 3 == 0) { continue; }
    view.Texels[at] = {.Normal = at % 2 == 0 ? Vec3f{{0, 0, 1}} : Vec3f{{1, 0, 0}},
                       .Depth = .5f,
                       .Surface = static_cast<uint32_t>(at % 2 + 1)};
  }
  Material opaque;
  opaque.Roughness = .5f;
  Material leaf;
  leaf.Scattering = ScatteringModel::DiffuseThinSheet;
  leaf.DiffuseTransmission = {{.25f, .5f, .125f}};
  std::string error;
  const auto atlas =
      ImpostorAtlas::Create(pixels, {{1, 2, 3}}, 4, {opaque, leaf}, {std::move(view)}, error);
  CHECK(atlas.has_value(), "mixed opaque and thin-sheet sources form a valid fixed view");
  if (!atlas) { return; }
  const auto full = Render::BuildFractionalImpostorSurface(*atlas, 0);
  CHECK(full.has_value(), "reference retains every joint moment level");
  if (!full) { return; }
  size_t level = 1;
  for (int width = pixels / 2; width >= 1; width /= 2, ++level) {
    const auto coarse = Render::BuildFractionalImpostorSurface(*atlas, 0, level);
    CHECK(coarse && coarse->wellFormed() && coarse->images() == full->images(),
          "selected detail retains all five material channels");
    if (!coarse) { return; }
    CHECK(std::ranges::equal(coarse->positionsOf(0), full->positionsOf(0)) &&
              std::ranges::equal(coarse->trianglesOf(0), full->trianglesOf(0)),
          "texture detail does not change card support or geometry");
    CHECK(coarse->imageAt(0).WidthPx == width, "only requested dimensions are retained");
    CheckImages(*full, *coarse);
  }
  CHECK(!Render::BuildFractionalImpostorSurface(*atlas, 0, level) &&
            !Render::BuildFractionalImpostorSurface(*atlas, 0, std::numeric_limits<size_t>::max()),
        "out-of-range detail fails before source moment allocation");
}
}

int main() {
  CheckLevels(8);
  CheckLevels(7);
  return outshine::Test::Report();
}
