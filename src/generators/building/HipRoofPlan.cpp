#include "HipRoofPlan.h"
#include "PolygonTriangulation.h"

#include "roof/RoofContourInternal.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {

struct Edge {
  EastNorth From, To, Gradient;
  double InterceptM;
  size_t Ring, Number;
};

struct Input {
  std::vector<Edge> Edges;
  double ToleranceM = 0;
};

EastNorth Local(EastNorth point, EastNorth origin) {
  return {.EastM = point.EastM - origin.EastM, .NorthM = point.NorthM - origin.NorthM};
}

bool Same(EastNorth a, EastNorth b, double tolerance) {
  return std::hypot(a.EastM - b.EastM, a.NorthM - b.NorthM) <= tolerance;
}

std::expected<double, HipRoofError>
RingArea(std::span<const EastNorth> points, EastNorth origin, double &magnitude) {
  double area = 0;
  for (size_t at = 0; at < points.size(); ++at) {
    const auto a = Local(points[at], origin);
    const auto b = Local(points[(at + 1) % points.size()], origin);
    if (!std::isfinite(a.EastM) || !std::isfinite(a.NorthM) || Same(a, b, 0)) {
      return std::unexpected(HipRoofError::InvalidInput);
    }
    magnitude = std::max({magnitude, std::abs(a.EastM), std::abs(a.NorthM)});
    area += a.EastM * b.NorthM - a.NorthM * b.EastM;
  }
  if (area == 0 || !std::isfinite(area)) { return std::unexpected(HipRoofError::InvalidInput); }
  return area;
}

void AppendEdges(std::span<const EastNorth> points,
                 EastNorth origin,
                 size_t ring,
                 double area,
                 std::vector<Edge> &edges) {
  for (size_t at = 0; at < points.size(); ++at) {
    const auto a = Local(points[at], origin);
    const auto b = Local(points[(at + 1) % points.size()], origin);
    const double length = std::hypot(b.EastM - a.EastM, b.NorthM - a.NorthM);
    const double direction = (area > 0 ? 1.0 : -1.0) * (ring == 0 ? 1.0 : -1.0);
    const EastNorth gradient{.EastM = (a.NorthM - b.NorthM) * direction / length,
                             .NorthM = (b.EastM - a.EastM) * direction / length};
    edges.push_back({.From = a,
                     .To = b,
                     .Gradient = gradient,
                     .InterceptM = -a.EastM * gradient.EastM - a.NorthM * gradient.NorthM,
                     .Ring = ring,
                     .Number = at});
  }
}

std::expected<Input, HipRoofError> ReadInput(std::span<const EastNorth> exterior,
                                             std::span<const std::vector<EastNorth>> holes,
                                             HipRoofPlan::Limits limits,
                                             EastNorth origin) {
  Input input;
  size_t count = 0;
  double magnitude = 1;
  for (size_t ring = 0; ring <= holes.size(); ++ring) {
    const std::span<const EastNorth> points = ring == 0 ? exterior : holes[ring - 1];
    if (points.size() < 3) { return std::unexpected(HipRoofError::InvalidInput); }
    if (points.size() > limits.Vertices - count) {
      return std::unexpected(HipRoofError::CapacityExceeded);
    }
    count += points.size();
    const auto area = RingArea(points, origin, magnitude);
    if (!area) { return std::unexpected(area.error()); }
    AppendEdges(points, origin, ring, *area, input.Edges);
  }
  input.ToleranceM = magnitude * 64 * std::numeric_limits<double>::epsilon();
  return input;
}

std::optional<std::vector<HipRoofPlan::Facet>> RectangleFacets(const Input &input) {
  if (input.Edges.size() != 4 || input.Edges.back().Ring != 0) { return std::nullopt; }
  const auto u = input.Edges[0].To;
  const auto v = input.Edges[3].From;
  const double width = std::hypot(u.EastM, u.NorthM);
  const double depth = std::hypot(v.EastM, v.NorthM);
  const EastNorth opposite{.EastM = u.EastM + v.EastM, .NorthM = u.NorthM + v.NorthM};
  if (!Same(opposite, input.Edges[2].From, input.ToleranceM) ||
      std::abs(u.EastM * v.EastM + u.NorthM * v.NorthM) >
          input.ToleranceM * std::max(width, depth)) {
    return std::nullopt;
  }
  const double radius = std::min(width, depth) / 2;
  const EastNorth first{.EastM = radius * (u.EastM / width + v.EastM / depth),
                        .NorthM = radius * (u.NorthM / width + v.NorthM / depth)};
  const EastNorth last =
      width >= depth ? EastNorth{.EastM = first.EastM + u.EastM * (width - 2 * radius) / width,
                                 .NorthM = first.NorthM + u.NorthM * (width - 2 * radius) / width}
                     : EastNorth{.EastM = first.EastM + v.EastM * (depth - 2 * radius) / depth,
                                 .NorthM = first.NorthM + v.NorthM * (depth - 2 * radius) / depth};
  const std::array peaks{first, last};
  std::vector<HipRoofPlan::Facet> facets;
  facets.reserve(4);
  for (const auto &edge : input.Edges) {
    HipRoofPlan::Facet facet{.Ring = {edge.From, edge.To},
                             .Gradient = edge.Gradient,
                             .InterceptM = edge.InterceptM,
                             .SourceRing = edge.Ring,
                             .SourceEdge = edge.Number};
    for (size_t at = 0; at < peaks.size(); ++at) {
      if (at != 0 && Same(peaks[0], peaks[at], input.ToleranceM)) { continue; }
      if (std::abs(facet.DistanceAt(peaks[at]) - radius) <= input.ToleranceM) {
        facet.Ring.push_back(peaks[at]);
      }
    }
    if (facet.Ring.size() < 3) { return std::nullopt; }
    EastNorth centre{};
    for (const auto point : facet.Ring) {
      centre.EastM += point.EastM / static_cast<double>(facet.Ring.size());
      centre.NorthM += point.NorthM / static_cast<double>(facet.Ring.size());
    }
    std::ranges::sort(facet.Ring, [&](EastNorth a, EastNorth b) {
      return std::atan2(a.NorthM - centre.NorthM, a.EastM - centre.EastM) <
             std::atan2(b.NorthM - centre.NorthM, b.EastM - centre.EastM);
    });
    facets.push_back(std::move(facet));
  }
  return facets;
}

std::expected<std::vector<HipRoofPlan::Facet>, HipRoofError>
PrepareFacets(const Input &input, HipRoofPlan::Limits limits) {
  if (auto rectangle = RectangleFacets(input)) { return std::move(*rectangle); }
  std::vector<RoofContour::Ring> rings;
  std::vector<RoofContour::Point> normals;
  std::vector<double> intercepts;
  for (const auto &edge : input.Edges) {
    if (edge.Number == 0) { rings.emplace_back(); }
    rings.back().push_back({edge.From.EastM, edge.From.NorthM});
    normals.push_back({edge.Gradient.EastM, edge.Gradient.NorthM});
    intercepts.push_back(edge.InterceptM);
  }
  const RoofContour::Input request{.Rings = rings,
                                   .Normals = normals,
                                   .Intercepts = intercepts,
                                   .Limits = limits,
                                   .ToleranceM = std::max(1e-12, 32 * input.ToleranceM)};
  auto partition = RoofContour::Prepare(request);
  if (!partition) { return std::unexpected(partition.error().Kind); }
  std::vector<HipRoofPlan::Facet> facets;
  facets.reserve(partition->Faces.size());
  for (size_t at = 0; at < partition->Faces.size(); ++at) {
    const auto &edge = input.Edges[partition->Ownership[at]];
    HipRoofPlan::Facet facet{.Ring = {},
                             .Gradient = edge.Gradient,
                             .InterceptM = edge.InterceptM,
                             .SourceRing = edge.Ring,
                             .SourceEdge = edge.Number};
    for (const auto index : partition->Faces[at]) {
      const auto point = partition->Nodes[index];
      facet.Ring.push_back({.EastM = point[0], .NorthM = point[1]});
    }
    facets.push_back(std::move(facet));
  }
  return facets;
}

bool ContainsPoint(std::span<const EastNorth> ring, EastNorth point, double tolerance) {
  bool inside = false;
  for (size_t at = 0; at < ring.size(); ++at) {
    const auto a = ring[at];
    const auto b = ring[(at + 1) % ring.size()];
    const double cross = (b.EastM - a.EastM) * (point.NorthM - a.NorthM) -
                         (b.NorthM - a.NorthM) * (point.EastM - a.EastM);
    const double length = std::hypot(b.EastM - a.EastM, b.NorthM - a.NorthM);
    if (std::abs(cross) <= tolerance * length &&
        point.EastM >= std::min(a.EastM, b.EastM) - tolerance &&
        point.EastM <= std::max(a.EastM, b.EastM) + tolerance &&
        point.NorthM >= std::min(a.NorthM, b.NorthM) - tolerance &&
        point.NorthM <= std::max(a.NorthM, b.NorthM) + tolerance) {
      return true;
    }
    if ((a.NorthM > point.NorthM) != (b.NorthM > point.NorthM) &&
        point.EastM <
            (b.EastM - a.EastM) * (point.NorthM - a.NorthM) / (b.NorthM - a.NorthM) + a.EastM) {
      inside = !inside;
    }
  }
  return inside;
}

}

double HipRoofPlan::Facet::DistanceAt(EastNorth point) const noexcept {
  return point.EastM * Gradient.EastM + point.NorthM * Gradient.NorthM + InterceptM;
}

bool HipRoofPlan::Facet::Contains(EastNorth point, double tolerance) const noexcept {
  return ContainsPoint(Ring, point, tolerance);
}

std::expected<HipRoofPlan, HipRoofError>
HipRoofPlan::Prepare(std::span<const EastNorth> exterior,
                     std::span<const std::vector<EastNorth>> holes,
                     Limits limits) {
  if (exterior.empty() || limits.Vertices == 0 || limits.Nodes == 0 || limits.Facets == 0 ||
      limits.Corners == 0) {
    return std::unexpected(HipRoofError::InvalidInput);
  }
  auto input = ReadInput(exterior, holes, limits, exterior.front());
  if (!input) { return std::unexpected(input.error()); }
  auto facets = PrepareFacets(*input, limits);
  if (!facets) { return std::unexpected(facets.error()); }
  if (facets->size() > limits.Facets) { return std::unexpected(HipRoofError::CapacityExceeded); }
  HipRoofPlan plan;
  plan.Origin_ = exterior.front();
  plan.PrecisionM_ = input->ToleranceM;
  plan.Facets_.reserve(facets->size());
  std::map<std::pair<double, double>, double> nodes;
  size_t corners = 0;
  for (auto &facet : *facets) {
    if (facet.Ring.size() > limits.Corners - corners) {
      return std::unexpected(HipRoofError::CapacityExceeded);
    }
    for (const auto point : facet.Ring) {
      const double distance = facet.DistanceAt(point);
      if (distance < -input->ToleranceM) { return std::unexpected(HipRoofError::InvalidFacet); }
      const auto key = std::pair{point.EastM, point.NorthM};
      const auto found = nodes.find(key);
      if (found == nodes.end()) {
        if (nodes.size() == limits.Nodes) {
          return std::unexpected(HipRoofError::CapacityExceeded);
        }
        nodes.emplace(key, distance);
      } else if (std::abs(found->second - distance) > input->ToleranceM) {
        return std::unexpected(HipRoofError::InvalidFacet);
      }
      plan.MaximumDistanceM_ = std::max(plan.MaximumDistanceM_, distance);
    }
    corners += facet.Ring.size();
    plan.Facets_.push_back(std::move(facet));
  }
  if (!(plan.MaximumDistanceM_ > 0) || !plan.OwnsEveryEave(input->Edges.size())) {
    return std::unexpected(HipRoofError::InvalidFacet);
  }
  return plan;
}

bool HipRoofPlan::OwnsEveryEave(size_t count) const {
  if (Facets_.size() != count) { return false; }
  std::vector<std::pair<size_t, size_t>> owners;
  owners.reserve(count);
  for (const auto &facet : Facets_) { owners.emplace_back(facet.SourceRing, facet.SourceEdge); }
  std::ranges::sort(owners);
  return std::ranges::adjacent_find(owners) == owners.end();
}

std::optional<double> HipRoofPlan::DistanceAt(EastNorth point) const noexcept {
  const auto local = Local(point, Origin_);
  if (!std::isfinite(local.EastM) || !std::isfinite(local.NorthM)) { return std::nullopt; }
  for (const auto &facet : Facets_) {
    if (facet.Contains(local, PrecisionM_)) { return std::max(0.0, facet.DistanceAt(local)); }
  }
  return std::nullopt;
}

}
