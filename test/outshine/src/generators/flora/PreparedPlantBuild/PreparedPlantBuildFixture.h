#ifndef OUTSHINE_TEST_PREPAREDPLANTBUILDFIXTURE_H
#define OUTSHINE_TEST_PREPAREDPLANTBUILDFIXTURE_H
#include "BuildPreparedPlant.h"
#include "PlantPlan.h"
#include "PlantBladeSurface.h"
#include "scene/Geometry.h"
#include "scene/Material.h"
#include "Check.h"
#include <array>
#include <cstdint>
#include <expected>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Test {
inline constexpr uint64_t kRootAxis = 10, kLeftAxis = 20, kRightAxis = 30, kTwigAxis = 40;
inline constexpr uint64_t kLeftLeaf = 100, kRightLeaf = 101, kTwigLeaf = 102, kUnknown = 999;
inline constexpr size_t kProductVertices = 114, kProductTriangles = 110, kDrawTriangles = 114;
inline constexpr std::array<double, 9> kPlantFrame{1, 0, 0, 0, 1, 0, 0, 0, 1};
inline constexpr double kRadiusM = 0.02;

inline std::expected<Generators::PlantPlan, std::string_view> PlantBuildPlan() {
  using namespace Generators;
  const PlantAxis root{
      .Id = kRootAxis, .Frame = kPlantFrame, .LengthM = 1, .RadiiM = {kRadiusM, kRadiusM}};
  PlantAxis left = root;
  PlantAxis right = root;
  PlantAxis twig = root;
  left.Id = kLeftAxis;
  left.Parent = kRootAxis;
  left.Path[0] = 0;
  left.OriginM = {1, 2, 3};
  right.Id = kRightAxis;
  right.Parent = kRootAxis;
  right.Path[0] = 1;
  twig.Id = kTwigAxis;
  twig.Parent = kLeftAxis;
  twig.Path[0] = twig.Path[1] = 0;
  const PlantLeaf leaf{.Id = kLeftLeaf,
                       .Axis = kLeftAxis,
                       .Variant = 2,
                       .OriginM = {2, 3, 5},
                       .Frame = {0, -1, 0, 1, 0, 0, 0, 0, 1}};
  PlantLeaf other = leaf;
  PlantLeaf tip = leaf;
  other.Id = kRightLeaf;
  other.Axis = kRightAxis;
  other.Variant = 0;
  tip.Id = kTwigLeaf;
  tip.Axis = kTwigAxis;
  return PlantPlan::Create({root, left, right, twig}, {leaf, other, tip}, 3);
}

inline std::array<Generators::PlantBlade, 3> PlantBuildBlades() {
  constexpr double kLeafLengthM = 0.1;
  constexpr double kLeafWidthM = 0.05;
  constexpr double kWidest = 0.6;
  constexpr double kBaseFill = 0.1;
  constexpr double kPetioleM = 0.01;
  const Generators::PlantBlade blade{.LengthM = kLeafLengthM,
                                     .WidthM = kLeafWidthM,
                                     .Widest = kWidest,
                                     .BaseFill = kBaseFill,
                                     .PetioleM = kPetioleM};
  auto compact = blade;
  compact.BaseFill = compact.PetioleM = 0;
  return {blade, {}, compact};
}

inline Geometry PlantBuildMaterials() {
  Geometry result;
  CHECK(result.addSurface("wood", Material{}), "wood material is prepared");
  constexpr std::array<uint8_t, 4> kPixel{64, 128, 32, 255};
  const auto image = result.addImage(1, 1, kPixel);
  CHECK(image, "procedural material image is owned by the recipe palette");
  Material leaf;
  leaf.DoubleSided = true;
  leaf.BaseColourMap.Image = image.value_or(-1);
  CHECK(result.addSurface("leaf", leaf), "leaf material retains its own image binding");
  return result;
}

inline Generators::PreparedPlantDemand PlantBuildDemand() {
  return {.Root = kRootAxis,
          .AxisSegments = 2,
          .AxisSides = 4,
          .BladeSegments = 2,
          .BladeAcross = 1,
          .Maximum = {.Vertices = kProductVertices,
                      .Triangles = kProductTriangles,
                      .LeafInstances = 3,
                      .TriangleReferences = kDrawTriangles}};
}
}
#endif
