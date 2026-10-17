#include "Check.h"
#include "PlantAxisSurface.h"
#include "PlantPlan.h"
#include <cstddef>

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  constexpr double kBaseRadiusM = 0.25;
  constexpr double kTipRadiusM = 0.125;
  constexpr size_t kVertices = 22;
  constexpr size_t kTriangles = 24;
  PlantAxis axis;
  axis.Frame = {1, 0, 0, 0, 1, 0, 0, 0, 1};
  axis.LengthM = 2;
  axis.RadiiM = {kBaseRadiusM, kTipRadiusM};
  axis.OriginM = {3, -1, 2};
  auto plan = PlantPlan::Create({axis}, {}, 1);
  CHECK(plan.has_value(), "straight tapered cylinder has a canonical plan");
  if (!plan) { return Report(); }
  PlantAxisSurfaceDemand demand{.Axis = 0,
                                .Segments = 2,
                                .Sides = 4,
                                .MaximumVertices = kVertices,
                                .MaximumTriangles = kTriangles};
  const auto mesh = BuildPlantAxisSurface(*plan, demand);
  CHECK(mesh.has_value(), "sufficient detail produces a closed oriented surface");
  if (!mesh) { return Report(); }
  CHECK(mesh->Positions.size() == kVertices * 3 && mesh->Normals.size() == kVertices * 3 &&
            mesh->Triangles.size() == kTriangles * 3,
        "hard cap normals have their own vertices within the exact declared budget");
  CHECK(mesh->Positions[0] == axis.OriginM[0] + kBaseRadiusM &&
            mesh->Positions[1] == axis.OriginM[1] && mesh->Positions[2] == axis.OriginM[2],
        "base point respects the supplied prototype origin");
  const size_t top = demand.Segments * demand.Sides * 3;
  CHECK(mesh->Positions[top] == axis.OriginM[0] + kTipRadiusM &&
            mesh->Positions[top + 1] == axis.OriginM[1] + axis.LengthM,
        "tip point follows length and taper without an invented coordinate conversion");
  const size_t base = (demand.Segments + 1) * demand.Sides * 3;
  CHECK(mesh->Normals[base] == 0 && mesh->Normals[base + 1] == -1 && mesh->Normals[base + 2] == 0,
        "base cap has its own hard normal");
  --demand.MaximumVertices;
  CHECK(!BuildPlantAxisSurface(*plan, demand), "vertex underbudget refuses construction");
  ++demand.MaximumVertices;
  --demand.MaximumTriangles;
  CHECK(!BuildPlantAxisSurface(*plan, demand), "triangle underbudget refuses construction");
  ++demand.MaximumTriangles;
  demand.Segments = 0;
  CHECK(!BuildPlantAxisSurface(*plan, demand), "unresolved detail is rejected");
  return Report();
}
