#include "WaterSurfaceMesh.h"
#include "Check.h"
#include "PlanHierarchy.h"
#include "Vec3.h"
#include <limits>
#include <utility>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const std::vector<Vec3> vertices{{{0, 0, 0}}, {{10, 0, 0}}, {{0, 10, 0}}};
  const std::vector<WaterSurfaceMesh::Face> faces{{.Vertices = {0, 1, 2}}};
  const auto mesh = WaterSurfaceMesh::Build({}, vertices, faces);
  CHECK(mesh, "valid reference mesh required");
  if (!mesh) { return Report(); }
  CHECK(!mesh->LevelAt({.LongitudeDeg = 1, .LatitudeDeg = 1}),
        "a position outside the finite water body does not receive an infinite plane");
  CHECK(!mesh->LevelAt({.LongitudeDeg = 180}),
        "a bounded tangent chart never reports water on the far side of the planet");
  CHECK(!WaterSurfaceMesh::Build({}, vertices, {{.Vertices = {0, 0, 2}}}),
        "degenerate triangles cannot enter contact storage");
  CHECK(!WaterSurfaceMesh::Build({}, vertices, {{.Vertices = {0, 2, 1}}}),
        "inverted facets cannot contradict the upward surface convention");
  CHECK(!WaterSurfaceMesh::Build({}, vertices, {{.Vertices = {0, 1, 3}}}),
        "out-of-range source indices cannot escape geometry storage");
  CHECK(!WaterSurfaceMesh::Build({}, {}, faces), "faces require their vertex storage");
  CHECK(!WaterSurfaceMesh::Build({}, vertices, {}), "unused nonempty geometry is not a mesh");
  CHECK(!WaterSurfaceMesh::Build({.LatitudeDeg = 91}, vertices, faces),
        "invalid geographic anchors are rejected before constructing a frame");
  auto nonfinite = vertices;
  nonfinite[0][2] = std::numeric_limits<double>::infinity();
  CHECK(!WaterSurfaceMesh::Build({}, std::move(nonfinite), faces),
        "nonfinite geometry is rejected before partitioning");
  auto nodes = std::vector(mesh->Hierarchy().Nodes().begin(), mesh->Hierarchy().Nodes().end());
  nodes[0].Bounds.Max[0] = 1;
  CHECK(!WaterSurfaceMesh::Restore({}, vertices, faces, std::move(nodes), {0}),
        "a stored bound cannot hide part of its source triangle");
  const auto empty = WaterSurfaceMesh::Build({}, {}, {});
  CHECK(empty && !empty->LevelAt({}) && empty->HeapBytes() == 0,
        "empty assets need neither sentinel facets nor heap storage");
  const auto island = WaterSurfaceMesh::Build({},
                                              {{{-100, -100, 0}},
                                               {{100, -100, 0}},
                                               {{100, 100, 0}},
                                               {{-100, 100, 0}},
                                               {{-20, -20, 0}},
                                               {{20, -20, 0}},
                                               {{20, 20, 0}},
                                               {{-20, 20, 0}}},
                                              {{.Vertices = {0, 1, 5}},
                                               {.Vertices = {0, 5, 4}},
                                               {.Vertices = {1, 2, 6}},
                                               {.Vertices = {1, 6, 5}},
                                               {.Vertices = {2, 3, 7}},
                                               {.Vertices = {2, 7, 6}},
                                               {.Vertices = {3, 0, 4}},
                                               {.Vertices = {3, 4, 7}}});
  CHECK(island && !island->LevelAt({}) && island->LevelAt({.LongitudeDeg = .0005}).has_value(),
        "facets preserve island holes while retaining water around them");
  return Report();
}
