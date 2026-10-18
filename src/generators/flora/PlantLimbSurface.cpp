#include "PlantLimbSurface.h"
#include "PlantLimbPlan.h"
#include "PlantSurface.h"
#include "math/Vec3.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <numbers>
#include <string_view>
#include <vector>

namespace outshine::Generators {
namespace {
constexpr size_t kMostAlong = 16, kLeastSides = 3, kMostSides = 64;
constexpr double kLeastForward = 1e-10;

struct Station {
  PlantLimbSample Sample;
  Vec3 Lateral, Normal;
};

std::expected<Vec3, std::string_view> Unit(Vec3 vector) {
  const double length = Length(vector);
  if (!(length > 0) || !std::isfinite(length)) {
    return std::unexpected("regular finite limb frame required");
  }
  return vector * (1 / length);
}

std::expected<Vec3, std::string_view>
Transport(const Station &previous, const PlantLimbSample &next, Vec3 reference) {
  const auto chord = Unit(next.CentreM - previous.Sample.CentreM);
  if (!chord || Dot(*chord, previous.Sample.Tangent) <= kLeastForward ||
      Dot(*chord, next.Tangent) <= kLeastForward) {
    return std::unexpected("forward resolved limb stations required");
  }
  const Vec3 reflected = reference - *chord * (2 * Dot(*chord, reference));
  const Vec3 tangent =
      previous.Sample.Tangent - *chord * (2 * Dot(*chord, previous.Sample.Tangent));
  const auto second = Unit(next.Tangent - tangent);
  if (!second) { return std::unexpected(second.error()); }
  return reflected - *second * (2 * Dot(*second, reflected));
}

std::expected<std::vector<Station>, std::string_view> PrepareStations(const PlantLimbPlan &plan,
                                                                      size_t segments) {
  std::vector<Station> stations;
  stations.reserve(segments + 1);
  Vec3 reference;
  for (size_t at = 0; at <= segments; ++at) {
    const double parameter =
        at == segments ? 1 : static_cast<double>(at) * (1.0 / static_cast<double>(segments));
    const auto sample = plan.Sample(parameter);
    if (!sample) { return std::unexpected(sample.error()); }
    if (!(sample->RadiusM * Length(sample->Curvature) < 1)) {
      return std::unexpected("sampled limb tube loses local regularity");
    }
    if (at == 0) {
      size_t least = 0;
      for (size_t axis = 1; axis < 3; ++axis) {
        if (std::abs(sample->Tangent[axis]) < std::abs(sample->Tangent[least])) { least = axis; }
      }
      reference[least] = 1;
      const auto initial = Unit(reference - sample->Tangent * Dot(reference, sample->Tangent));
      if (!initial) { return std::unexpected(initial.error()); }
      reference = *initial;
    } else {
      const auto transported = Transport(stations.back(), *sample, reference);
      if (!transported) { return std::unexpected(transported.error()); }
      reference = *transported;
    }
    const auto lateral = Unit(reference - sample->Tangent * Dot(reference, sample->Tangent));
    if (!lateral) { return std::unexpected(lateral.error()); }
    stations.push_back(
        {.Sample = *sample, .Lateral = *lateral, .Normal = Cross(*lateral, sample->Tangent)});
  }
  return stations;
}

bool Append(PlantSurface &surface, Vec3 position, Vec3 normal) {
  for (size_t axis = 0; axis < 3; ++axis) {
    if (!std::isfinite(position[axis]) ||
        std::abs(position[axis]) > std::numeric_limits<float>::max() ||
        !std::isfinite(normal[axis])) {
      return false;
    }
    surface.Positions.push_back(static_cast<float>(position[axis]));
    surface.Normals.push_back(static_cast<float>(normal[axis]));
  }
  return true;
}

Vec3 Radial(const Station &station, size_t side, size_t sides) {
  const double angle =
      2 * std::numbers::pi * static_cast<double>(side) / static_cast<double>(sides);
  return station.Lateral * std::cos(angle) + station.Normal * std::sin(angle);
}

bool AppendRing(PlantSurface &mesh, const Station &station, size_t sides) {
  const auto &sample = station.Sample;
  for (size_t side = 0; side < sides; ++side) {
    const Vec3 radial = Radial(station, side, sides);
    const Vec3 angular = Cross(radial, sample.Tangent);
    const Vec3 longitudinal =
        sample.Tangent * (sample.SpeedM * (1 - sample.RadiusM * Dot(sample.Curvature, radial))) +
        radial * sample.RadiusSlopeM;
    const auto normal = Unit(Cross(longitudinal, angular));
    if (!normal || !Append(mesh, sample.CentreM + radial * sample.RadiusM, *normal)) {
      return false;
    }
  }
  return true;
}

void SideTriangles(PlantSurface &mesh, PlantLimbSurfaceDemand demand, size_t segments) {
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

bool AppendCap(PlantSurface &mesh, const Station &station, size_t sides, bool tip) {
  const auto centre = static_cast<uint32_t>(mesh.Positions.size() / 3);
  const Vec3 normal = station.Sample.Tangent * (tip ? 1.0 : -1.0);
  if (!Append(mesh, station.Sample.CentreM, normal)) { return false; }
  for (size_t side = 0; side < sides; ++side) {
    if (!Append(mesh,
                station.Sample.CentreM + Radial(station, side, sides) * station.Sample.RadiusM,
                normal)) {
      return false;
    }
    const auto a = centre + 1 + static_cast<uint32_t>(side);
    const auto b = centre + 1 + static_cast<uint32_t>((side + 1) % sides);
    if (tip) {
      mesh.Triangles.insert(mesh.Triangles.end(), {centre, b, a});
    } else {
      mesh.Triangles.insert(mesh.Triangles.end(), {centre, a, b});
    }
  }
  return true;
}
}

std::expected<PlantSurfaceSize, std::string_view>
MeasurePlantLimbSurface(const PlantLimbPlan &plan, PlantLimbSurfaceDemand demand) noexcept {
  if (plan.Nodes().size() < 2 || demand.Along < 1 || demand.Along > kMostAlong ||
      demand.Sides < kLeastSides || demand.Sides > kMostSides) {
    return std::unexpected("bounded explicit limb detail required");
  }
  const size_t stations = (plan.Nodes().size() - 1) * demand.Along + 1;
  const PlantSurfaceSize size{.Vertices = stations * demand.Sides + 2 * (demand.Sides + 1),
                              .Triangles = 2 * demand.Sides * stations};
  if (size.Vertices > demand.MaximumVertices || size.Triangles > demand.MaximumTriangles) {
    return std::unexpected("limb surface exceeds caller capacity");
  }
  return size;
}

std::expected<PlantSurface, std::string_view> BuildPlantLimbSurface(const PlantLimbPlan &plan,
                                                                    PlantLimbSurfaceDemand demand) {
  const auto size = MeasurePlantLimbSurface(plan, demand);
  if (!size) { return std::unexpected(size.error()); }
  const size_t segments = (plan.Nodes().size() - 1) * demand.Along;
  const auto stations = PrepareStations(plan, segments);
  if (!stations) { return std::unexpected(stations.error()); }
  PlantSurface mesh;
  mesh.Positions.reserve(size->Vertices * 3);
  mesh.Normals.reserve(size->Vertices * 3);
  mesh.Triangles.reserve(size->Triangles * 3);
  for (const auto &station : *stations) {
    if (!AppendRing(mesh, station, demand.Sides)) {
      return std::unexpected("finite regular limb ring required");
    }
  }
  SideTriangles(mesh, demand, segments);
  if (!AppendCap(mesh, stations->front(), demand.Sides, false) ||
      !AppendCap(mesh, stations->back(), demand.Sides, true) || !PlantSurfaceIsOriented(mesh)) {
    return std::unexpected("limb detail does not resolve the oriented surface");
  }
  return mesh;
}
}
