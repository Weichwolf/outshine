#include "TreePrototype.h"
#include "TreeSpecies.h"
#include "TreeFoliage.h"
#include "math/Mat4.h"
#include "scene/Geometry.h"
#include <chrono>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <print>
#include <ratio>
#include <string>
#include <vector>

namespace {
struct Cost {
  size_t Vertices = 0;
  size_t Triangles = 0;
  size_t Arrays = 0;
};

Cost Count(const outshine::Geometry &geometry) {
  Cost result;
  for (int part = 0; part < geometry.parts(); ++part) {
    result.Vertices += geometry.positionsOf(part).size() / 3;
    result.Triangles += geometry.trianglesOf(part).size() / 3;
    result.Arrays += geometry.positionsOf(part).size_bytes() +
                     geometry.normalsOf(part).size_bytes() + geometry.textureOf(part).size_bytes() +
                     geometry.trianglesOf(part).size_bytes();
  }
  return result;
}

double Elapsed(std::chrono::steady_clock::time_point start) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
      .count();
}

size_t UniqueOrigins(const outshine::Generators::TreePrototype::Rank &rank) {
  std::vector<std::array<float, 3>> origins;
  origins.reserve(rank.CardCount);
  for (size_t at = 0; at < rank.Cards.size(); at += outshine::Generators::TreeFoliage::kFloats) {
    origins.push_back({rank.Cards[at], rank.Cards[at + 1], rank.Cards[at + 2]});
  }
  std::ranges::sort(origins);
  return static_cast<size_t>(std::ranges::unique(origins).begin() - origins.begin());
}
}

int main(int argc, const char **argv) {
  using namespace outshine::Generators;
  if (argc < 2) { return 2; }
  std::puts("source,rank,grow_ms,expanded_ms,expanded_vertices,expanded_triangles,expanded_arrays_"
            "bytes,instanced_ms,shared_vertices,shared_triangles,shared_arrays_bytes,leaf_"
            "placements,placement_bytes,unique_leaf_origins");
  for (int arg = 1; arg < argc; ++arg) {
    std::ifstream input(argv[arg]);
    if (!input) { return 2; }
    const std::string definition{std::istreambuf_iterator<char>(input), {}};
    TreeSpecies species;
    if (!species.Parse(definition.data(), definition.size())) { return 2; }
    const auto growAt = std::chrono::steady_clock::now();
    const auto tree = TreePrototype::Grow(species);
    const double growMs = Elapsed(growAt);
    if (!tree) { return 2; }
    for (size_t rank = 0; rank < tree->Ranks().size(); ++rank) {
      const auto expandedAt = std::chrono::steady_clock::now();
      auto expanded = tree->GeometryAt(rank);
      const double expandedMs = Elapsed(expandedAt);
      if (!expanded) { return 2; }
      const Cost full = Count(*expanded);
      expanded.reset();
      const auto instancedAt = std::chrono::steady_clock::now();
      auto instanced = tree->InstancedGeometryAt(rank);
      const double instancedMs = Elapsed(instancedAt);
      if (!instanced) { return 2; }
      const Cost bark = Count(instanced->Bark);
      const Cost leaf = Count(instanced->Leaf);
      if (full.Triangles != bark.Triangles + leaf.Triangles * instanced->Placements.size() ||
          full.Vertices != bark.Vertices + leaf.Vertices * instanced->Placements.size()) {
        std::fputs("expanded and instanced geometry have different primitive counts\n", stderr);
        return 2;
      }
      std::println("{},{},{:.6f},{:.6f},{},{},{},{:.6f},{},{},{},{},{},{}",
                   argv[arg],
                   rank,
                   growMs,
                   expandedMs,
                   full.Vertices,
                   full.Triangles,
                   full.Arrays,
                   instancedMs,
                   bark.Vertices + leaf.Vertices,
                   bark.Triangles + leaf.Triangles,
                   bark.Arrays + leaf.Arrays,
                   instanced->Placements.size(),
                   instanced->Placements.size() * sizeof(outshine::Mat4),
                   UniqueOrigins(tree->Ranks()[rank]));
    }
  }
}
