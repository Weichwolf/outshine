#include "GroundPatchError.h"
#include "GroundPatchMesh.h"
#include "GroundLattice.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace outshine::Render {
namespace {
constexpr auto kSide = GroundPatchMesh::kSide;
constexpr auto kCells = GroundPatchMesh::kGroundPatchCells;
constexpr size_t kColumns = (GroundLattice::kSide - 1) / kCells;
constexpr double kInfinity = std::numeric_limits<double>::infinity();

const auto &Meshes() {
  static const std::array meshes{MakeGroundPatchMesh(1),
                                 MakeGroundPatchMesh(2),
                                 MakeGroundPatchMesh(4),
                                 MakeGroundPatchMesh(8)};
  return meshes;
}

const auto &Moments() {
  static const auto moments = [] {
    std::array<std::array<double, 6>, 4> result{};
    const auto &meshes = Meshes();
    for (size_t level = 1; level < meshes.size(); ++level) {
      if (!meshes[level]) {
        result[level].fill(kInfinity);
        continue;
      }
      for (const auto &sample : meshes[level]->Samples) {
        std::array<double, 6> delta{};
        for (size_t k = 0; k < sample.Nodes.size(); ++k) {
          const double u = static_cast<double>(sample.Nodes[k] % kSide) / kCells;
          const auto row = sample.Nodes[k] / kSide;
          const double v = static_cast<double>(row) / kCells;
          const std::array basis{u * u, v * v, u * v, u * u * v, u * v * v, u * u * v * v};
          for (size_t i = 0; i < delta.size(); ++i) { delta[i] += sample.Weights[k] * basis[i]; }
        }
        for (size_t i = 0; i < delta.size(); ++i) {
          result[level][i] = std::max(result[level][i], std::abs(delta[i]));
        }
      }
    }
    return result;
  }();
  return moments;
}

float Outward(double value) {
  return std::isfinite(value)
             ? std::nextafter(static_cast<float>(value), std::numeric_limits<float>::infinity())
             : std::numeric_limits<float>::infinity();
}

using XY = std::array<double, 2>;

XY At(const GroundInstance &tile, double u, double v) {
  XY result{};
  for (size_t i = 0; i < result.size(); ++i) {
    result[i] = (1 - v) * ((1 - u) * tile.Corners[i] + u * tile.Corners[i + 2]) +
                v * ((1 - u) * tile.Corners[i + 4] + u * tile.Corners[i + 6]);
  }
  return result;
}

double Dot(XY a, XY b) {
  return a[0] * b[0] + a[1] * b[1];
}
}

std::array<float, 4> GroundPatchHeightErrors(std::span<const float> heights, size_t patch) {
  std::array<float, GroundPatchMesh::kNodes> local{};
  for (size_t y = 0; y < 9; ++y) {
    for (size_t x = 0; x < 9; ++x) {
      local[y * 9 + x] =
          heights[(patch / 4 * 8 + y + 1) * GroundLattice::kPageSide + patch % 4 * 8 + x + 1];
    }
  }
  std::array<float, 4> result{};
  const auto &meshes = Meshes();
  for (size_t level = 1; level < result.size(); ++level) {
    result[level] =
        Outward(meshes[level] ? GroundPatchHeightError(local, meshes[level]->Samples) : kInfinity);
  }
  return result;
}

std::array<float, 4> GroundPatchWorldErrors(const GroundInstance &tile,
                                            size_t patch,
                                            const std::array<float, 4> &heightErrors) {
  const size_t row = patch / kColumns;
  const double u = static_cast<double>(patch % kColumns) / kColumns;
  const double v = static_cast<double>(row) / kColumns;
  const auto a = At(tile, u, v);
  const auto b = At(tile, u + 0.25, v);
  const auto c = At(tile, u, v + 0.25);
  const auto d = At(tile, u + 0.25, v + 0.25);
  XY bu{};
  XY cv{};
  XY duv{};
  for (size_t i = 0; i < a.size(); ++i) {
    bu[i] = b[i] - a[i];
    cv[i] = c[i] - a[i];
    duv[i] = d[i] - b[i] - c[i] + a[i];
  }
  const std::array coefficients{Dot(bu, bu),
                                Dot(cv, cv),
                                2 * (Dot(bu, cv) + Dot(a, duv)),
                                2 * Dot(bu, duv),
                                2 * Dot(cv, duv),
                                Dot(duv, duv)};
  double upLength = 0;
  double crossLength = 0;
  for (size_t i = 0; i < 3; ++i) {
    upLength += double{tile.Row[8 + i]} * tile.Row[8 + i];
    const double cross = duv[0] * tile.Row[i] + duv[1] * tile.Row[4 + i];
    crossLength += cross * cross;
  }
  upLength = std::sqrt(upLength);
  crossLength = std::sqrt(crossLength);
  std::array<float, 4> result{};
  for (size_t level = 1; level < result.size(); ++level) {
    const auto &moments = Moments()[level];
    double sag = 0;
    for (size_t i = 0; i < moments.size(); ++i) { sag += std::abs(coefficients[i]) * moments[i]; }
    const double bound = (heightErrors[level] + 0.5 * std::abs(tile.SagInv) * sag) * upLength +
                         moments[2] * crossLength;
    result[level] = Outward(bound);
  }
  return result;
}
}
