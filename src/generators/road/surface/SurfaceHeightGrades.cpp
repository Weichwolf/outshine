#include "SurfaceHeightProgramInternal.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>
#include <span>
#include <vector>

namespace outshine::Generators::HeightAssembly {
namespace {
constexpr double kHighGradeThreshold = 0.1;
constexpr double kHighGradeReserve = 0.002;
constexpr double kLowGradeReserve = 0.0002;
constexpr double kAxisAreaThreshold = 1e-12;

double ReservedGrade(double grade) {
  return grade - (grade > kHighGradeThreshold ? kHighGradeReserve : kLowGradeReserve);
}

bool StairFacets(const Input &input, size_t owner, System &system) {
  const auto &source = input.Sources[owner];
  const double reserve = std::cos(std::numbers::pi / 16);
  const double bound = ReservedGrade(source.Grade) * reserve;
  for (const auto face : source.Faces) {
    const auto gradient = Gradients(input.Points, face, false);

    for (size_t direction = 0; direction < 16; ++direction) {
      const double angle = static_cast<double>(direction) * 2 * std::numbers::pi / 16;
      const PlanarPoint normal{std::cos(angle), std::sin(angle)};
      const auto values = Along(gradient, normal);
      system.Add(face, values, bound);
      ++system.Work.GradientRows;

      if (!system.Check()) { return false; }
    }
  }
  return true;
}

bool DirectionalFacets(const Input &input, Workspace &workspace, size_t owner, System &system) {
  const auto &source = input.Sources[owner];
  std::vector<bool> covered(source.Faces.size());
  const auto height = static_cast<float>(system.HeightBound);
  const double epsilon =
      (std::nextafter(height, std::numeric_limits<float>::infinity()) - height) / 2;
  std::vector<uint32_t> candidates;
  for (size_t segment = 1; segment < source.Axis.size(); ++segment) {
    auto tangent = Difference(source.Axis[segment], source.Axis[segment - 1]);
    const double length = std::hypot(tangent[0], tangent[1]);
    for (auto &v : tangent) { v /= length; }
    const std::array vectors{tangent,
                             PlanarPoint{-tangent[0], -tangent[1]},
                             PlanarPoint{-tangent[1], tangent[0]},
                             PlanarPoint{tangent[1], -tangent[0]}};
    if (!Influence(input,
                   workspace,
                   owner,
                   source.Axis[segment - 1],
                   source.Axis[segment],
                   kAxisAreaThreshold,
                   candidates,
                   system)) {
      return false;
    }
    for (const auto at : candidates) {
      covered[at] = true;
      const auto face = source.Faces[at];
      const auto gradient = Gradients(input.Points, face, false);
      const auto cast = Gradients(input.Points, face, true);
      for (size_t direction = 0; direction < 4; ++direction) {
        const double limit = direction < 2 ? source.Grade : input.Crossfall;
        const auto values = Along(gradient, vectors[direction]);
        system.Add(face, values, ReservedGrade(limit));
        ++system.Work.GradientRows;
        const auto quantized = Along(cast, vectors[direction]);
        double sum = 0;
        for (const double v : quantized) { sum += std::abs(v); }
        const double allowed = limit - epsilon * sum;
        if (allowed < 0) { return false; }
        system.Add(face, quantized, allowed);
        ++system.Work.GradientRows;
      }
      if (!system.Check()) { return false; }
    }
  }
  return std::ranges::all_of(covered, [](bool v) { return v; });
}

}

bool Facets(const Input &input, Workspace &workspace, size_t owner, System &system) {
  return input.Sources[owner].Steps ? StairFacets(input, owner, system)
                                    : DirectionalFacets(input, workspace, owner, system);
}
}
