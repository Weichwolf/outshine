#include "RoadGroundStripInternal.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <numeric>
#include <span>
#include <vector>

namespace outshine::Generators::RoadGround {
namespace {
constexpr double kCutSignLeast = 1e-12;
constexpr double kDenominatorLeast = 1e-4;
constexpr double kDenominatorRatioMost = 32;
constexpr double kBlendExcursionMost = 4;

EastNorth Point(const RoadGroundField &field, size_t at) {
  const auto p = field.Vertices[at];
  return {.EastM = p.EastM, .NorthM = p.NorthM};
}

std::expected<void, RoadGroundError> Cut(const RoadGroundStrip &strip,
                                         const RoadGroundField &field,
                                         size_t patch,
                                         RoadPortField::Patch &model) {
  const size_t intervals = strip.TerrainQueries.size() / 2 - 1;
  for (size_t end = 0; end < 2; ++end) {
    const size_t station = strip.Boundaries[patch + end];
    const auto a = Point(field, 2 * station);
    const auto b = Point(field, 2 * station + 1);
    const double width = std::hypot(b.EastM - a.EastM, b.NorthM - a.NorthM);
    if (!(width > 0)) { return std::unexpected(RoadGroundError::InvalidGeometry); }
    const EastNorth centre{.EastM = std::midpoint(a.EastM, b.EastM),
                           .NorthM = std::midpoint(a.NorthM, b.NorthM)};
    EastNorth normal{.EastM = -(b.NorthM - a.NorthM) / width,
                     .NorthM = (b.EastM - a.EastM) / width};
    const size_t triangle = end == 0 ? station : intervals + station - 1;
    EastNorth inside;
    for (const auto corner : strip.Triangles[triangle]) {
      inside.EastM += field.Vertices[corner].EastM / 3;
      inside.NorthM += field.Vertices[corner].NorthM / 3;
    }
    const double sign = (inside.EastM - centre.EastM) * normal.EastM +
                        (inside.NorthM - centre.NorthM) * normal.NorthM;
    if (std::abs(sign) <= kCutSignLeast) {
      return std::unexpected(RoadGroundError::InvalidGeometry);
    }
    if (sign < 0) {
      normal.EastM = -normal.EastM;
      normal.NorthM = -normal.NorthM;
    }
    model.CutCentres[end] = centre;
    model.InwardNormals[end] = normal;
  }
  return {};
}

bool Blend(const RoadGroundField &field, size_t first, size_t last, RoadPortField::Patch &patch) {
  EastNorth low{.EastM = std::numeric_limits<double>::infinity(),
                .NorthM = std::numeric_limits<double>::infinity()};
  EastNorth high{.EastM = -low.EastM, .NorthM = -low.NorthM};
  for (size_t at = first; at < last; ++at) {
    const auto p = Point(field, at);
    low.EastM = std::min(low.EastM, p.EastM);
    low.NorthM = std::min(low.NorthM, p.NorthM);
    high.EastM = std::max(high.EastM, p.EastM);
    high.NorthM = std::max(high.NorthM, p.NorthM);
  }
  patch.ScaleM = std::max(1., std::hypot(high.EastM - low.EastM, high.NorthM - low.NorthM));
  double least = std::numeric_limits<double>::infinity();
  double most = 0;
  double excursion = 0;
  for (size_t at = first; at < last; ++at) {
    const auto p = Point(field, at);
    std::array<double, 2> signedDistance{};
    for (size_t end = 0; end < 2; ++end) {
      signedDistance[end] =
          ((p.EastM - patch.CutCentres[end].EastM) * patch.InwardNormals[end].EastM +
           (p.NorthM - patch.CutCentres[end].NorthM) * patch.InwardNormals[end].NorthM) /
          patch.ScaleM;
    }
    const double d = signedDistance[0] + signedDistance[1];
    least = std::min(least, d);
    most = std::max(most, d);
    excursion = std::max(excursion, std::abs(signedDistance[0] / d));
  }
  return least > kDenominatorLeast && most / least <= kDenominatorRatioMost &&
         excursion <= kBlendExcursionMost;
}

uint32_t Root(std::span<uint32_t> parents, uint32_t at) {
  while (parents[at] != at) {
    parents[at] = parents[parents[at]];
    at = parents[at];
  }
  return at;
}
}

std::expected<void, RoadGroundError> Layout(const RoadGroundStrip &strip, RoadGroundField &field) {
  const size_t count = strip.Boundaries.size() / 2;
  field.Planes.reserve(count);
  std::vector<uint32_t> parents(count);
  std::ranges::iota(parents, uint32_t{0});
  field.Patches.resize(2 * count - 1);
  for (size_t patch = 1; patch < field.Patches.size(); patch += 2) {
    auto &model = field.Patches[patch];
    const auto cut = Cut(strip, field, patch, model);
    if (!cut) { return std::unexpected(cut.error()); }
    const size_t first = size_t{2} * strip.Boundaries[patch];
    const size_t last = 2 * (static_cast<size_t>(strip.Boundaries[patch + 1]) + 1);
    if (!Blend(field, first, last, model)) {
      const auto a = Root(parents, static_cast<uint32_t>(patch / 2));
      const auto b = Root(parents, static_cast<uint32_t>(patch / 2 + 1));
      parents[std::max(a, b)] = std::min(a, b);
      ++field.Work.MergedPlanes;
    }
  }
  std::vector<uint32_t> ids(count);
  for (size_t at = 0; at < count; ++at) {
    const auto root = Root(parents, static_cast<uint32_t>(at));
    if (root == at) {
      ids[at] = static_cast<uint32_t>(field.Planes.size());
      field.Planes.push_back({.Origin = {}, .Gradient = {}});
    } else {
      ids[at] = ids[root];
    }
  }
  std::vector<size_t> vertices(field.Planes.size(), 0);
  for (size_t at = 0; at < count; ++at) {
    auto &origin = field.Planes[ids[at]].Origin;
    for (size_t vertex = size_t{2} * strip.Boundaries[2 * at];
         vertex < 2 * (static_cast<size_t>(strip.Boundaries[2 * at + 1]) + 1);
         ++vertex) {
      origin.EastM += field.Vertices[vertex].EastM;
      origin.NorthM += field.Vertices[vertex].NorthM;
      ++vertices[ids[at]];
    }
  }
  for (size_t at = 0; at < field.Planes.size(); ++at) {
    field.Planes[at].Origin.EastM /= static_cast<double>(vertices[at]);
    field.Planes[at].Origin.NorthM /= static_cast<double>(vertices[at]);
  }
  for (size_t at = 0; at < field.Patches.size(); ++at) {
    field.Patches[at].Parents = {ids[at / 2], ids[(at + 1) / 2]};
  }
  return {};
}
}
