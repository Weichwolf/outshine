#include "Check.h"
#include "GroundPatchMesh.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <span>
#include <utility>

namespace {
using namespace outshine::Render;
using Point = std::array<double, 2>;

Point XY(uint32_t id) {
  return {static_cast<double>(id % 9) / 8, static_cast<double>(id / 9) / 8};
}

double Area(Point a, Point b, Point c) {
  return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
}

double Coarse(std::span<const GroundPatchFace> faces, const std::array<float, 81> &h, Point p) {
  for (const auto &face : faces) {
    const Point a = XY(face[0]), b = XY(face[1]), c = XY(face[2]);
    const double area = Area(a, b, c);
    const std::array w{Area(p, b, c) / area, Area(a, p, c) / area, Area(a, b, p) / area};
    if (std::ranges::all_of(w, [](double v) { return v >= -1e-12; })) {
      return w[0] * h[face[0]] + w[1] * h[face[1]] + w[2] * h[face[2]];
    }
  }
  return std::numeric_limits<double>::quiet_NaN();
}

double Fine(const std::array<float, 81> &h, Point p) {
  const auto x = static_cast<size_t>(p[0] * 8), y = static_cast<size_t>(p[1] * 8);
  const double u = p[0] * 8 - static_cast<double>(x), v = p[1] * 8 - static_cast<double>(y);
  const size_t a = y * 9 + x;
  return u >= v ? h[a] * (1 - u) + h[a + 1] * (u - v) + h[a + 10] * v
                : h[a] * (1 - v) + h[a + 9] * (v - u) + h[a + 10] * u;
}

void Topology(const GroundPatchMesh &mesh) {
  std::map<std::pair<uint32_t, uint32_t>, unsigned> edges;
  double area = 0;
  for (const auto &f : mesh.Faces) {
    const double signedArea = Area(XY(f[0]), XY(f[1]), XY(f[2]));
    CHECK(signedArea < 0, "each terrain triangle retains source winding");
    area -= signedArea / 2;
    for (size_t k = 0; k < 3; ++k) {
      const auto [a, b] = std::minmax(f[k], f[(k + 1) % 3]);
      ++edges[{a, b}];
    }
  }
  CHECK_NEAR(area, 1.0, 1e-12, "area", "the complete patch is covered once");
  size_t boundary = 0;
  for (const auto &[edge, count] : edges) {
    CHECK(count == 1 || count == 2, "mesh edges remain manifold");
    if (count != 1) { continue; }
    ++boundary;
    const auto a = XY(edge.first), b = XY(edge.second);
    CHECK((a[0] == b[0] && (a[0] == 0 || a[0] == 1)) || (a[1] == b[1] && (a[1] == 0 || a[1] == 1)),
          "single edges lie only on the original perimeter");
    CHECK_NEAR(std::hypot(a[0] - b[0], a[1] - b[1]),
               0.125,
               1e-12,
               "length",
               "every original boundary segment remains");
  }
  CHECK(boundary == 32, "all neighboring patch connections remain exact");
}
}

int main() {
  using namespace outshine::Test;
  for (uint32_t invalid : {0u, 3u, 16u}) {
    CHECK(!MakeGroundPatchMesh(invalid), "invalid strides fail explicitly");
  }
  for (uint32_t stride : {1u, 2u, 4u, 8u}) {
    const auto mesh = MakeGroundPatchMesh(stride);
    CHECK(mesh.has_value(), "the native common refinement builds");
    if (!mesh) { continue; }
    Topology(*mesh);
    std::array<float, 81> h{};
    for (size_t i = 0; i < h.size(); ++i) {
      h[i] = static_cast<float>(std::sin(static_cast<double>(i) * 0.7) * 19);
    }
    const double bound = GroundPatchHeightError(h, mesh->Samples);
    double observed = 0;
    bool finite = true;
    for (unsigned y = 0; y < 64; ++y) {
      for (unsigned x = 0; x < 64; ++x) {
        const Point p{(x + 0.37) / 64, (y + 0.23) / 64};
        const double difference = std::abs(Fine(h, p) - Coarse(mesh->Faces, h, p));
        finite &= std::isfinite(difference);
        observed = std::max(observed, difference);
      }
    }
    CHECK(finite && observed <= bound + 1e-11,
          "independent dense interpolation stays inside the bound");
    for (size_t i = 0; i < h.size(); ++i) {
      const auto p = XY(static_cast<uint32_t>(i));
      h[i] = static_cast<float>(17 + 2 * p[0] - 3 * p[1]);
    }
    CHECK(GroundPatchHeightError(h, mesh->Samples) < 1e-11,
          "affine terrain keeps zero geometric error");
    h[3 * 9 + 3] += 19;
    if (stride > 1) {
      CHECK(GroundPatchHeightError(h, mesh->Samples) > 10,
            "an omitted peak is retained in the error bound");
    }
    h[0] = std::numeric_limits<float>::quiet_NaN();
    CHECK(std::isinf(GroundPatchHeightError(h, mesh->Samples)),
          "invalid height data cannot select a coarse mesh");
  }
  return Report();
}
