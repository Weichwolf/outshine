#ifndef OUTSHINE_TEST_PREPAREDPLANTFIXTURE_H
#define OUTSHINE_TEST_PREPAREDPLANTFIXTURE_H
#include "PreparedPlant.h"
#include "Check.h"
#include "scene/Geometry.h"
#include "scene/Material.h"
#include "math/Mat4.h"
#include <cstdint>

namespace outshine::Test {
inline constexpr uint64_t kPlantRoot = 10, kPlantBranch = 20;
inline constexpr uint64_t kLeafA = 100, kLeafB = 101, kLeafC = 102, kUnknownAxis = 999;

inline Generators::PreparedPlant PreparedPlantFixture() {
  Generators::PreparedPlant plant;
  plant.PlanIdentity[0] = 1;
  plant.Root = kPlantRoot;
  const auto material = plant.Prototypes.addSurface("leaf", Material{.DoubleSided = true});
  CHECK(material, "fixture owns a valid native material");
  for (const char *name : {"wood", "blade-a", "blade-b"}) {
    const auto part = plant.Prototypes.addPart(name, material.value_or(MaterialInstance{}));
    CHECK(part, "fixture owns a native prototype part");
    if (!part) { return plant; }
    Geometry::Mesh mesh;
    mesh.PositionsM = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    mesh.Normals = {0, 0, 1, 0, 0, 1, 0, 0, 1};
    mesh.Uv = {0, 0, 1, 0, 0, 1};
    mesh.Indices = {0, 1, 2};
    if (*part == 0) { mesh.Indices.insert(mesh.Indices.end(), {0, 1, 2}); }
    CHECK(plant.Prototypes.adoptMesh(*part, mesh), "fixture transfers prepared mesh buffers");
  }
  plant.Axes = {{.Id = kPlantRoot, .FirstIndex = 0, .IndexCount = 3},
                {.Id = kPlantBranch, .FirstIndex = 3, .IndexCount = 3}};
  Mat4 first;
  first.SetTranslation({{1, 2, 3}});
  Mat4 second;
  second.SetTranslation({{4, 5, 6}});
  plant.Leaves = {{.Variant = 2,
                   .Ids = {kLeafA, kLeafB},
                   .Axes = {kPlantRoot, kPlantBranch},
                   .Transforms = {first, second}},
                  {.Variant = 7, .Ids = {kLeafC}, .Axes = {kPlantBranch}, .Transforms = {second}}};
  return plant;
}
}
#endif
