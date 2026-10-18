#include "NativePlantWorldAssets.h"
#include "PlantBladeSurface.h"
#include "PlantRecipeJson.h"
#include "LeafOpticsDefaults.h"
#include "math/Vec3.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::NativePlantWorldProbe;
constexpr double kAnchorToleranceM = 1e-9;
constexpr float kLeafRoughness = 0.8f;
constexpr size_t kMostSourceNodes = 65536;
constexpr size_t kMostLeaves = 100000;

std::expected<PlantLimbPlan, std::string> ReadLimb(std::istream &input,
                                                   const PlantCrownPlan &crown,
                                                   std::span<uint8_t> seen,
                                                   double &radiusMost) {
  size_t nodes = 0;
  size_t along = 0;
  size_t sides = 0;
  if (!(input >> nodes >> along >> sides) || nodes < 2 || nodes > PlantLimbPlan::MostAnchors) {
    return std::unexpected("limb input bounds");
  }
  std::vector<PlantLimbAnchor> anchors(nodes);
  uint32_t previous = 0;
  for (size_t row = 0; row < nodes; ++row) {
    auto &anchor = anchors[row];
    if (!(input >> anchor.Node >> anchor.PositionM[0] >> anchor.PositionM[1] >>
          anchor.PositionM[2] >> anchor.RadiusM) ||
        anchor.Node >= seen.size()) {
      return std::unexpected("limb input row");
    }
    if (Length(anchor.PositionM - Vec3{crown.Skeleton().PositionsM[anchor.Node]}) >
        kAnchorToleranceM) {
      return std::unexpected("wood and foliage have different original source anchors");
    }
    if (row > 0) {
      if (!std::cmp_equal(crown.Skeleton().Parents[anchor.Node], previous) ||
          seen[anchor.Node] != 0) {
        return std::unexpected("source edge ownership");
      }
      seen[anchor.Node] = 1;
    }
    previous = anchor.Node;
    radiusMost = std::max(radiusMost, anchor.RadiusM);
  }
  auto plan = PlantLimbPlan::Create(anchors);
  if (!plan) { return std::unexpected(std::string(plan.error())); }
  return std::move(*plan);
}

std::expected<std::vector<PlantLimbPlan>, std::string>
Wood(std::istream &input, const PlantCrownPlan &crown, double &radiusMost) {
  constexpr size_t kMostLimbs = 16384;
  size_t count = 0;
  if (!(input >> count) || count == 0 || count > kMostLimbs ||
      crown.Skeleton().Parents.size() > kMostSourceNodes) {
    return std::unexpected("limb count");
  }
  std::vector<PlantLimbPlan> plans;
  std::vector<uint8_t> seen(crown.Skeleton().Parents.size(), 0);
  plans.reserve(count);
  for (size_t at = 0; at < count; ++at) {
    auto plan = ReadLimb(input, crown, seen, radiusMost);
    if (!plan) { return std::unexpected(plan.error()); }
    plans.push_back(std::move(*plan));
  }
  for (size_t node = 1; node < seen.size(); ++node) {
    if (seen[node] != 1) { return std::unexpected("original wood edge is missing"); }
  }
  return plans;
}

std::expected<Geometry, std::string> Blades(const std::string &text, size_t segments) {
  const auto recipe = ReadPlantRecipe(text, kAssetBytesMost);
  if (!recipe || recipe->Blades.size() != 8) { return std::unexpected("blade recipe"); }
  Geometry result;
  Material material;
  const auto rgb = DefaultLeafReflection();
  material.BaseColour = {rgb[0], rgb[1], rgb[2], 1};
  material.Roughness = kLeafRoughness;
  material.DoubleSided = true;
  material.Scattering = ScatteringModel::DiffuseThinSheet;
  material.DiffuseTransmission = DefaultLeafTransmission();
  const auto surface = result.addSurface("foliage", material);
  if (!surface) { return std::unexpected("blade material"); }
  for (size_t variant = 0; variant < 8; ++variant) {
    auto leaf = BuildPlantBladeSurface(
        recipe->Blades[variant],
        {.Segments = segments, .Across = 1, .MaximumVertices = 65536, .MaximumTriangles = 262144});
    if (!leaf) { return std::unexpected(std::string(leaf.error())); }
    const auto part = result.addPart(std::to_string(variant), *surface);
    if (!part) { return std::unexpected("blade part"); }
    Geometry::Mesh mesh;
    mesh.PositionsM = std::move(leaf->Positions);
    mesh.Normals = std::move(leaf->Normals);
    mesh.Uv = std::move(leaf->Texture);
    mesh.Indices = std::move(leaf->Triangles);
    if (!result.adoptMesh(*part, mesh)) { return std::unexpected("blade mesh"); }
  }
  return result;
}

double LeafArea(const Geometry &geometry, const PlantCrownPlan &crown) {
  double area = 0;
  for (int variant = 0; variant < geometry.parts(); ++variant) {
    const auto positions = geometry.positionsOf(variant);
    const auto indices = geometry.trianglesOf(variant);
    const auto point = [&](uint32_t index) {
      const size_t at = static_cast<size_t>(index) * 3;
      return Vec3{{positions[at], positions[at + 1], positions[at + 2]}};
    };
    for (size_t at = 0; at < indices.size(); at += 3) {
      const auto a = point(indices[at]);
      const auto b = point(indices[at + 1]);
      const auto c = point(indices[at + 2]);
      area += 0.5 * Length(Cross(b - a, c - a)) *
              static_cast<double>(crown.Summarize().Variants[static_cast<size_t>(variant)]);
    }
  }
  return area;
}

std::expected<std::vector<uint8_t>, std::string> CrownInput(const char *const *argv) {
  if (std::string_view(argv[2]) != "-") {
    auto cache = AssetCache::Open(argv[2]);
    if (!cache) { return std::unexpected("source crown cache"); }
    const auto original = ResolveAssetRequest(**cache, argv[3], kAssetBytesMost, {});
    if (!original) { return std::unexpected(original.error()); }
    return std::vector<uint8_t>(original->Bytes().begin(), original->Bytes().end());
  }
  size_t nodes = 0;
  size_t leaves = 0;
  PlantCrownRecipe recipe;
  if (!(std::cin >> nodes >> leaves >> recipe.Seed >> recipe.LengthM >> recipe.BendRad >>
        recipe.PetioleM >> recipe.FoliageDepthM >> recipe.LeafAngleRad >> recipe.NormalAlignment) ||
      nodes < 2 || nodes > kMostSourceNodes || leaves > kMostLeaves) {
    return std::unexpected("normalized crown bounds");
  }
  PlantGrowthPlan growth;
  growth.PositionsM.resize(nodes);
  growth.Parents.resize(nodes);
  for (size_t at = 0; at < nodes; ++at) {
    auto &point = growth.PositionsM[at];
    if (!(std::cin >> growth.Parents[at] >> point[0] >> point[1] >> point[2])) {
      return std::unexpected("normalized crown nodes");
    }
  }
  std::vector<uint8_t> variants;
  variants.reserve(leaves);
  for (size_t at = 0; at < leaves; ++at) {
    unsigned variant = 0;
    if (!(std::cin >> variant) || variant >= 8) {
      return std::unexpected("normalized crown variants");
    }
    variants.push_back(static_cast<uint8_t>(variant));
  }
  auto crown =
      PlantCrownPlan::Create(std::move(growth), std::move(variants), recipe, kAssetBytesMost);
  if (!crown) { return std::unexpected(std::string(crown.error())); }
  auto bytes = EncodePlantCrownPlan(*crown, kAssetBytesMost);
  if (!bytes) { return std::unexpected(std::string(bytes.error())); }
  return std::move(*bytes);
}

Box Bounds(const PlantCrownPlan &crown, const Geometry &blades, double radiusMost) {
  const auto &points = crown.Skeleton().PositionsM;
  Box result{.Min = Vec3{points.front()}, .Max = Vec3{points.front()}};
  double chordMost = 0;
  double bladeMost = 0;
  for (size_t at = 0; at < points.size(); ++at) {
    for (size_t axis = 0; axis < 3; ++axis) {
      result.Min[axis] = std::min(result.Min[axis], points[at][axis]);
      result.Max[axis] = std::max(result.Max[axis], points[at][axis]);
    }
    if (at > 0) {
      chordMost = std::max(chordMost,
                           Length(Vec3{points[at]} -
                                  Vec3{points[static_cast<size_t>(crown.Skeleton().Parents[at])]}));
    }
  }
  for (int part = 0; part < blades.parts(); ++part) {
    const auto positions = blades.positionsOf(part);
    for (size_t at = 0; at < positions.size(); at += 3) {
      bladeMost =
          std::max(bladeMost, Length(Vec3{{positions[at], positions[at + 1], positions[at + 2]}}));
    }
  }
  const double margin = std::max(chordMost + radiusMost,
                                 crown.Recipe().LengthM + crown.Recipe().PetioleM + bladeMost);
  for (size_t axis = 0; axis < 3; ++axis) {
    result.Min[axis] -= margin;
    result.Max[axis] += margin;
  }
  return result;
}

int Prepare(int argc, const char *const *argv) {
  if (argc != 8) { return 1; }
  const auto original = CrownInput(argv);
  if (!original) {
    std::cerr << original.error();
    return 3;
  }
  const auto crown = DecodePlantCrownPlan(*original, kAssetBytesMost);
  if (!crown) { return 4; }
  std::ifstream woodInput(argv[4]);
  double radiusMost = 0;
  auto limbs = Wood(woodInput, *crown, radiusMost);
  if (!limbs) {
    std::cerr << limbs.error();
    return 5;
  }
  std::ifstream bladeInput(argv[5]);
  const std::string text((std::istreambuf_iterator<char>(bladeInput)), {});
  const std::string_view detail = argv[7];
  constexpr std::array<std::pair<std::string_view, size_t>, 3> profiles{
      {{"2", 2}, {"4", 4}, {"8", 8}}};
  const auto *const profile =
      std::ranges::find(profiles, detail, &decltype(profiles)::value_type::first);
  if (profile == profiles.end()) { return 1; }
  const size_t segments = profile->second;
  const auto blades = Blades(text, segments);
  if (!blades) {
    std::cerr << blades.error();
    return 6;
  }
  const auto wood = EncodePlantLimbs(*limbs, kAssetBytesMost);
  const auto leaf = EncodeGeometryAsset(*blades, kAssetBytesMost);
  if (!wood || !leaf) { return 7; }
  const auto bounds = Bounds(*crown, *blades, radiusMost);
  const std::array<std::span<const uint8_t>, 3> slices{*original, *wood, *leaf};
  const std::array<std::string_view, 3> kinds{
      "plant-crown-plan", "plant-limb-plan", "plant-blade-geometry"};
  const std::array<std::string_view, 3> names{"crown", "wood", "blades"};
  GeneratedAssetPackage package;
  std::string parent;
  for (size_t at = 0; at < slices.size(); ++at) {
    const auto bytes = slices[at];
    const auto key = Sha256Hex(bytes.data(), bytes.size());
    package.Records.push_back({.Key = key,
                               .Kind = std::string(kinds[at]),
                               .Bounds = bounds,
                               .Package = {},
                               .OffsetBytes = package.Bytes.size(),
                               .ByteCount = bytes.size(),
                               .Parent = parent,
                               .RequestKey = Request(argv[6], names[at])});
    package.Bytes.insert(package.Bytes.end(), bytes.begin(), bytes.end());
    if (at == 0) { parent = key; }
  }
  auto cache = AssetCache::Open(argv[1]);
  if (!cache || !(**cache).Publish(package.Records, package.Bytes)) { return 8; }
  auto loaded = Load(**cache, argv[6]);
  if (!loaded) {
    std::cerr << loaded.error();
    return 9;
  }
  std::cout << "nodes=" << loaded->Crown.Summarize().Nodes << " limbs=" << loaded->Wood.size()
            << " leaves=" << loaded->Crown.Summarize().Leaves
            << " package_bytes=" << package.Bytes.size() << " wood_bytes=" << wood->size()
            << " blade_segments=" << segments << " leaf_area_m2=" << LeafArea(*blades, *crown)
            << '\n';
  return 0;
}
}

int main(int argc, const char *const *argv) {
  return Prepare(argc, argv);
}
