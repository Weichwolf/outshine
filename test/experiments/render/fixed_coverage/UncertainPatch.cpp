#include "UncertainPatch.h"
#include "Exact.h"
#include "FixedPatch.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <utility>

namespace FixedCoverage {
namespace {
struct Rectangle {
  ExactPoint Min;
  ExactPoint Max;
};

struct Bounds {
  std::array<Rectangle, 4> Window;
  Exact FarDepth = Exact(1);
};

Exact Abs(const Exact &value) {
  return value.Sign() < 0 ? Exact(0) - value : value;
}

ExactPoint Corner(const Rectangle &rectangle, size_t i) {
  return {.X = (i & 1u) != 0 ? rectangle.Max.X : rectangle.Min.X,
          .Y = (i & 2u) != 0 ? rectangle.Max.Y : rectangle.Min.Y};
}

std::expected<Bounds, Unknown>
Window(const UncertainQuad &source, uint32_t width, uint32_t height) {
  if (!source.Opaque) { return std::unexpected(Unknown::NonOpaque); }
  if (!source.Complete) { return std::unexpected(Unknown::Incomplete); }
  if (width == 0 || height == 0 || width > MaximumCoordinate || height > MaximumCoordinate ||
      (source.FrontWindowOrientation != -1 && source.FrontWindowOrientation != 1)) {
    return std::unexpected(Unknown::InvalidProfile);
  }
  Bounds result;
  for (size_t i = 0; i < source.Clip.size(); ++i) {
    const auto &clip = source.Clip[i];
    for (const auto &range : clip) {
      if (!std::isfinite(range.Lower) || !std::isfinite(range.Upper) || range.Lower > range.Upper) {
        return std::unexpected(Unknown::Nonfinite);
      }
    }
    if (clip[3].Lower <= 0 || clip[2].Lower < 0 || clip[2].Upper > clip[3].Lower) {
      return std::unexpected(Unknown::ClipCrossing);
    }
    std::array<Exact, 2> lower;
    std::array<Exact, 2> upper;
    for (size_t axis = 0; axis < 2; ++axis) {
      const std::array values{Exact(clip[axis].Lower) / Exact(clip[3].Lower),
                              Exact(clip[axis].Lower) / Exact(clip[3].Upper),
                              Exact(clip[axis].Upper) / Exact(clip[3].Lower),
                              Exact(clip[axis].Upper) / Exact(clip[3].Upper)};
      const auto less = [](const Exact &a, const Exact &b) { return a < b; };
      lower[axis] = *std::ranges::min_element(values, less);
      upper[axis] = *std::ranges::max_element(values, less);
    }
    result.Window[i] = {.Min = {.X = (lower[0] + Exact(1)) * Exact(width) / Exact(2),
                                .Y = (Exact(1) - upper[1]) * Exact(height) / Exact(2)},
                        .Max = {.X = (upper[0] + Exact(1)) * Exact(width) / Exact(2),
                                .Y = (Exact(1) - lower[1]) * Exact(height) / Exact(2)}};
    result.FarDepth = std::min(result.FarDepth, Exact(clip[2].Lower) / Exact(clip[3].Upper));
  }
  return result;
}

bool Facing(const Bounds &bounds, int sign) {
  for (size_t i = 0; i < 4; ++i) {
    for (size_t a = 0; a < 4; ++a) {
      for (size_t b = 0; b < 4; ++b) {
        for (size_t c = 0; c < 4; ++c) {
          if (Edge(Corner(bounds.Window[i], a),
                   Corner(bounds.Window[(i + 1) % 4], b),
                   Corner(bounds.Window[(i + 2) % 4], c))
                  .Sign() != sign) {
            return false;
          }
        }
      }
    }
  }
  return true;
}

HalfPlane Line(const ExactPoint &a, const ExactPoint &b) {
  return {.A = a.Y - b.Y, .B = b.X - a.X, .C = (b.Y - a.Y) * a.X - (b.X - a.X) * a.Y};
}

Exact Distance(const HalfPlane &plane, const ExactPoint &point) {
  return plane.A * point.X + plane.B * point.Y + plane.C;
}

std::expected<std::array<ExactPoint, 4>, Unknown> Erode(const Bounds &bounds) {
  std::array<ExactPoint, 4> centre;
  Exact rx;
  Exact ry;
  for (size_t i = 0; i < 4; ++i) {
    const auto &box = bounds.Window[i];
    centre[i] = {.X = (box.Min.X + box.Max.X) / Exact(2), .Y = (box.Min.Y + box.Max.Y) / Exact(2)};
    rx = std::max(rx, (box.Max.X - box.Min.X) / Exact(2));
    ry = std::max(ry, (box.Max.Y - box.Min.Y) / Exact(2));
  }
  std::array<HalfPlane, 4> lines;
  for (size_t i = 0; i < 4; ++i) {
    lines[i] = Line(centre[i], centre[(i + 1) % 4]);
    lines[i].C = lines[i].C - Abs(lines[i].A) * rx - Abs(lines[i].B) * ry;
  }
  std::array<ExactPoint, 4> result;
  for (size_t i = 0; i < 4; ++i) {
    const auto &a = lines[(i + 3) % 4];
    const auto &b = lines[i];
    const auto determinant = a.A * b.B - b.A * a.B;
    if (determinant.Sign() == 0) { return std::unexpected(Unknown::EmptyInterior); }
    result[i] = {.X = (a.B * b.C - b.B * a.C) / determinant,
                 .Y = (a.C * b.A - b.C * a.A) / determinant};
    for (const auto &line : lines) {
      if (Distance(line, result[i]).Sign() < 0) { return std::unexpected(Unknown::EmptyInterior); }
    }
  }
  return result;
}
}

std::expected<Projected, Unknown>
Project(const UncertainQuad &source, uint32_t width, uint32_t height) {
  auto bounds = Window(source, width, height);
  if (!bounds) { return std::unexpected(bounds.error()); }
  if (!Facing(*bounds, source.FrontWindowOrientation)) {
    return std::unexpected(Unknown::UncertainFacing);
  }
  if (source.FrontWindowOrientation < 0) { std::ranges::reverse(bounds->Window); }
  auto corners = Erode(*bounds);
  if (!corners) { return std::unexpected(corners.error()); }
  return Projected{.Corners = std::move(*corners),
                   .FarDepth = std::move(bounds->FarDepth),
                   .SourceWindowOrientation = source.FrontWindowOrientation};
}

std::expected<FixedPatch, Unknown>
Prepare(const UncertainQuad &source, uint32_t width, uint32_t height) {
  auto projected = Project(source, width, height);
  if (!projected) { return std::unexpected(projected.error()); }
  return Prepare(*projected, width, height);
}

std::expected<UncertainOracle, Unknown>
MakeOracle(const UncertainQuad &source, uint32_t width, uint32_t height) {
  auto bounds = Window(source, width, height);
  if (!bounds) { return std::unexpected(bounds.error()); }
  if (!Facing(*bounds, source.FrontWindowOrientation)) {
    return std::unexpected(Unknown::UncertainFacing);
  }
  if (source.FrontWindowOrientation < 0) { std::ranges::reverse(bounds->Window); }
  UncertainOracle result;
  result.FarDepth = bounds->FarDepth;
  result.Min = bounds->Window[0].Min;
  result.Max = bounds->Window[0].Max;
  size_t n = 0;
  for (size_t i = 0; i < 4; ++i) {
    result.Min.X = std::min(result.Min.X, bounds->Window[i].Min.X);
    result.Min.Y = std::min(result.Min.Y, bounds->Window[i].Min.Y);
    result.Max.X = std::max(result.Max.X, bounds->Window[i].Max.X);
    result.Max.Y = std::max(result.Max.Y, bounds->Window[i].Max.Y);
    for (size_t a = 0; a < 4; ++a) {
      for (size_t b = 0; b < 4; ++b) {
        result.Edges[n++] =
            Line(Corner(bounds->Window[i], a), Corner(bounds->Window[(i + 1) % 4], b));
      }
    }
  }
  return result;
}

bool Covers(const UncertainOracle &source, const std::array<uint32_t, 2> &pixel, float depth) {
  if (!std::isfinite(depth) || source.FarDepth < Exact::Float32(depth)) { return false; }
  const ExactPoint p{.X = Exact(pixel[0]), .Y = Exact(pixel[1])};
  if (p.X < source.Min.X || p.Y < source.Min.Y || source.Max.X < p.X + Exact(1) ||
      source.Max.Y < p.Y + Exact(1)) {
    return false;
  }
  return std::ranges::all_of(source.Edges, [&](const auto &edge) {
    return (Distance(edge, p) + std::min(Exact(0), edge.A) + std::min(Exact(0), edge.B)).Sign() >=
           0;
  });
}
}
