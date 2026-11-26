#include "Check.h"
#include "GroupImpostorView.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  GroupImpostorView square;
  square.Side = 128;
  square.TowardEye = {{0, 0, 1}};
  square.Patches = {
      {.Left = 40, .Top = 96, .Right = 56, .Bottom = 104, .Depth = 0.5f, .SlopeX = 0, .SlopeY = 0}};
  const auto pixels = GroupImpostorView::MipTexels(square.Side);
  square.AlbedoCoverage.resize(pixels);
  square.NormalMaterial.resize(pixels);
  for (size_t i = 0; i < pixels; ++i) {
    square.AlbedoCoverage[i][0] = static_cast<uint8_t>(i);
    square.NormalMaterial[i][0] = static_cast<uint8_t>(i >> 8);
  }
  const auto legacy = EncodeGroupImpostorView(square);
  auto trimmed = square;
  CHECK(CropGroupImpostorView(trimmed), "compact a narrow occupied view");
  const auto stored = EncodeGroupImpostorView(trimmed);
  CHECK(legacy && stored && stored->size() < legacy->size(), "compact package saves storage");
  if (!stored) { return Report(); }
  const auto decoded = DecodeGroupImpostorView(*stored);
  CHECK(decoded && decoded->Crop, "cropped view round-trips through the asset format");
  if (!decoded || !decoded->Crop) { return Report(); }
  const auto &crop = *decoded->Crop;
  size_t from = 0;
  size_t to = 0;
  for (uint32_t level = 0; level < crop.Levels; ++level) {
    const size_t side = square.Side >> level;
    const size_t width = crop.Width >> level;
    const size_t height = crop.Height >> level;
    const auto start = from + (crop.Y >> level) * side + (crop.X >> level);
    for (size_t y = 0; y < height; ++y) {
      const auto source = start + y * side;
      const auto target = to + y * width;
      CHECK(std::ranges::equal(std::span(square.AlbedoCoverage).subspan(source, width),
                               std::span(decoded->AlbedoCoverage).subspan(target, width)) &&
                std::ranges::equal(std::span(square.NormalMaterial).subspan(source, width),
                                   std::span(decoded->NormalMaterial).subspan(target, width)),
            "retained filter and normal samples remain the same at every mip");
    }
    from += side * side;
    to += width * height;
  }
  CHECK(!DecodeGroupImpostorView(std::span(*stored).first(stored->size() - 1)),
        "truncated mip chain falls back to source geometry");
  auto unaligned = crop;
  ++unaligned.X;
  trimmed.Crop = unaligned;
  CHECK(!EncodeGroupImpostorView(trimmed), "crop offset must align at the coarsest retained mip");
  return Report();
}
