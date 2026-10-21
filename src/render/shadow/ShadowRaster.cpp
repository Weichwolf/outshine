#include "ShadowRaster.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace outshine::Render {
namespace {
constexpr double kMaximumRasterOffset = 1'000'000;

struct Window {
  int64_t X0, Y0, X1, Y1;
};

int64_t Period(int64_t value, int64_t side) noexcept {
  return value >= 0 ? value / side : -((-value + side - 1) / side);
}

bool Matches(const ShadowRaster &raster,
             const Mat4 &projection,
             const Vec3 &anchor,
             uint32_t side,
             std::array<int64_t, 2> &offset) noexcept {
  if (raster.Side != side || raster.Anchor != anchor) { return false; }
  for (size_t at = 0; at < 16; ++at) {
    if (at != 12 && at != 13 && raster.Projection[at] != projection[at]) { return false; }
  }
  const std::array<double, 2> raw{(projection[12] - raster.Projection[12]) * side / 2,
                                  -(projection[13] - raster.Projection[13]) * side / 2};
  for (size_t axis = 0; axis < raw.size(); ++axis) {
    const double value = raw[axis];
    if (!std::isfinite(value) || std::abs(value) > kMaximumRasterOffset ||
        value != std::round(value)) {
      return false;
    }
    offset[axis] = static_cast<int64_t>(value);
  }
  return true;
}

ShadowRaster Reset(const Mat4 &projection, const Vec3 &anchor, uint32_t side) noexcept {
  ShadowRaster raster{.Projection = projection, .Anchor = anchor, .Side = side};
  Mat4 packed = projection;
  for (size_t row = 0; row < 4; ++row) {
    for (size_t axis = 0; axis < 3; ++axis) {
      packed[12 + row] += projection[4 * axis + row] * anchor[axis];
    }
  }
  for (size_t at = 0; at < 16; ++at) { raster.Packed[at] = static_cast<float>(packed[at]); }
  return raster;
}

void AddPatch(ShadowRasterUpdate &update, const Window &rect, int64_t x, int64_t y) noexcept {
  const int64_t side = update.Next.Side;
  const int64_t x0 = std::max(rect.X0, x * side) - x * side;
  const int64_t y0 = std::max(rect.Y0, y * side) - y * side;
  const int64_t x1 = std::min(rect.X1, (x + 1) * side) - x * side;
  const int64_t y1 = std::min(rect.Y1, (y + 1) * side) - y * side;
  assert(update.Count < update.Patches.size());
  update.Patches[update.Count++] = {.X = static_cast<uint32_t>(x0),
                                    .Y = static_cast<uint32_t>(y0),
                                    .Width = static_cast<uint32_t>(x1 - x0),
                                    .Height = static_cast<uint32_t>(y1 - y0),
                                    .Period = {x, y}};
  update.Texels += static_cast<uint64_t>(x1 - x0) * static_cast<uint64_t>(y1 - y0);
}

void Add(ShadowRasterUpdate &update, const Window &rect) noexcept {
  if (rect.X0 >= rect.X1 || rect.Y0 >= rect.Y1) { return; }
  const int64_t side = update.Next.Side;
  for (int64_t y = Period(rect.Y0, side); y <= Period(rect.Y1 - 1, side); ++y) {
    for (int64_t x = Period(rect.X0, side); x <= Period(rect.X1 - 1, side); ++x) {
      AddPatch(update, rect, x, y);
    }
  }
}

Window Bounds(const ShadowRaster &raster) noexcept {
  return {.X0 = -raster.Offset[0],
          .Y0 = -raster.Offset[1],
          .X1 = raster.Side - raster.Offset[0],
          .Y1 = raster.Side - raster.Offset[1]};
}

}

ShadowRasterUpdate PrepareShadowRaster(const ShadowRaster &accepted,
                                       const Mat4 &projection,
                                       const Vec3 &anchor,
                                       uint32_t side,
                                       bool full) noexcept {
  assert(side > 0);
  ShadowRasterUpdate update;
  std::array<int64_t, 2> offset{};
  const bool matches = Matches(accepted, projection, anchor, side, offset);
  update.Next = matches ? accepted : Reset(projection, anchor, side);
  if (matches) { update.Next.Offset = offset; }
  for (size_t axis = 0; axis < update.Origin.size(); ++axis) {
    update.Origin[axis] = static_cast<uint32_t>(((-update.Next.Offset[axis] % side) + side) % side);
  }
  const Window current = Bounds(update.Next);
  const Window previous = Bounds(accepted);
  const Window shared{.X0 = std::max(current.X0, previous.X0),
                      .Y0 = std::max(current.Y0, previous.Y0),
                      .X1 = std::min(current.X1, previous.X1),
                      .Y1 = std::min(current.Y1, previous.Y1)};
  if (!matches || full || shared.X0 >= shared.X1 || shared.Y0 >= shared.Y1) {
    Add(update, current);
    return update;
  }
  Add(update, {.X0 = current.X0, .Y0 = current.Y0, .X1 = shared.X0, .Y1 = current.Y1});
  Add(update, {.X0 = shared.X1, .Y0 = current.Y0, .X1 = current.X1, .Y1 = current.Y1});
  Add(update, {.X0 = shared.X0, .Y0 = current.Y0, .X1 = shared.X1, .Y1 = shared.Y0});
  Add(update, {.X0 = shared.X0, .Y0 = shared.Y1, .X1 = shared.X1, .Y1 = current.Y1});
  return update;
}

Mat4f ShadowPatchProjection(const ShadowRaster &raster, const ShadowRasterPatch &patch) noexcept {
  Mat4f projection = raster.Packed;
  projection[12] += static_cast<float>(-2 * patch.Period[0]);
  projection[13] += static_cast<float>(2 * patch.Period[1]);
  return projection;
}

Mat4f ShadowPatchCullProjection(const Mat4f &projection,
                                const ShadowRasterPatch &patch,
                                uint32_t side) noexcept {
  const double x0 = std::max(0.0, static_cast<double>(patch.X) - 1);
  const double y0 = std::max(0.0, static_cast<double>(patch.Y) - 1);
  const double x1 =
      std::min(static_cast<double>(side), static_cast<double>(patch.X) + patch.Width + 1);
  const double y1 =
      std::min(static_cast<double>(side), static_cast<double>(patch.Y) + patch.Height + 1);
  const double sx = side / (x1 - x0);
  const double sy = side / (y1 - y0);
  const double tx = (side - x0 - x1) / (x1 - x0);
  const double ty = (y0 + y1 - side) / (y1 - y0);
  Mat4f result = projection;
  for (size_t column = 0; column < 4; ++column) {
    result[column * 4] =
        static_cast<float>(sx * projection[column * 4] + tx * projection[column * 4 + 3]);
    result[column * 4 + 1] =
        static_cast<float>(sy * projection[column * 4 + 1] + ty * projection[column * 4 + 3]);
  }
  return result;
}

}
