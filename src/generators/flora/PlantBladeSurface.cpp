#include "PlantBladeSurface.h"
#include "PlantArc.h"
#include "math/Vec3.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <numbers>
#include <optional>
#include <string_view>

namespace outshine::Generators {
namespace {
constexpr size_t kMinimumSegments = 2;
constexpr size_t kMaximumSegments = 1024;
constexpr size_t kMaximumAcross = 32;
constexpr int kMaximumLobes = 32;

struct Coordinate {
  double T = 0, S = 0;
  int Side = 1;
  bool PetioleSide = false;
};

struct Sample {
  Vec3 Position, Normal;
};

struct Width {
  double Value = 0, Derivative = 0;
};

Width WidthAt(const PlantBlade &blade, double parameter) {
  const double denominator = blade.Widest + (1 - 2 * blade.Widest) * parameter;
  const double warped = (1 - blade.Widest) * parameter / denominator;
  const double derivative = blade.Widest * (1 - blade.Widest) / (denominator * denominator);
  const double phase = std::numbers::pi * blade.Lobes * (warped - 0.5);
  const double sine = std::sin(phase);
  const double margin = 1 - blade.LobeDepth * sine * sine;
  const double marginDerivative =
      -blade.LobeDepth * std::numbers::pi * blade.Lobes * std::sin(2 * phase) * derivative;
  const double base = parameter == 1 ? 0
                                     : (1 - blade.BaseFill) * std::sin(std::numbers::pi * warped) +
                                           blade.BaseFill * (1 - parameter);
  const double baseDerivative =
      (1 - blade.BaseFill) * std::numbers::pi * std::cos(std::numbers::pi * warped) * derivative -
      blade.BaseFill;
  return {.Value = blade.WidthM / 2 * base * margin,
          .Derivative = blade.WidthM / 2 * (baseDerivative * margin + base * marginDerivative)};
}

std::optional<Sample> SampleAt(const PlantBlade &blade, Coordinate coordinate) {
  const bool petiole = coordinate.T < 0 || coordinate.PetioleSide;
  const double parameter = std::max(coordinate.T, 0.0);
  auto frame =
      PlantArc{.LengthM = blade.LengthM, .BendRad = blade.BendRad, .TwistRad = blade.TwistRad}
          .FrameAt(parameter);
  if (petiole) { frame.Centre[1] += coordinate.T * blade.LengthM; }
  const Width width = WidthAt(blade, parameter);
  const double derivative = petiole ? 0 : width.Derivative;
  const double bend = petiole ? 0 : blade.BendRad;
  const double twist = petiole ? 0 : blade.TwistRad;
  const Vec3 dtLateral =
      frame.Normal * twist - frame.Tangent * (bend * std::sin(blade.TwistRad * parameter));
  const Vec3 dtNormal =
      frame.Lateral * -twist - frame.Tangent * (bend * std::cos(blade.TwistRad * parameter));
  const double spread = std::abs(coordinate.S);
  const double profile = blade.FoldPower == 1 ? spread : spread * spread;
  const double slope = blade.FoldPower * (blade.FoldPower == 1 ? 1.0 : spread) * coordinate.Side;
  const Vec3 point = frame.Centre + frame.Lateral * (coordinate.S * width.Value) +
                     frame.Normal * (profile * width.Value * blade.Fold);
  const Vec3 dt = frame.Tangent * blade.LengthM + frame.Lateral * (coordinate.S * derivative) +
                  dtLateral * (coordinate.S * width.Value) +
                  frame.Normal * (profile * blade.Fold * derivative) +
                  dtNormal * (profile * blade.Fold * width.Value);
  const Vec3 ds = frame.Lateral + frame.Normal * (blade.Fold * slope);
  const Vec3 normal = Cross(ds, dt);
  const double length = std::hypot(normal[0], normal[1], normal[2]);
  if (!(length > 0) || !std::isfinite(length)) { return std::nullopt; }
  return Sample{.Position = point, .Normal = normal * (1.0 / length)};
}

struct Corner {
  size_t Row = 0, Column = 0;
  int Side = 1;
  bool PetioleSide = false;
};

struct Grid {
  size_t Segments = 0, Across = 0;
  bool Petiole = false, CollapsedBase = false;

  [[nodiscard]] size_t Rows() const { return Segments + 1 + (Petiole ? 1 : 0); }

  [[nodiscard]] size_t RowWidth() const { return 2 * (Across + 1); }

  [[nodiscard]] bool Collapsed(size_t row) const {
    return row + 1 == Rows() || (row == 0 && CollapsedBase);
  }

  [[nodiscard]] size_t Vertices() const {
    return Rows() * RowWidth() - 2 * Across * (1 + (CollapsedBase ? 1 : 0)) +
           (Petiole ? RowWidth() : 0);
  }

  [[nodiscard]] size_t Triangles() const {
    return 4 * Across * (Rows() - 1) - 2 * Across * (1 + (CollapsedBase ? 1 : 0));
  }

  [[nodiscard]] uint32_t Index(Corner corner) const {
    size_t offset = corner.Row * RowWidth() - (CollapsedBase && corner.Row > 0 ? 2 * Across : 0);
    if (Petiole && corner.Row > 1) { offset += RowWidth(); }
    if (Collapsed(corner.Row)) { return static_cast<uint32_t>(offset + (corner.Side > 0 ? 1 : 0)); }
    if (Petiole && corner.Row == 1 && corner.PetioleSide) { offset += RowWidth(); }
    offset += corner.Side > 0 ? Across + 1 + corner.Column - Across : corner.Column;
    return static_cast<uint32_t>(offset);
  }
};

bool AppendRow(
    PlantSurface &mesh, const PlantBlade &blade, const Grid &grid, size_t row, bool petioleSide) {
  const double parameter =
      grid.Petiole && row == 0
          ? -blade.PetioleM / blade.LengthM
          : static_cast<double>(row - (grid.Petiole ? 1 : 0)) / static_cast<double>(grid.Segments);
  for (const int side : {-1, 1}) {
    const size_t count = grid.Collapsed(row) ? 1 : grid.Across + 1;
    for (size_t at = 0; at < count; ++at) {
      const size_t column = grid.Collapsed(row) ? grid.Across : at + (side > 0 ? grid.Across : 0);
      const double transverse = static_cast<double>(column) / static_cast<double>(grid.Across) - 1;
      const auto sample = SampleAt(
          blade, {.T = parameter, .S = transverse, .Side = side, .PetioleSide = petioleSide});
      if (!sample) { return false; }
      for (size_t axis = 0; axis < 3; ++axis) {
        mesh.Positions.push_back(static_cast<float>(sample->Position[axis]));
        mesh.Normals.push_back(static_cast<float>(sample->Normal[axis]));
      }
      mesh.Texture.push_back(static_cast<float>((transverse + 1) / 2));
      mesh.Texture.push_back(static_cast<float>(parameter));
    }
  }
  return true;
}

void AppendTriangles(PlantSurface &mesh, const Grid &grid) {
  for (size_t row = 0; row + 1 < grid.Rows(); ++row) {
    for (size_t column = 0; column < 2 * grid.Across; ++column) {
      const int side = column < grid.Across ? -1 : 1;
      const bool petiole = grid.Petiole && row == 0;
      const auto a =
          grid.Index({.Row = row, .Column = column, .Side = side, .PetioleSide = petiole});
      const auto b =
          grid.Index({.Row = row, .Column = column + 1, .Side = side, .PetioleSide = petiole});
      const auto c =
          grid.Index({.Row = row + 1, .Column = column + 1, .Side = side, .PetioleSide = petiole});
      const auto d =
          grid.Index({.Row = row + 1, .Column = column, .Side = side, .PetioleSide = petiole});
      for (const std::array<uint32_t, 3> triangle : {std::array{a, b, c}, std::array{a, c, d}}) {
        if (triangle[0] != triangle[1] && triangle[1] != triangle[2] &&
            triangle[0] != triangle[2]) {
          mesh.Triangles.insert(mesh.Triangles.end(), triangle.begin(), triangle.end());
        }
      }
    }
  }
}

bool Valid(const PlantBlade &blade) {
  const std::array values{blade.LengthM,
                          blade.WidthM,
                          blade.Widest,
                          blade.LobeDepth,
                          blade.BaseFill,
                          blade.BendRad,
                          blade.Fold,
                          blade.TwistRad,
                          blade.PetioleM};
  if (!std::ranges::all_of(values, [](double value) { return std::isfinite(value); })) {
    return false;
  }
  const double extent =
      blade.LengthM + blade.PetioleM + blade.WidthM / 2 * (1 + std::abs(blade.Fold));
  return blade.LengthM > 0 && blade.WidthM > 0 && blade.Widest > 0 && blade.Widest < 1 &&
         blade.Lobes >= 0 && blade.Lobes <= kMaximumLobes && blade.LobeDepth >= 0 &&
         blade.LobeDepth < 1 && blade.BaseFill >= 0 && blade.BaseFill <= 1 && blade.PetioleM >= 0 &&
         (blade.FoldPower == 1 || blade.FoldPower == 2) &&
         std::abs(blade.BendRad) <= std::numbers::pi &&
         std::abs(blade.BendRad) * blade.WidthM / 2 * (1 + std::abs(blade.Fold)) < blade.LengthM &&
         std::isfinite(blade.PetioleM / blade.LengthM) && std::isfinite(extent) &&
         extent <= std::numeric_limits<float>::max();
}
}

std::expected<PlantSurfaceSize, std::string_view>
MeasurePlantBladeSurface(const PlantBlade &blade, PlantBladeSurfaceDemand demand) {
  if (!Valid(blade) || demand.Segments < kMinimumSegments || demand.Segments > kMaximumSegments ||
      demand.Across == 0 || demand.Across > kMaximumAcross) {
    return std::unexpected(
        "plant blade requires finite regular parameters and bounded subdivision");
  }
  const Grid grid{.Segments = demand.Segments,
                  .Across = demand.Across,
                  .Petiole = blade.PetioleM > 0 && blade.BaseFill > 0,
                  .CollapsedBase = blade.BaseFill == 0};
  if (grid.Vertices() > demand.MaximumVertices || grid.Triangles() > demand.MaximumTriangles) {
    return std::unexpected("plant blade detail exceeds demand budget");
  }
  return PlantSurfaceSize{.Vertices = grid.Vertices(), .Triangles = grid.Triangles()};
}

std::expected<PlantSurface, std::string_view>
BuildPlantBladeSurface(const PlantBlade &blade, PlantBladeSurfaceDemand demand) {
  const auto size = MeasurePlantBladeSurface(blade, demand);
  if (!size) { return std::unexpected(size.error()); }
  const Grid grid{.Segments = demand.Segments,
                  .Across = demand.Across,
                  .Petiole = blade.PetioleM > 0 && blade.BaseFill > 0,
                  .CollapsedBase = blade.BaseFill == 0};
  PlantSurface mesh;
  mesh.Positions.reserve(size->Vertices * 3);
  mesh.Normals.reserve(size->Vertices * 3);
  mesh.Texture.reserve(size->Vertices * 2);
  mesh.Triangles.reserve(size->Triangles * 3);
  for (size_t row = 0; row < grid.Rows(); ++row) {
    if (!AppendRow(mesh, blade, grid, row, false) ||
        (grid.Petiole && row == 1 && !AppendRow(mesh, blade, grid, row, true))) {
      return std::unexpected("plant blade normal is not representable");
    }
  }
  AppendTriangles(mesh, grid);
  if (!PlantSurfaceIsOriented(mesh)) {
    return std::unexpected("plant blade subdivision does not resolve orientation");
  }
  return mesh;
}
}
