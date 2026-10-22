#include <scene/Geometry.h>
#include <content/GeometryAsset.h>
#include "Check.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;

std::array<std::span<const float>, 3> Views(const Geometry &geometry) {
  return {geometry.positionsOf(0), geometry.normalsOf(0), geometry.coloursOf(0)};
}

template <typename T, typename Adopt>
void Reject(Geometry &geometry,
            std::vector<T> values,
            Adopt adopt,
            GeometryAttributeError error,
            int part = 0) {
  const auto before = EncodeGeometryAsset(geometry, 4096);
  const auto saved = values;
  const auto *pointer = values.data();
  const auto result = (geometry.*adopt)(part, values);
  CHECK(!result && result.error() == error, "a failed ownership transfer reports its exact cause");
  CHECK(values.data() == pointer && values.size() == saved.size() &&
            (values.empty() ||
             std::memcmp(values.data(), saved.data(), values.size() * sizeof(T)) == 0),
        "failed validation retains the caller's allocation and all original bits");
  CHECK(before && EncodeGeometryAsset(geometry, 4096) == before,
        "a failed attribute transfer preserves the complete target product");
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Geometry geometry;
  const auto material = geometry.addSurface("streets", Material{});
  CHECK(material.has_value(), "the target owns a material");
  if (!material) { return Report(); }
  const auto part = geometry.addPart("streets", *material);
  CHECK(part.has_value(), "the target owns a part");
  if (!part) { return Report(); }
  constexpr std::array transfers{
      &Geometry::adoptPositions, &Geometry::adoptNormals, &Geometry::adoptColours};
  std::array<std::vector<float>, 3> values{std::vector<float>{0, 0, 0, 1, 0, 0, 0, 1, 0},
                                           std::vector<float>{0, 0, 1, 0, 0, 1, 0, 0, 1},
                                           std::vector<float>{1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1}};
  for (size_t at = 0; at < values.size(); ++at) {
    const auto *pointer = values[at].data();
    CHECK((geometry.*transfers[at])(*part, values[at]) && Views(geometry)[at].data() == pointer,
          "each float attribute retains its allocation across an independent transfer");
  }
  std::vector<uint32_t> triangles{0, 1, 2};
  const auto *indices = triangles.data();
  CHECK(geometry.adoptTriangles(*part, triangles) && geometry.trianglesOf(*part).data() == indices,
        "the separate index transfer retains its allocation");
  CHECK(geometry.wellFormed() && geometry.nameOf(*part) == "streets" &&
            geometry.materialOf(*part) == *material,
        "independent attribute transfers preserve metadata and produce complete native geometry");
  Reject(geometry,
         std::vector<float>{0, 0},
         &Geometry::adoptPositions,
         GeometryAttributeError::IncompleteTuple);
  Reject(geometry,
         std::vector<float>{0, 0, 2},
         &Geometry::adoptNormals,
         GeometryAttributeError::NonUnitNormal);
  Reject(geometry,
         std::vector<float>{1, 1, 1},
         &Geometry::adoptColours,
         GeometryAttributeError::IncompleteTuple);
  Reject(geometry,
         std::vector<uint32_t>{0, 1},
         &Geometry::adoptTriangles,
         GeometryAttributeError::IncompleteTuple);
  const float nan = std::numeric_limits<float>::quiet_NaN();
  Reject(geometry,
         std::vector<float>{nan, 0, 0},
         &Geometry::adoptPositions,
         GeometryAttributeError::NonFiniteValue);
  Reject(geometry,
         std::vector<float>{nan, 0, 1},
         &Geometry::adoptNormals,
         GeometryAttributeError::NonFiniteValue);
  Reject(geometry,
         std::vector<float>{1, 1, 1, nan},
         &Geometry::adoptColours,
         GeometryAttributeError::NonFiniteValue);
  for (const auto adopt : transfers) {
    Reject(geometry, std::vector<float>{0, 0, 1}, adopt, GeometryAttributeError::MissingPart, -1);
  }
  Reject(geometry,
         std::vector<uint32_t>{0, 1, 2},
         &Geometry::adoptTriangles,
         GeometryAttributeError::MissingPart,
         1);
  for (const auto adopt : transfers) {
    std::vector<float> empty;
    CHECK((geometry.*adopt)(0, empty), "an empty owned float stream can clear an attribute");
  }
  std::vector<uint32_t> empty;
  CHECK(geometry.adoptTriangles(0, empty) && geometry.positionsOf(0).empty() &&
            geometry.normalsOf(0).empty() && geometry.coloursOf(0).empty() &&
            geometry.trianglesOf(0).empty(),
        "empty ownership transfers clear all four attributes");
  return Report();
}
