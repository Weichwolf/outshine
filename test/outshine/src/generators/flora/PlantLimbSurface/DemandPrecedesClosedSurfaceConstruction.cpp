#include "Check.h"
#include "PlantLimbPlan.h"
#include "PlantLimbSurface.h"
#include "PlantSurface.h"
#include <array>
#include <cmath>
#include <cstddef>

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  constexpr std::array anchors{
      PlantLimbAnchor{.Node = 2, .PositionM = {{0, 0, 0}}, .RadiusM = 0.1},
      PlantLimbAnchor{.Node = 17, .PositionM = {{0, 2, 0}}, .RadiusM = 0.05}};
  const auto plan = PlantLimbPlan::Create(anchors);
  CHECK(plan.has_value(), "straight analytic control is valid");
  if (!plan) { return Report(); }
  constexpr size_t kVertices = 22, kTriangles = 24;
  PlantLimbSurfaceDemand demand{
      .Along = 2, .Sides = 4, .MaximumVertices = kVertices, .MaximumTriangles = kTriangles};
  const auto size = MeasurePlantLimbSurface(*plan, demand);
  CHECK(size && size->Vertices == kVertices && size->Triangles == kTriangles,
        "exact demand includes two separate hard-normal cap rings before allocation");
  const auto mesh = BuildPlantLimbSurface(*plan, demand);
  CHECK(mesh && PlantSurfaceIsOriented(*mesh), "tapered surface has finite outward corner normals");
  if (!mesh) { return Report(); }
  CHECK(mesh->Positions.size() == 3 * kVertices && mesh->Triangles.size() == 3 * kTriangles,
        "output uses exactly the preflighted capacity");
  constexpr double kSlope = 0.025, kTolerance = 1e-7;
  const double divisor = std::sqrt(1 + kSlope * kSlope);
  CHECK(std::abs(mesh->Normals[0] - 1 / divisor) < kTolerance &&
            std::abs(mesh->Normals[1] - kSlope / divisor) < kTolerance,
        "taper normal agrees with the analytic frustum independently of frame transport");
  const size_t base = 3 * (demand.Along + 1) * demand.Sides;
  CHECK(mesh->Normals[base] == 0 && mesh->Normals[base + 1] == -1 && mesh->Normals[base + 2] == 0,
        "caps retain their axial normals instead of smoothing into the taper");
  --demand.MaximumVertices;
  CHECK(!BuildPlantLimbSurface(*plan, demand), "a vertex shortfall refuses the whole surface");
  ++demand.MaximumVertices;
  --demand.MaximumTriangles;
  CHECK(!BuildPlantLimbSurface(*plan, demand), "a triangle shortfall refuses the whole surface");
  ++demand.MaximumTriangles;
  demand.Along = 0;
  CHECK(!BuildPlantLimbSurface(*plan, demand), "an unresolved demand cannot publish geometry");
  return Report();
}
