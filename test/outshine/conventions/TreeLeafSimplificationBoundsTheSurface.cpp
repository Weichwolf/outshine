#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include "Check.h"
#include "TreeLeaf.h"

namespace {
using namespace outshine;
using namespace outshine::Generators;

double Area(const TreeMesh &mesh) {
  double area = 0;
  for (size_t at = 0; at < mesh.LeafIdx.size(); at += 3) {
    std::array<Vec3, 3> triangle;
    for (size_t corner = 0; corner < 3; ++corner) {
      const size_t index = mesh.LeafIdx[at + corner] * TreeMesh::kLeafFloats;
      triangle[corner] = {{mesh.LeafVerts[index], mesh.LeafVerts[index + 1], mesh.LeafVerts[index + 2]}};
    }
    area += Length(Cross(triangle[1] - triangle[0], triangle[2] - triangle[0])) * 0.5;
  }
  return area;
}

std::optional<Vec3> Sample(const TreeMesh &mesh, double u, double v) {
  for (size_t at = 0; at < mesh.LeafIdx.size(); at += 3) {
    const float *a = &mesh.LeafVerts[mesh.LeafIdx[at] * TreeMesh::kLeafFloats];
    const float *b = &mesh.LeafVerts[mesh.LeafIdx[at + 1] * TreeMesh::kLeafFloats];
    const float *c = &mesh.LeafVerts[mesh.LeafIdx[at + 2] * TreeMesh::kLeafFloats];
    const double determinant = (b[7] - c[7]) * (a[6] - c[6]) +
                               (c[6] - b[6]) * (a[7] - c[7]);
    if (std::abs(determinant) < 1e-12) { continue; }
    const double wa = ((b[7] - c[7]) * (u - c[6]) + (c[6] - b[6]) * (v - c[7])) / determinant;
    const double wb = ((c[7] - a[7]) * (u - c[6]) + (a[6] - c[6]) * (v - c[7])) / determinant;
    const double wc = 1.0 - wa - wb;
    if (std::min({wa, wb, wc}) < -1e-6) { continue; }
    return Vec3{{a[0] * wa + b[0] * wb + c[0] * wc,
                 a[1] * wa + b[1] * wb + c[1] * wc,
                 a[2] * wa + b[2] * wb + c[2] * wc}};
  }
  return std::nullopt;
}
}

int main() {
  using namespace outshine::Test;
  bool reduced = false;
  for (const bool irregular : {false, true}) {
    auto leaf = TreeSpecies::kLeafUnsaid;
    leaf.Segments = 56;
    leaf.Width = 0.5f;
    leaf.Fold = irregular ? 0.7f : 0.1f;
    leaf.Curve = irregular ? 0.8f : 0.18f;
    leaf.BaseSkew = irregular ? 0.6f : 0.0f;
    leaf.BaseFill = 0.12f;
    leaf.Serration = irregular ? 0.8f : 0.0f;
    TreeMesh reference;
    TreeLeaf::Build(leaf, reference);
    for (const float tolerance : {0.01f, 0.04f, 0.1f}) {
      TreeMesh simplified;
      constexpr float areaTolerance = 0.02f;
      TreeLeaf::Build(leaf, simplified, tolerance, areaTolerance);
      const double areaError = std::abs(Area(simplified) / Area(reference) - 1.0);
      CHECK(areaError <= areaTolerance + 1e-6,
            "blade simplification preserves the declared one-sided area budget");
      reduced = reduced || simplified.LeafIdx.size() < reference.LeafIdx.size();
      double largest = 0;
      bool covered = true;
      for (int y = 0; y <= 64; ++y) {
        for (int x = 0; x <= 64; ++x) {
          const auto expected = Sample(reference, x / 64.0, y / 64.0);
          const auto actual = Sample(simplified, x / 64.0, y / 64.0);
          covered = covered && expected.has_value() && actual.has_value();
          if (expected && actual) { largest = std::max(largest, Length(*expected - *actual)); }
        }
      }
      CHECK(covered, "both triangulated leaves cover the complete parameter domain");
      CHECK(largest <= tolerance + 1e-6,
            "independent barycentric samples obey the declared surface deviation");
      std::printf("leaf irregular %d tolerance %.3f triangles %zu -> %zu largest deviation %.6f\n",
                  irregular, tolerance, reference.LeafIdx.size() / 3,
                  simplified.LeafIdx.size() / 3, largest);
    }
  }
  CHECK(reduced, "the bounded selection removes redundant blade stations");
  return Report();
}
