#include "PlantAxisSurface.h"
#include "PlantArc.h"

#include "math/Vec3.h"
#include <algorithm>
#include <vector>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <numbers>
#include <string_view>

namespace outshine::Generators {
namespace {
constexpr size_t kMinimumSegments = 2;
constexpr size_t kMaximumSegments = 1024;
constexpr size_t kMinimumSides = 3;
constexpr size_t kMaximumSides = 128;

struct Section {
  Vec3 Centre, Tangent;
  double Radius = 0, Angle = 0;
};

Section SectionAt(const PlantAxis &axis, double parameter) {
  const auto frame = PlantArc{.LengthM = axis.LengthM, .BendRad = axis.Bend}.FrameAt(parameter);
  return {.Centre = frame.Centre,
          .Tangent = frame.Tangent,
          .Radius = axis.RadiiM[0] + (axis.RadiiM[1] - axis.RadiiM[0]) * parameter,
          .Angle = axis.Bend * parameter};
}

Vec3 Rotate(const PlantAxis &axis, Vec3 local) {
  Vec3 result;
  for (size_t row = 0; row < 3; ++row) {
    for (size_t column = 0; column < 3; ++column) {
      result[row] += axis.Frame[row * 3 + column] * local[column];
    }
  }
  return result;
}

struct Vertex {
  Vec3 Position, Normal;
};

void Append(PlantSurface &mesh, const PlantAxis &axis, Vertex vertex) {
  const Vec3 point = Rotate(axis, vertex.Position);
  const Vec3 normal = Rotate(axis, vertex.Normal);
  for (size_t component = 0; component < 3; ++component) {
    mesh.Positions.push_back(static_cast<float>(point[component] + axis.OriginM[component]));
    mesh.Normals.push_back(static_cast<float>(normal[component]));
  }
}

bool AppendRing(PlantSurface &mesh, const PlantAxis &axis, const Section &section, size_t sides) {
  const double slope = axis.RadiiM[1] - axis.RadiiM[0];
  for (size_t side = 0; side < sides; ++side) {
    const double angle =
        static_cast<double>(side) * 2 * std::numbers::pi / static_cast<double>(sides);
    const double along = std::sin(angle);
    const Vec3 radial{
        {std::cos(angle), -along * std::sin(section.Angle), along * std::cos(section.Angle)}};
    const double axial = axis.LengthM - axis.Bend * section.Radius * along;
    const Vec3 normal = radial * axial - section.Tangent * slope;
    const double length = Length(normal);
    if (!(length > 0)) { return false; }
    Append(
        mesh,
        axis,
        {.Position = section.Centre + radial * section.Radius, .Normal = normal * (1.0 / length)});
  }
  return true;
}

void SideTriangles(PlantSurface &mesh, const PlantAxisSurfaceDemand &demand) {
  const size_t segments = demand.Segments;
  const size_t sides = demand.Sides;
  for (size_t row = 0; row < segments; ++row) {
    for (size_t side = 0; side < sides; ++side) {
      const auto a = static_cast<uint32_t>(row * sides + side);
      const auto b = static_cast<uint32_t>(row * sides + (side + 1) % sides);
      const auto d = static_cast<uint32_t>((row + 1) * sides + side);
      const auto c = static_cast<uint32_t>((row + 1) * sides + (side + 1) % sides);
      mesh.Triangles.insert(mesh.Triangles.end(), {a, d, c, a, c, b});
    }
  }
}

void AppendCap(
    PlantSurface &mesh, const PlantAxis &axis, const Section &section, size_t sides, bool tip) {
  const auto centre = static_cast<uint32_t>(mesh.Positions.size() / 3);
  const Vec3 normal = section.Tangent * (tip ? 1.0 : -1.0);
  Append(mesh, axis, {.Position = section.Centre, .Normal = normal});
  for (size_t side = 0; side < sides; ++side) {
    const double angle =
        static_cast<double>(side) * 2 * std::numbers::pi / static_cast<double>(sides);
    const Vec3 radial{{std::cos(angle),
                       -std::sin(angle) * std::sin(section.Angle),
                       std::sin(angle) * std::cos(section.Angle)}};
    Append(mesh, axis, {.Position = section.Centre + radial * section.Radius, .Normal = normal});
  }
  for (size_t side = 0; side < sides; ++side) {
    const auto a = centre + 1 + static_cast<uint32_t>(side);
    const auto b = centre + 1 + static_cast<uint32_t>((side + 1) % sides);
    if (tip) {
      mesh.Triangles.insert(mesh.Triangles.end(), {centre, b, a});
    } else {
      mesh.Triangles.insert(mesh.Triangles.end(), {centre, a, b});
    }
  }
}

bool Representable(const PlantAxis &axis) {
  const double radius = std::max(axis.RadiiM[0], axis.RadiiM[1]);
  if (std::abs(axis.Bend) * radius >= axis.LengthM) { return false; }
  const double extent = axis.LengthM + radius;
  for (size_t row = 0; row < 3; ++row) {
    double reach = std::abs(axis.OriginM[row]);
    for (size_t column = 0; column < 3; ++column) {
      reach += std::abs(axis.Frame[row * 3 + column]) * extent;
    }
    if (!std::isfinite(reach) || reach > std::numeric_limits<float>::max()) { return false; }
  }
  return true;
}
}

std::expected<PlantSurfaceSize, std::string_view>
MeasurePlantAxisSurface(const PlantPlan &plan, PlantAxisSurfaceDemand demand) {
  const PlantAxis *axis = plan.Axis(demand.Axis);
  if (axis == nullptr || demand.Segments < kMinimumSegments || demand.Segments > kMaximumSegments ||
      demand.Sides < kMinimumSides || demand.Sides > kMaximumSides || !Representable(*axis)) {
    return std::unexpected("plant axis surface requires regular dimensions and bounded detail");
  }
  const size_t vertices = (demand.Segments + 1) * demand.Sides + 2 * (demand.Sides + 1);
  const PlantSurfaceSize size{.Vertices = vertices,
                              .Triangles = 2 * demand.Sides * (demand.Segments + 1)};
  if (size.Vertices > demand.MaximumVertices || size.Triangles > demand.MaximumTriangles) {
    return std::unexpected("plant axis detail exceeds demand budget");
  }
  return size;
}

std::expected<PlantSurface, std::string_view> BuildPlantAxisSurface(const PlantPlan &plan,
                                                                    PlantAxisSurfaceDemand demand) {
  const auto size = MeasurePlantAxisSurface(plan, demand);
  if (!size) { return std::unexpected(size.error()); }
  const PlantAxis &axis = *plan.Axis(demand.Axis);
  PlantSurface mesh;
  mesh.Positions.reserve(size->Vertices * 3);
  mesh.Normals.reserve(size->Vertices * 3);
  mesh.Triangles.reserve(size->Triangles * 3);
  for (size_t row = 0; row <= demand.Segments; ++row) {
    if (!AppendRing(
            mesh,
            axis,
            SectionAt(axis, static_cast<double>(row) / static_cast<double>(demand.Segments)),
            demand.Sides)) {
      return std::unexpected("plant axis normal is not representable");
    }
  }
  SideTriangles(mesh, demand);
  AppendCap(mesh, axis, SectionAt(axis, 0), demand.Sides, false);
  AppendCap(mesh, axis, SectionAt(axis, 1), demand.Sides, true);
  if (!PlantSurfaceIsOriented(mesh)) {
    return std::unexpected("plant axis detail does not resolve the oriented surface");
  }
  return mesh;
}
}
