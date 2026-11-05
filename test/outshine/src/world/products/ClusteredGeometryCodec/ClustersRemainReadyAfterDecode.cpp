#include "ClusteredGeometryCodec.h"
#include "ClusterCook.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr size_t kBytes = 16384;

Geometry Quad() {
  Geometry result;
  const auto material = result.addSurface("concrete", Material{});
  CHECK(material, "the fixture has a native material");
  if (!material) { return result; }
  const auto part = result.addPart("surface", *material);
  CHECK(part, "the fixture has a native mesh part");
  if (!part) { return result; }
  CHECK(result.setPositions(*part, std::array<float, 12>{0, 0, 0, 3, 0, 0, 3, 3, 0, 0, 3, 0}) &&
            result.setTriangles(*part, std::array<uint32_t, 6>{0, 1, 2, 0, 2, 3}),
        "the source contains two indexed triangles");
  return result;
}
}

int main() {
  auto geometry = Quad();
  auto cooked =
      CookClusters({.PositionsM = geometry.positionsOf(0), .Indices = geometry.trianglesOf(0)}, 1);
  CHECK(cooked && cooked->Clusters.size() == 2, "each triangle receives its own cooked cluster");
  if (!cooked) { return Report(); }
  CHECK(geometry.setTriangles(0, cooked->Index), "the serialized index order is the cooked order");
  GeometryClusters clusters;
  clusters.push_back(std::move(cooked->Clusters));
  auto encoded = EncodeClusteredGeometry(geometry, clusters, kBytes);
  CHECK(encoded, "complete native mesh and clusters encode together");
  if (!encoded) { return Report(); }
  const auto decoded = DecodeClusteredGeometry(*encoded, kBytes);
  CHECK(decoded && decoded->Clusters == clusters &&
            std::ranges::equal(decoded->Mesh.trianglesOf(0), geometry.trianglesOf(0)),
        "a hit restores exact ready-to-use clusters and index order without a cluster cook");
  for (size_t size = 0; size < encoded->size(); ++size) {
    CHECK(!DecodeClusteredGeometry(std::span(*encoded).first(size), kBytes),
          "every truncated cluster or mesh record fails before becoming a product");
  }
  encoded->push_back(0);
  CHECK(!DecodeClusteredGeometry(*encoded, kBytes), "trailing bytes cannot hide another layout");
  CHECK(!EncodeClusteredGeometry(geometry, clusters, 1), "encoded bytes obey caller admission");
  clusters[0][1].First = 0;
  CHECK(!EncodeClusteredGeometry(geometry, clusters, kBytes),
        "overlapping clusters cannot claim the same index storage");
  clusters[0][1].First = 3;
  clusters[0][1].Count = 0;
  CHECK(!EncodeClusteredGeometry(geometry, clusters, kBytes), "missing triangles cannot publish");
  clusters[0][1].Count = 3;
  clusters[0][0].SelfRadius = std::numeric_limits<float>::quiet_NaN();
  CHECK(!EncodeClusteredGeometry(geometry, clusters, kBytes), "nonfinite culling bounds fail");
  clusters.clear();
  CHECK(!EncodeClusteredGeometry(geometry, clusters, kBytes), "every part needs its own clusters");
  return Report();
}
