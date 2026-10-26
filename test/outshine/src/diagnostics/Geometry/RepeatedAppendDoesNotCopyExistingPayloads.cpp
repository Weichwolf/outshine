#include "Check.h"
#include "Geometry.h"
#include "Heap.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <print>
#include <vector>

namespace {
constexpr size_t kFragments = 128;
constexpr size_t kQuads = 64;
constexpr size_t kAmortizedAllocationFactor = 4;
constexpr size_t kPositionFloats = 3;

outshine::Geometry Fragment() {
  std::vector<float> positions;
  std::vector<uint32_t> indices;
  for (uint32_t quad = 0; quad < kQuads; ++quad) {
    const auto x = static_cast<float>(quad);
    const auto base = static_cast<uint32_t>(positions.size() / kPositionFloats);
    const std::array corners{x, 0.0f, 0.0f, x + 1, 0.0f, 0.0f, x + 1, 0.0f, 1.0f, x, 0.0f, 1.0f};
    positions.insert(positions.end(), corners.begin(), corners.end());
    const std::array triangles{base, base + 1, base + 2, base, base + 2, base + 3};
    indices.insert(indices.end(), triangles.begin(), triangles.end());
  }
  outshine::Geometry geometry;
  const auto part = geometry.addPart("road-fragment", {});
  if (part) {
    if (!geometry.setPositions(*part, positions) || !geometry.setTriangles(*part, indices)) {
      return {};
    }
  }
  return geometry;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Geometry fragment = Fragment();
  CHECK(fragment.wellFormed(), "the measured road fragment is complete native geometry");
  Geometry combined;
  const Heap::Tagged measured("geometry-append");
  const size_t before = Heap::TakenUnder("geometry-append");
  uintptr_t firstPositions = 0;
  bool noPrefixCopies = true;
  bool appended = true;
  for (size_t at = 0; at < kFragments; ++at) {
    appended = combined.append(fragment).has_value() && appended;
    const auto positions = reinterpret_cast<uintptr_t>(combined.positionsOf(0).data());
    if (at == 0) { firstPositions = positions; }
    noPrefixCopies = noPrefixCopies && positions == firstPositions;
  }
  const size_t bytes = Heap::TakenUnder("geometry-append") - before;
  const size_t budget = kAmortizedAllocationFactor * kFragments * fragment.storageBytes();
  std::println("append fragments={} allocated={} budget={}", kFragments, bytes, budget);
  CHECK(appended && combined.parts() == static_cast<int>(kFragments) && combined.wellFormed(),
        "all repeated fragments retain valid owned geometry");
  CHECK(noPrefixCopies, "appending a fragment never reallocates earlier mesh payloads");
  CHECK(bytes <= budget,
        "payload copies plus amortized metadata growth fit a linear allocation budget");
  CHECK(fragment.append(fragment) && fragment.parts() == 2 && fragment.wellFormed(),
        "self append preserves both independent copies");
  combined.clear();
  CHECK(combined.append(fragment) && combined.parts() == 2 && combined.wellFormed(),
        "append after clear removes stale slots before publishing the new live prefix");
  return Report();
}
