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
  uint32_t X = 0, Y = 0;
  double Depth = 0.0;
};

template <class F>
void EachSample(DepthSurfaceSource source, const DepthSurfacePatch &patch, F &&take) {
  for (uint32_t y = patch.Top; y < patch.Bottom; ++y) {
    for (uint32_t x = patch.Left; x < patch.Right; ++x) {
      const size_t at = static_cast<size_t>(y) * source.Width + x;
      if (source.Surface[at] == 0) { continue; }
      take(Sample{.X = x, .Y = y, .Depth = source.Depth[at]});
    }
  }
}

struct PlaneMoments {
  double Count = 0.0;
  double X = 0.0, Y = 0.0, Z = 0.0;
  double XX = 0.0, XY = 0.0, YY = 0.0, XZ = 0.0, YZ = 0.0;

  void Add(double x, double y, double z) {
    ++Count;
    X += x;
    Y += y;
    Z += z;
    XX += x * x;
    XY += x * y;
    YY += y * y;
    XZ += x * z;
    YZ += y * z;
  }

  void Fit(DepthSurfacePatch &patch, uint32_t left, uint32_t top) const {
    const double meanX = X / Count;
    const double meanY = Y / Count;
    const double meanZ = Z / Count;
    const double xx = XX - meanX * X;
    const double xy = XY - meanX * Y;
    const double yy = YY - meanY * Y;
    const double xz = XZ - meanX * Z;
    const double yz = YZ - meanY * Z;
    const double determinant = xx * yy - xy * xy;
    const double trace = xx + yy;
    if (determinant > 32 * std::numeric_limits<double>::epsilon() * trace * trace) {
      patch.SlopeX = (yy * xz - xy * yz) / determinant;
      patch.SlopeY = (xx * yz - xy * xz) / determinant;
    } else if (trace > 0.0) {
      patch.SlopeX = xz / trace;
      patch.SlopeY = yz / trace;
    }
    patch.OriginDepth =
        meanZ - patch.SlopeX * (left + 0.5 + meanX) - patch.SlopeY * (top + 0.5 + meanY);
  }
};

std::optional<DepthSurfacePatch> PlaneOf(DepthSurfaceSource source, DepthSurfacePatch patch) {
  PlaneMoments moments;
  DepthSurfacePatch plane{.Left = patch.Right, .Top = patch.Bottom};
  EachSample(source, patch, [&](Sample at) {
    moments.Add(at.X - patch.Left, at.Y - patch.Top, at.Depth);
    plane.Left = std::min(plane.Left, at.X);
    plane.Top = std::min(plane.Top, at.Y);
    plane.Right = std::max(plane.Right, at.X + 1);
    plane.Bottom = std::max(plane.Bottom, at.Y + 1);
  });
  if (moments.Count == 0.0) { return std::nullopt; }
  moments.Fit(plane, patch.Left, patch.Top);
  return plane;
}

bool Fits(DepthSurfaceSource source, const DepthSurfacePatch &patch) {
  if (std::hypot(patch.SlopeX, patch.SlopeY) > source.MaximumSlope) { return false; }
  bool fits = true;
  EachSample(source, patch, [&](Sample at) {
    fits = fits && std::abs(patch.At(at.X + 0.5, at.Y + 0.5) - at.Depth) <= source.AllowedError;
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
  if (!std::isfinite(source.AllowedError) || source.AllowedError < 0 ||
      std::isnan(source.MaximumSlope) || source.MaximumSlope < 0) {
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
      Subdivide(pending, *plane);
    }
  }
  return result;
}

}
