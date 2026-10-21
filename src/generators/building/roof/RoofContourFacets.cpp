#include "RoofContourInternal.h"
#include <algorithm>
#include <expected>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <numbers>
#include <numeric>
#include <optional>
#include <set>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Generators::RoofContour {
namespace {
Checked<std::vector<Ring>>
SourceFacetPolygons(const Geos &g,
                    const Input &input,
                    std::span<const std::vector<std::array<Point, 4>>> patches,
                    std::span<const Point> original) {
  const auto normals = input.Normals;
  const auto intercepts = input.Intercepts;
  const double tolerance = input.ToleranceM;
  std::vector<std::vector<Point>> polygons;
  size_t owner = 0;
  for (const auto &pieces : patches) {
    std::vector<Geometry> raw;
    for (const auto &piece : pieces) {
      std::vector<Point> points(piece.begin(), piece.end());
      if (points.size() > 1 && points.front() == points.back()) { points.pop_back(); }
      raw.push_back(Polygon(g, points));
      if (!raw.back()) {
        return std::unexpected(Failure{HipRoofError::InvalidInput, "swept polygon input"});
      }
    }
    const auto collection = Collection(g, std::move(raw));
    if (!collection) { return std::unexpected(Failure{"swept collection"}); }
    const auto merged = g.Own(GEOSUnaryUnion_r(g.Handle, collection.get()));
    if (!merged || GEOSGeomTypeId_r(g.Handle, merged.get()) != GEOS_POLYGON ||
        GEOSGetNumInteriorRings_r(g.Handle, merged.get()) != 0) {
      return std::unexpected(Failure{"disconnected or holed source facet"});
    }
    const auto simplified =
        g.Own(GEOSTopologyPreserveSimplify_r(g.Handle, merged.get(), tolerance));
    if (!simplified) { return std::unexpected(Failure{"facet simplify"}); }
    auto stationTime = Coordinates(g, GEOSGetExteriorRing_r(g.Handle, simplified.get()), true);
    if (!stationTime) { return std::unexpected(stationTime.error()); }
    const auto normal = normals[owner];
    const Point tangent{normal[1], -normal[0]};
    std::vector<Point> points;
    for (const auto p : *stationTime) {
      points.push_back(Add(Scale(tangent, p[0]), Scale(normal, p[1] - intercepts[owner])));
    }
    auto divided = Subdivide(points, original, tolerance);
    if (!divided) { return std::unexpected(divided.error()); }
    polygons.push_back(std::move(*divided));
    ++owner;
  }
  return polygons;
}

using EdgeKey = std::array<Point, 2>;
using EaveOwners = std::map<EdgeKey, size_t>;

Checked<EaveOwners> SourceEaves(std::span<const Ring> rings, size_t planeCount) {
  EaveOwners boundaries;
  size_t owner = 0;
  for (const auto &ring : rings) {
    for (size_t at = 0; at < ring.size(); ++at) {
      EdgeKey key{ring[at], ring[(at + 1) % ring.size()]};
      std::ranges::sort(key);
      if (!boundaries.emplace(key, owner++).second) {
        return std::unexpected(Failure{"duplicate source eave"});
      }
    }
  }
  if (owner != planeCount) { return std::unexpected(Failure{"source-plane count"}); }
  return boundaries;
}

struct FacetLayout {
  std::span<const Point> Nodes, Shared, Original;
  std::span<const size_t> Lengths;
};

Checked<Partition> BindFaces(const Input &input, FacetLayout data) {
  const auto coordinates = data.Nodes;
  const auto normals = input.Normals;
  const auto intercepts = input.Intercepts;
  const double tolerance = input.ToleranceM;
  const auto lengths = data.Lengths;
  std::map<Point, size_t> lookup;
  for (size_t at = 0; at < coordinates.size(); ++at) { lookup.emplace(coordinates[at], at); }
  const auto builtBoundaries = SourceEaves(input.Rings, input.Normals.size());
  if (!builtBoundaries) { return std::unexpected(builtBoundaries.error()); }
  const auto &boundaries = *builtBoundaries;
  std::vector<std::vector<size_t>> faces;
  std::vector<size_t> ownership;
  std::vector<double> distances(coordinates.size());
  size_t offset = data.Original.size();
  std::set<size_t> usedOwners;
  for (const auto length : lengths) {
    const std::span<const Point> points = data.Shared.subspan(offset, length);
    auto split = Subdivide(points, coordinates, tolerance);
    if (!split) { return std::unexpected(split.error()); }
    if (split->size() < 3) { return std::unexpected(Failure{"degenerate facet"}); }
    std::vector<size_t> face;
    std::vector<size_t> matched;
    for (size_t at = 0; at < split->size(); ++at) {
      const auto point = (*split)[at];
      face.push_back(lookup.at(point));
      EdgeKey key{point, (*split)[(at + 1) % split->size()]};
      std::ranges::sort(key);
      const auto found = boundaries.find(key);
      if (found != boundaries.end()) { matched.push_back(found->second); }
    }
    if (matched.size() != 1 || !usedOwners.insert(matched.front()).second) {
      return std::unexpected(Failure{"facet source-edge ownership"});
    }
    const auto id = matched.front();
    ownership.push_back(id);
    for (const auto node : face) {
      distances[node] =
          std::max(distances[node], Dot(coordinates[node], normals[id]) + intercepts[id]);
    }
    faces.push_back(std::move(face));
    offset += length;
  }
  if (usedOwners.size() != normals.size()) {
    return std::unexpected(Failure{"missing source facet"});
  }
  for (const auto point : data.Original) { distances[lookup.at(point)] = 0; }
  return Partition{.Nodes = Ring(coordinates.begin(), coordinates.end()),
                   .Distances = std::move(distances),
                   .Faces = std::move(faces),
                   .Ownership = std::move(ownership)};
}

}

Checked<Partition> Assemble(const Input &input,
                            std::span<const std::vector<std::array<Point, 4>>> patches) {
  const Geos g;
  if (g.Handle == nullptr) { return std::unexpected(Failure{"GEOS context"}); }
  const auto normals = input.Normals;
  const auto intercepts = input.Intercepts;
  const double tolerance = input.ToleranceM;
  const size_t nodeLimit = input.Limits.Nodes;
  const auto rings = input.Rings;
  std::vector<Point> original;
  for (const auto &ring : rings) { original.insert(original.end(), ring.begin(), ring.end()); }
  if (original.empty() || normals.size() != intercepts.size() || normals.size() != patches.size()) {
    return std::unexpected(Failure{HipRoofError::InvalidInput, "facet input shape"});
  }
  auto builtPolygons = SourceFacetPolygons(g, input, patches, original);
  if (!builtPolygons) { return std::unexpected(builtPolygons.error()); }
  auto polygons = std::move(*builtPolygons);
  std::vector<Point> combined = original;
  std::vector<int> owners(original.size(), -1);
  std::vector<size_t> lengths;
  for (size_t at = 0; at < polygons.size(); ++at) {
    lengths.push_back(polygons[at].size());
    combined.insert(combined.end(), polygons[at].begin(), polygons[at].end());
    owners.insert(owners.end(), polygons[at].size(), static_cast<int>(at));
  }
  auto refined = Refine(std::move(combined),
                        owners,
                        normals,
                        intercepts,
                        {.FixedVertices = original.size(), .ToleranceM = tolerance});
  if (!refined) { return std::unexpected(refined.error()); }
  auto shared = WeldVertices(refined->Points, tolerance, original.size());
  if (!shared) { return std::unexpected(shared.error()); }
  const auto coordinates = Unique(*shared);
  if (coordinates.size() > nodeLimit) {
    return std::unexpected(Failure{HipRoofError::CapacityExceeded, "facet node budget"});
  }
  auto result = BindFaces(
      input, {.Nodes = coordinates, .Shared = *shared, .Original = original, .Lengths = lengths});
  if (!result) { return std::unexpected(result.error()); }
  result->MaximumInterfaceRefinementM = refined->MaximumM;
  return result;
}
}
