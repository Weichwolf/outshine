#include "BuildPreparedPlant.h"
#include "PlantAxisSurface.h"
#include "PlantAxisDetail.h"
#include "PlantSurface.h"
#include "math/Mat4.h"
#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
constexpr size_t kVariants = size_t{1} + std::numeric_limits<uint8_t>::max();

struct Measurement {
  PlantPlan::Summary Part;
  PlantProductCounts Counts;
  PlantSurfaceSize Wood;
};

bool Accumulate(size_t &total, size_t unit, size_t count, size_t most) {
  if (total > most || (count > 0 && unit > (most - total) / count)) { return false; }
  total += unit * count;
  return true;
}

std::expected<PlantAxisSurfaceDemand, std::string_view> AxisDemand(const PlantAxis &axis,
                                                                   PreparedPlantDemand demand) {
  size_t segments = demand.AxisSegments;
  size_t sides = demand.AxisSides;
  if (demand.AxisParametricErrorM) {
    const auto detail = SelectPlantAxisDetail(axis, *demand.AxisParametricErrorM);
    if (!detail) { return std::unexpected(detail.error()); }
    segments = detail->Segments;
    sides = detail->Sides;
  }
  return PlantAxisSurfaceDemand{.Axis = axis.Id,
                                .Segments = segments,
                                .Sides = sides,
                                .MaximumVertices = demand.Maximum.Vertices,
                                .MaximumTriangles = demand.Maximum.Triangles};
}

PlantBladeSurfaceDemand BladeDemand(PreparedPlantDemand demand) {
  return {.Segments = demand.BladeSegments,
          .Across = demand.BladeAcross,
          .MaximumVertices = demand.Maximum.Vertices,
          .MaximumTriangles = demand.Maximum.Triangles};
}

bool AddSurface(PlantProductCounts &counts,
                PlantSurfaceSize size,
                size_t instances,
                PlantProductCounts most) {
  return Accumulate(counts.Vertices, size.Vertices, 1, most.Vertices) &&
         Accumulate(counts.Triangles, size.Triangles, 1, most.Triangles) &&
         Accumulate(counts.TriangleReferences, size.Triangles, instances, most.TriangleReferences);
}

std::expected<void, std::string_view>
MeasureWood(const PlantPlan &plan, PreparedPlantDemand demand, Measurement &result) {
  const size_t verticesMost =
      std::min<size_t>(std::numeric_limits<uint32_t>::max(), std::vector<float>{}.max_size() / 3);
  const size_t trianglesMost = std::min<size_t>(std::numeric_limits<uint32_t>::max() / 3,
                                                std::vector<uint32_t>{}.max_size() / 3);
  for (const size_t row : result.Part.AxisRows) {
    const auto selected = AxisDemand(plan.Axes()[row], demand);
    if (!selected) { return std::unexpected(selected.error()); }
    const auto size = MeasurePlantAxisSurface(plan, *selected);
    if (!size) { return std::unexpected(size.error()); }
    if (!AddSurface(result.Counts, *size, 1, demand.Maximum) ||
        !Accumulate(result.Wood.Vertices, size->Vertices, 1, verticesMost) ||
        !Accumulate(result.Wood.Triangles, size->Triangles, 1, trianglesMost)) {
      return std::unexpected("prepared plant wood exceeds product or index budget");
    }
  }
  return {};
}

std::expected<Measurement, std::string_view>
Measure(const PlantPlan &plan, std::span<const PlantBlade> blades, PreparedPlantDemand demand) {
  if (demand.AxisParametricErrorM && (demand.AxisSegments != 0 || demand.AxisSides != 0)) {
    return std::unexpected("prepared plant requires exactly one axis detail mode");
  }
  const auto part = plan.Summarize(demand.Root);
  if (!part || blades.size() != part->Variants.size() ||
      part->Axes > std::numeric_limits<uint32_t>::max() ||
      part->Leaves > demand.Maximum.LeafInstances) {
    return std::unexpected("prepared plant requires bounded branch demand and matching variants");
  }
  Measurement result{.Part = *part, .Counts = {.LeafInstances = part->Leaves}, .Wood = {}};
  const auto wood = MeasureWood(plan, demand, result);
  if (!wood) { return std::unexpected(wood.error()); }
  for (size_t variant = 0; variant < blades.size(); ++variant) {
    if (part->Variants[variant] == 0) { continue; }
    const auto size = MeasurePlantBladeSurface(blades[variant], BladeDemand(demand));
    if (!size) { return std::unexpected(size.error()); }
    if (!AddSurface(result.Counts, *size, part->Variants[variant], demand.Maximum)) {
      return std::unexpected("prepared plant blades exceed product or draw budget");
    }
  }
  return result;
}

Mat4 Placement(const std::array<double, 9> &frame, const std::array<double, 3> &origin) {
  Mat4 result;
  for (size_t column = 0; column < 3; ++column) {
    result.At(0, column) = frame[column];
    result.At(1, column) = frame[6 + column];
    result.At(2, column) = -frame[3 + column];
  }
  result.SetTranslation({{origin[0], origin[2], -origin[1]}});
  return result;
}

void PrepareLeaves(PreparedPlant &plant, const PlantPlan::Summary &part) {
  plant.Leaves.reserve(static_cast<size_t>(
      std::ranges::count_if(part.Variants, [](size_t count) { return count != 0; })));
  for (size_t variant = 0; variant < part.Variants.size(); ++variant) {
    const size_t count = part.Variants[variant];
    if (count == 0) { continue; }
    auto &group = plant.Leaves.emplace_back();
    group.Variant = static_cast<uint8_t>(variant);
    group.Ids.reserve(count);
    group.Axes.reserve(count);
    group.Transforms.reserve(count);
  }
}

std::expected<Geometry::Mesh, std::string_view> BuildWood(const PlantPlan &plan,
                                                          PreparedPlant &plant,
                                                          const Measurement &measured,
                                                          PreparedPlantDemand demand) {
  Geometry::Mesh wood;
  wood.PositionsM.reserve(measured.Wood.Vertices * 3);
  wood.Normals.reserve(measured.Wood.Vertices * 3);
  wood.Indices.reserve(measured.Wood.Triangles * 3);
  plant.Axes.reserve(measured.Part.Axes);
  for (const size_t row : measured.Part.AxisRows) {
    const uint64_t id = plan.Axes()[row].Id;
    const auto selected = AxisDemand(plan.Axes()[row], demand);
    if (!selected) { return std::unexpected(selected.error()); }
    auto mesh = BuildPlantAxisSurface(plan, *selected);
    if (!mesh) { return std::unexpected(mesh.error()); }
    plant.Axes.push_back({.Id = id,
                          .FirstIndex = static_cast<uint32_t>(wood.Indices.size()),
                          .IndexCount = static_cast<uint32_t>(mesh->Triangles.size())});
    const auto base = static_cast<uint32_t>(wood.PositionsM.size() / 3);
    wood.PositionsM.append_range(mesh->Positions);
    wood.Normals.append_range(mesh->Normals);
    for (const auto index : mesh->Triangles) { wood.Indices.push_back(base + index); }
  }
  return wood;
}

void BuildPlacements(const PlantPlan &plan, PreparedPlant &plant, const PlantPlan::Summary &part) {
  std::array<size_t, kVariants> groups{};
  for (size_t at = 0; at < plant.Leaves.size(); ++at) { groups[plant.Leaves[at].Variant] = at; }
  for (const size_t row : part.AxisRows) {
    const auto leaves = plan.Leaves({.Axis = plan.Axes()[row].Id, .Maximum = part.Leaves});
    for (const auto &leaf : *leaves) {
      auto &group = plant.Leaves[groups[leaf.Variant]];
      group.Ids.push_back(leaf.Id);
      group.Axes.push_back(leaf.Axis);
      group.Transforms.push_back(Placement(leaf.Frame, leaf.OriginM));
    }
  }
}

std::expected<int, std::string_view>
Adopt(Geometry &geometry, Geometry::Mesh &mesh, MaterialInstance material, std::string_view name) {
  const auto part = geometry.addPart(name, material);
  if (!part) { return std::unexpected(Describe(part.error())); }
  const auto adopted = geometry.adoptMesh(*part, mesh);
  if (!adopted) { return std::unexpected(Describe(adopted.error())); }
  return *part;
}

std::expected<void, std::string_view>
BuildBlades(std::span<const PlantBlade> blades, PreparedPlant &plant, PreparedPlantDemand demand) {
  if (plant.Leaves.empty()) { return {}; }
  for (const auto &group : plant.Leaves) {
    auto blade = BuildPlantBladeSurface(blades[group.Variant], BladeDemand(demand));
    if (!blade) { return std::unexpected(blade.error()); }
    Geometry::Mesh mesh{.PositionsM = std::move(blade->Positions),
                        .Normals = std::move(blade->Normals),
                        .Uv = std::move(blade->Texture),
                        .Uv1 = {},
                        .Tangents = {},
                        .Colours = {},
                        .Indices = std::move(blade->Triangles)};
    const auto part = Adopt(plant.Prototypes, mesh, MaterialInstance(1), "blade");
    if (!part) { return std::unexpected(part.error()); }
  }
  return {};
}
}

std::expected<PlantProductCounts, std::string_view> MeasurePreparedPlant(
    const PlantPlan &plan, std::span<const PlantBlade> blades, PreparedPlantDemand demand) {
  const auto measured = Measure(plan, blades, demand);
  if (!measured) { return std::unexpected(measured.error()); }
  return measured->Counts;
}

std::expected<PreparedPlant, std::string_view>
BuildPreparedPlant(const PlantPlan &plan,
                   std::span<const PlantBlade> blades,
                   const std::array<uint8_t, 32> &identity,
                   const Geometry &materials,
                   PreparedPlantDemand demand) {
  const auto measured = Measure(plan, blades, demand);
  if (!measured) { return std::unexpected(measured.error()); }
  if (materials.parts() != 0 || materials.lamps() != 0 || materials.surfaces() < 1 ||
      materials.surfaces() > 2 ||
      (measured->Counts.LeafInstances > 0 && materials.surfaces() != 2)) {
    return std::unexpected("prepared plant requires an owned wood/leaf material palette");
  }
  PreparedPlant result{.PlanIdentity = identity,
                       .Root = demand.Root,
                       .Prototypes = materials.clone(),
                       .Axes = {},
                       .Leaves = {}};
  auto wood = BuildWood(plan, result, *measured, demand);
  if (!wood) { return std::unexpected(wood.error()); }
  const auto part = Adopt(result.Prototypes, *wood, MaterialInstance(0), "wood");
  if (!part) { return std::unexpected(part.error()); }
  const auto placed =
      result.Prototypes.setPlacement(*part, Placement({1, 0, 0, 0, 1, 0, 0, 0, 1}, {}));
  if (!placed) { return std::unexpected("prepared plant wood placement is invalid"); }
  PrepareLeaves(result, measured->Part);
  BuildPlacements(plan, result, measured->Part);
  const auto leaves = BuildBlades(blades, result, demand);
  if (!leaves) { return std::unexpected(leaves.error()); }
  return result;
}
}
