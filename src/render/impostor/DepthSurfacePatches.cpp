#include "DepthSurfacePatches.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace outshine::Render {
namespace {

struct Sample {
  double X = 0.0, Y = 0.0, Depth = 0.0;
};

template <class F>
void EachSample(DepthSurfaceSource source, const DepthSurfacePatch &patch, F &&take) {
  for (uint32_t y = patch.Top; y < patch.Bottom; ++y) {
    for (uint32_t x = patch.Left; x < patch.Right; ++x) {
      const size_t at = static_cast<size_t>(y) * source.Width + x;
      if (source.Surface[at] == 0) { continue; }
      take(Sample{.X = static_cast<double>(x) + 0.5,
                  .Y = static_cast<double>(y) + 0.5,
                  .Depth = source.Depth[at]});
    }
  }
}

std::optional<DepthSurfacePatch> PlaneOf(DepthSurfaceSource source, DepthSurfacePatch patch) {
  std::optional<Sample> first;
  Sample second;
  double extent = 0.0;
  EachSample(source, patch, [&](Sample at) {
    if (!first) { first = at; }
    const double dx = at.X - first->X;
    const double dy = at.Y - first->Y;
    const double distance = dx * dx + dy * dy;
    if (distance > extent) {
      extent = distance;
      second = at;
    }
  });
  if (!first) { return std::nullopt; }
  double sx = 0.0;
  double sy = 0.0;
  if (extent > 0.0) {
    const double dx = second.X - first->X;
    const double dy = second.Y - first->Y;
    const double dz = second.Depth - first->Depth;
    sx = dz * dx / extent;
    sy = dz * dy / extent;
    Sample third;
    double determinant = 0.0;
    EachSample(source, patch, [&](Sample at) {
      const double cross = dx * (at.Y - first->Y) - dy * (at.X - first->X);
      if (std::abs(cross) > std::abs(determinant)) {
        determinant = cross;
        third = at;
      }
    });
    if (determinant != 0.0) {
      const double tx = third.X - first->X;
      const double ty = third.Y - first->Y;
      const double tz = third.Depth - first->Depth;
      sx = (dz * ty - dy * tz) / determinant;
      sy = (dx * tz - dz * tx) / determinant;
    }
  }
  patch.SlopeX = sx;
  patch.SlopeY = sy;
  patch.OriginDepth = first->Depth - sx * first->X - sy * first->Y;
  return patch;
}

bool Fits(DepthSurfaceSource source, const DepthSurfacePatch &patch) {
  bool fits = true;
  EachSample(source, patch, [&](Sample at) {
    fits = fits && std::abs(patch.At(at.X, at.Y) - at.Depth) <= source.AllowedError;
  });
  for (const auto x : {patch.Left, patch.Right}) {
    for (const auto y : {patch.Top, patch.Bottom}) {
      const double depth = patch.At(x, y);
      fits = fits && depth >= 0.0 && depth <= 1.0;
    }
  }
  return fits;
}

void Subdivide(std::vector<DepthSurfacePatch> &pending, const DepthSurfacePatch &patch) {
  const uint32_t mx = patch.Left + (patch.Right - patch.Left) / 2;
  const uint32_t my = patch.Top + (patch.Bottom - patch.Top) / 2;
  for (const auto [top, bottom] : {std::pair{patch.Top, my}, std::pair{my, patch.Bottom}}) {
    for (const auto [left, right] : {std::pair{patch.Left, mx}, std::pair{mx, patch.Right}}) {
      if (left < right && top < bottom) {
        pending.push_back({.Left = left, .Top = top, .Right = right, .Bottom = bottom});
      }
    }
  }
}

}

std::expected<std::vector<DepthSurfacePatch>, DepthSurfaceError>
BuildDepthSurfacePatches(DepthSurfaceSource source) {
  const uint64_t count = static_cast<uint64_t>(source.Width) * source.Height;
  if (source.Width == 0 || source.Height == 0 || count > std::numeric_limits<uint32_t>::max()) {
    return std::unexpected(DepthSurfaceError::InvalidDimensions);
  }
  if (source.Depth.size() != count || source.Surface.size() != count) {
    return std::unexpected(DepthSurfaceError::InvalidSamples);
  }
  if (!std::isfinite(source.AllowedError) || source.AllowedError < 0) {
    return std::unexpected(DepthSurfaceError::InvalidAllowance);
  }
  for (size_t at = 0; at < source.Depth.size(); ++at) {
    if (source.Surface[at] == 0) { continue; }
    if (!std::isfinite(source.Depth[at]) || source.Depth[at] <= 0 || source.Depth[at] > 1) {
      return std::unexpected(DepthSurfaceError::InvalidSamples);
    }
  }
  std::vector<DepthSurfacePatch> pending{{.Right = source.Width, .Bottom = source.Height}};
  std::vector<DepthSurfacePatch> result;
  while (!pending.empty()) {
    const auto region = pending.back();
    pending.pop_back();
    const auto plane = PlaneOf(source, region);
    if (!plane) { continue; }
    if (Fits(source, *plane)) {
      result.push_back(*plane);
    } else {
      Subdivide(pending, region);
    }
  }
  return result;
}

}
