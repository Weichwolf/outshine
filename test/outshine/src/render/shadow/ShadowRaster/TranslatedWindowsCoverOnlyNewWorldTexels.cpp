#include "Check.h"
#include "ShadowRaster.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
constexpr uint32_t kSide = 8;
constexpr size_t kCells = static_cast<size_t>(kSide) * kSide;
constexpr uint32_t kResizedSide = 16;
constexpr Vec3 kMovedAnchor{{10, 20, 30}};
constexpr double kChangedDepth = 0.25;
constexpr double kPartialTexelTranslation = 0.125;
constexpr int kOffsetBeyondHistory = 1'000'001;

struct WindowMotion {
  std::array<int64_t, 2> Previous{};
  std::array<int64_t, 2> Current{};
  bool Full = false;
};

Mat4 Projection(std::array<int, 2> offset) {
  Mat4 matrix;
  matrix[12] = 2.0 * offset[0] / kSide;
  matrix[13] = -2.0 * offset[1] / kSide;
  return matrix;
}

void Verify(const ShadowRasterUpdate &update, const WindowMotion &motion) {
  const auto &previous = motion.Previous;
  const auto &current = motion.Current;
  std::array<unsigned, kCells> written{};
  uint64_t texels = 0;
  bool valid = true;
  for (size_t at = 0; at < update.Count; ++at) {
    const auto &patch = update.Patches[at];
    valid = valid && patch.Width > 0 && patch.Height > 0 && patch.X + patch.Width <= kSide &&
            patch.Y + patch.Height <= kSide;
    for (uint32_t y = patch.Y; y < patch.Y + patch.Height; ++y) {
      for (uint32_t x = patch.X; x < patch.X + patch.Width; ++x) {
        ++written[static_cast<size_t>(y) * kSide + x];
        const int64_t wx = patch.Period[0] * kSide + x;
        const int64_t wy = patch.Period[1] * kSide + y;
        valid = valid && wx >= -current[0] && wx < kSide - current[0] && wy >= -current[1] &&
                wy < kSide - current[1];
      }
    }
  }
  for (uint32_t y = 0; y < kSide; ++y) {
    for (uint32_t x = 0; x < kSide; ++x) {
      const int64_t wx = static_cast<int64_t>(x) - current[0];
      const int64_t wy = static_cast<int64_t>(y) - current[1];
      const bool shared = wx >= -previous[0] && wx < kSide - previous[0] && wy >= -previous[1] &&
                          wy < kSide - previous[1];
      const bool needed = motion.Full || !shared;
      const uint32_t px = (x + update.Origin[0]) % kSide;
      const uint32_t py = (y + update.Origin[1]) % kSide;
      valid = valid && written[static_cast<size_t>(py) * kSide + px] == (needed ? 1u : 0u);
      texels += needed ? 1u : 0u;
    }
  }
  CHECK(valid && update.Texels == texels, "patches cover exactly the new world cells once");
}

void Translations() {
  const auto initial = PrepareShadowRaster({}, {}, {}, kSide, false);
  CHECK(initial.Texels == kCells, "first build covers the complete tile");
  for (int py = -2; py <= 2; ++py) {
    for (int px = -2; px <= 2; ++px) {
      const auto accepted =
          PrepareShadowRaster(initial.Next, Projection({px, py}), {}, kSide, false);
      for (int y = -9; y <= 9; ++y) {
        for (int x = -9; x <= 9; ++x) {
          const auto update =
              PrepareShadowRaster(accepted.Next, Projection({x, y}), {}, kSide, false);
          CHECK(update.Next.Offset == (std::array<int64_t, 2>{x, y}),
                "both signed axes retain exact global offsets");
          CHECK(update.Next.Packed == initial.Next.Packed,
                "camera translation preserves the fixed physical raster matrix");
          Verify(update, {.Previous = {px, py}, .Current = {x, y}});
        }
      }
    }
  }
  const auto full = PrepareShadowRaster(initial.Next, Projection({3, -5}), {}, kSide, true);
  CHECK(full.Next.Packed == initial.Next.Packed,
        "content invalidation fully redraws the existing physical mapping");
  Verify(full, {.Current = {3, -5}, .Full = true});
}

void Resets() {
  const auto initial = PrepareShadowRaster({}, {}, {}, kSide, false);
  const auto full = [&](const Mat4 &matrix, const Vec3 &anchor, uint32_t side) {
    const auto update = PrepareShadowRaster(initial.Next, matrix, anchor, side, false);
    CHECK(update.Texels == static_cast<uint64_t>(side) * side &&
              update.Origin == (std::array<uint32_t, 2>{}) &&
              update.Next.Offset == (std::array<int64_t, 2>{}) && update.Next.Projection == matrix,
          "changed raster inputs reset and completely rebuild the current mapping");
  };
  full({}, {}, kResizedSide);
  full({}, kMovedAnchor, kSide);
  Mat4 matrix;
  matrix[0] = 0.5;
  full(matrix, {}, kSide);
  matrix = {};
  matrix[14] = kChangedDepth;
  full(matrix, {}, kSide);
  matrix = {};
  matrix[12] = kPartialTexelTranslation;
  full(matrix, {}, kSide);
  full(Projection({kOffsetBeyondHistory, 0}), {}, kSide);
}
}

int main() {
  Translations();
  Resets();
  return Report();
}
