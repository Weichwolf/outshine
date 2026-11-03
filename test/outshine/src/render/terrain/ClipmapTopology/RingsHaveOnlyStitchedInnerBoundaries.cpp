#include "Check.h"
#include "ClipmapTopology.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <utility>

namespace {
using namespace outshine::Render;
using namespace outshine::Test;
using Point = std::array<float, 2>;

Point Position(const ClipmapVertex &v) {
  return {v.East, v.North};
}

void CheckBoundary(const ClipmapTopology &mesh,
                   ClipmapShape shape,
                   const std::map<std::array<uint32_t, 2>, uint32_t> &edges) {
  const size_t half = shape.HalfCells;
  const auto h = static_cast<float>(half);
  size_t outer = 0;
  size_t inner = 0;
  for (const auto &[edge, count] : edges) {
    CHECK(count == 1 || count == 2, "no edge has more than two neighbours");
    if (count != 1) { continue; }
    const auto a = Position(mesh.Vertices[edge[0]]);
    const auto b = Position(mesh.Vertices[edge[1]]);
    const bool rim = (a[0] == b[0] && std::abs(a[0]) == h) || (a[1] == b[1] && std::abs(a[1]) == h);
    if (rim) {
      ++outer;
      CHECK(std::hypot(a[0] - b[0], a[1] - b[1]) == 1, "outer rim has one edge per coarse cell");
    } else {
      ++inner;
      CHECK(shape.HolePhase.has_value(), "only a ring can have an inner boundary");
      if (!shape.HolePhase) { continue; }
      const Point low{-h / 2 + static_cast<float>((*shape.HolePhase)[0]),
                      -h / 2 + static_cast<float>((*shape.HolePhase)[1])};
      const bool along = (a[0] == b[0] && (a[0] == low[0] || a[0] == low[0] + h)) ||
                         (a[1] == b[1] && (a[1] == low[1] || a[1] == low[1] + h));
      CHECK(along && std::hypot(a[0] - b[0], a[1] - b[1]) == 0.5,
            "inner edges match the finer grid without cracks or skirts");
    }
  }
  CHECK(outer == 8 * half && inner == (shape.HolePhase ? 8 * half : 0),
        "only the intended two perimeters remain");
}

void CheckMesh(ClipmapShape shape) {
  const auto built = ClipmapTopology::Make(shape, std::numeric_limits<size_t>::max());
  CHECK(built.has_value(), "admitted topology builds");
  if (!built) { return; }
  const auto &mesh = *built;
  const size_t half = shape.HalfCells;
  const size_t faces = shape.HolePhase ? 6 * half * half + 4 * half : 8 * half * half;
  const size_t vertices =
      shape.HolePhase ? 3 * half * half + 10 * half : (2 * half + 1) * (2 * half + 1);
  CHECK(mesh.Vertices.size() == vertices && mesh.Indices.size() == faces * 3,
        "counts follow the ring area and its four stitched inner edges");
  std::set<Point> points;
  for (const auto &vertex : mesh.Vertices) { points.insert(Position(vertex)); }
  CHECK(points.size() == mesh.Vertices.size(), "one stored vertex per used position");
  std::map<std::array<uint32_t, 2>, uint32_t> edges;
  std::set<uint32_t> used;
  double area = 0;
  for (size_t i = 0; i < mesh.Indices.size(); i += 3) {
    const std::array ids{mesh.Indices[i], mesh.Indices[i + 1], mesh.Indices[i + 2]};
    CHECK(std::ranges::all_of(ids, [&](auto id) { return id < mesh.Vertices.size(); }),
          "all indices name stored vertices");
    const auto a = Position(mesh.Vertices[ids[0]]);
    const auto b = Position(mesh.Vertices[ids[1]]);
    const auto c = Position(mesh.Vertices[ids[2]]);
    const double twice = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
    CHECK(twice > 0, "every triangle faces up and has positive area");
    area += twice / 2;
    for (size_t k = 0; k < 3; ++k) {
      auto edge = std::array{ids[k], ids[(k + 1) % 3]};
      std::ranges::sort(edge);
      ++edges[edge];
      used.insert(ids[k]);
    }
  }
  CHECK(area == (shape.HolePhase ? 3 : 4) * static_cast<double>(half * half),
        "mesh area excludes the hole before geometry emission");
  CHECK(used.size() == mesh.Vertices.size(), "the hole leaves no unused vertices");
  CheckBoundary(mesh, shape, edges);
}
}

int main() {
  for (const uint32_t half : {8u, 64u, 128u}) {
    CheckMesh({.HalfCells = half});
    for (uint32_t y = 0; y < 2; ++y) {
      for (uint32_t x = 0; x < 2; ++x) { CheckMesh({.HalfCells = half, .HolePhase = {{x, y}}}); }
    }
  }
  for (const auto shape : {ClipmapShape{.HalfCells = 4},
                           ClipmapShape{.HalfCells = 9},
                           ClipmapShape{.HalfCells = std::numeric_limits<uint32_t>::max()},
                           ClipmapShape{.HalfCells = 8, .HolePhase = {{2, 0}}}}) {
    const auto refused = ClipmapTopology::Make(shape, std::numeric_limits<size_t>::max());
    CHECK(!refused && refused.error() == ClipmapTopologyError::InvalidShape,
          "unsupported shape is refused before allocation");
  }
  const auto budget = ClipmapTopology::Make({.HalfCells = 128}, 1024);
  CHECK(!budget && budget.error() == ClipmapTopologyError::OverBudget,
        "caller admission covers output and construction scratch");
  return Report();
}
