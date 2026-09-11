#include <fstream>
#include <iterator>
#include <filesystem>
#include <cstdio>
#include <set>
#include <array>
#include <chrono>
#include <SDL3/SDL.h>
#include <Outshine.h>
#include <scenario/Scenario.h>
#include "TreePrototype.h"
#include "TreeFoliage.h"
#include "TreeLeaf.h"
#include "TreeGrower.h"
#include "TreeSkeleton.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  std::ifstream input("src/assets/world/species/birch.json");
  const std::string text{std::istreambuf_iterator<char>(input), {}};
  TreeSpecies species;
  CHECK(species.Parse(text.data(), text.size()), "the shipped birch parses");
  TreeGrower grower;
  TreeSkeleton first, again;
  grower.Grow(species, first);
  grower.Grow(species, again);
  CHECK(first.Shoots.size() > 1 && !first.LeafPoints.empty(),
        "spawned branches are processed and produce foliage");
  CHECK(first.Nodes.size() == again.Nodes.size(), "growth repeats the node count");
  if (first.Nodes.size() == again.Nodes.size()) {
    bool same = true;
    for (size_t at = 0; at < first.Nodes.size(); ++at) {
      same = same && first.Nodes[at].Pos == again.Nodes[at].Pos &&
             first.Nodes[at].Radius == again.Nodes[at].Radius;
    }
    CHECK(same, "identical seeds reproduce positions and radii exactly");
  }
  const std::string bare = R"({"name":"bare","max_order":0,"terminal_fork":0,"foliate":0})";
  TreeSpecies unbranched;
  CHECK(unbranched.Parse(bare.data(), bare.size()), "nonbranching control parses");
  TreeSkeleton pole;
  grower.Grow(unbranched, pole);
  CHECK(pole.Shoots.size() == 1 && pole.LeafPoints.empty(),
        "negative control: disabling branching produces a pole without foliage");
  const std::string varied = R"({"name":"material","bark_roughness":0.23,"leaf_roughness":0.78})";
  TreeSpecies materialSpecies;
  CHECK(materialSpecies.Parse(varied.data(), varied.size()), "explicit species roughness parses");
  const TreeLook look = TreePrototype::LookOf(materialSpecies);
  CHECK_NEAR(look.BarkRoughness, 0.23, 1e-7, "ratio", "bark roughness is not a renderer constant");
  CHECK_NEAR(look.LeafRoughness, 0.78, 1e-7, "ratio", "leaf roughness is independent of bark");
  const std::string invalid = R"({"name":"invalid","leaf_roughness":1.2})";
  CHECK(!materialSpecies.Parse(invalid.data(), invalid.size()),
        "invalid roughness is refused at the data boundary");
  CHECK(materialSpecies.Definition() == varied && materialSpecies.Name() == "material" &&
            materialSpecies.ShadingParams().LeafRoughness == look.LeafRoughness &&
            materialSpecies.ShadingParams().BarkRoughness == look.BarkRoughness &&
            !materialSpecies.Error().empty(),
        "rejected profile preserves accepted definition and material parameters");
  std::string replacement = R"({"name":"replacement"})";
  CHECK(materialSpecies.Parse(replacement.data(), replacement.size()),
        "replacement profile parses");
  CHECK(materialSpecies.ShadingParams().BarkRoughness ==
                TreeSpecies::kShadingUnsaid.BarkRoughness &&
            materialSpecies.ShadingParams().LeafRoughness ==
                TreeSpecies::kShadingUnsaid.LeafRoughness,
        "a replacement profile uses defaults rather than the preceding profile's roughness");
  CHECK(materialSpecies.Definition() == replacement && materialSpecies.Error().empty(),
        "successful profile replacement publishes its own definition and clears the old error");
  replacement.assign(replacement.size(), 'x');
  CHECK(materialSpecies.Definition() == R"({"name":"replacement"})",
        "profile owns its source after the caller changes the input buffer");
  CHECK(species.Definition() == text,
        "generator profile retains the exact parsed source for cache provenance");
  const auto tree = TreePrototype::Grow(species);
  CHECK(tree.has_value(), "the existing growth model creates a prototype");
  if (!tree) { return Report(); }
  CHECK(!tree->GeometryAt(tree->Ranks().size()), "an absent LOD is refused");
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared(SDL_GetError());
    return Report();
  }
  double coarseLeafArea = 0;
  for (const size_t rank : {size_t{3}, size_t{0}}) {
    CHECK_NEAR(tree->Ranks()[rank].CardLeafM * species.LeafParams().Length,
               species.LeafParams().CardH,
               1e-4,
               "m",
               "distance changes representation without inflating individual leaves");
    const auto started = std::chrono::steady_clock::now();
    const auto geometry = tree->GeometryAt(rank);
    const auto built = std::chrono::steady_clock::now();
    std::printf("tree rank %zu leaf length %.6f m native geometry build %.3f ms (CPU only, no "
                "frame rate)\n",
                rank,
                species.LeafParams().Length * tree->Ranks()[rank].CardLeafM,
                std::chrono::duration<double, std::milli>(built - started).count());
    CHECK(geometry.has_value(), "the prototype produces native geometry");
    if (!geometry) { return Report(); }
    CHECK(geometry->parts() == 2 && geometry->surfaces() == 2,
          "bark and leaves have separate surfaces");
    for (int part = 0; part < geometry->parts(); ++part) {
      const Material &material = geometry->surfaceAt(geometry->materialOf(part));
      CHECK(material.Metalness == 0, "wood and foliage are dielectric");
      const double expected =
          part == 0 ? species.ShadingParams().BarkRoughness : species.ShadingParams().LeafRoughness;
      CHECK_NEAR(
          material.Roughness, expected, 1e-7, "ratio", "roughness retains its species declaration");
      CHECK(material.DoubleSided == (part == 1),
            "leaf surfaces shade both sides, bark remains a solid surface");
      CHECK(!geometry->positionsOf(part).empty() && !geometry->trianglesOf(part).empty(),
            "both surfaces have geometry");
      std::printf("tree part %s vertices %zu triangles %zu\n",
                  geometry->nameOf(part).data(),
                  geometry->positionsOf(part).size() / 3,
                  geometry->trianglesOf(part).size() / 3);
    }
    double leafArea = 0;
    Vec3 projectedArea{};
    const auto leafPositions = geometry->positionsOf(1);
    const auto leafTriangles = geometry->trianglesOf(1);
    for (size_t at = 0; at < leafTriangles.size(); at += 3) {
      std::array<Vec3, 3> corners;
      for (size_t corner = 0; corner < 3; ++corner) {
        const size_t vertex = static_cast<size_t>(leafTriangles[at + corner]) * 3;
        corners[corner] = {
            {leafPositions[vertex], leafPositions[vertex + 1], leafPositions[vertex + 2]}};
      }
      const Vec3 area = Cross(corners[1] - corners[0], corners[2] - corners[0]) * 0.5;
      leafArea += Length(area);
      for (size_t axis = 0; axis < 3; ++axis) { projectedArea[axis] += std::abs(area[axis]); }
    }
    if (rank == 3) {
      coarseLeafArea = leafArea;
    } else {
      CHECK(std::abs(coarseLeafArea / leafArea - 1.0) <= 0.02 + 1e-6,
            "native coarse foliage retains the one-sided area of the fine reference within two "
            "percent");
    }
    std::printf("tree rank %zu one-sided leaf area %.6f m2 projected sums XYZ %.6f %.6f %.6f m2 "
                "(no occlusion or image coverage)\n",
                rank,
                leafArea,
                projectedArea[0],
                projectedArea[1],
                projectedArea[2]);
    if (rank == 3) {
      const auto foliage = geometry->positionsOf(1);
      const size_t bladeFloats = foliage.size() / tree->Ranks()[rank].CardCount;
      std::multiset<std::array<float, 3>> attachments, expectedAttachments;
      const auto &cards = tree->Ranks()[rank].Cards;
      for (size_t at = 0; at < cards.size(); at += TreeFoliage::kFloats) {
        expectedAttachments.insert({cards[at] * species.HeightM(),
                                    cards[at + 1] * species.HeightM(),
                                    cards[at + 2] * species.HeightM()});
      }
      size_t leafCount = 0;
      for (size_t at = 3; at + 2 < foliage.size(); at += bladeFloats) {
        attachments.insert({foliage[at], foliage[at + 1], foliage[at + 2]});
        ++leafCount;
      }
      TreeMesh leafShape;
      CHECK(TreeLeaf::Build(species.LeafParams(), leafShape).has_value(), "species leaf builds");
      TreeFoliage placed;
      placed.Build(first, leafShape, species);
      std::set<std::array<float, 3>> growthAttachments;
      for (const auto &point : first.LeafPoints) {
        growthAttachments.insert({point.Pos[0], point.Pos[1], point.Pos[2]});
      }
      std::printf(
          "attachments growth %zu unique %zu leaves %zu per point %.6f native origins %zu\n",
          first.LeafPoints.size(),
          growthAttachments.size(),
          leafCount,
          placed.PerPoint(),
          attachments.size());
      CHECK(
          attachments == expectedAttachments,
          "native leaf origins preserve input positions and multiplicity instead of forming fans");
    }
    float highest = 0;
    const auto bark = geometry->positionsOf(0);
    for (size_t at = 1; at < bark.size(); at += 3) { highest = std::max(highest, bark[at]); }
    CHECK(highest > species.HeightM() * 0.8f && highest < species.HeightM() * 1.2f,
          "native vertices are in metres rather than normalized tree units");
    for (const bool close : {false, true}) {
      if (close && rank != 0) { continue; }
      Engine engine;
      Scenario::Document scenario;
      scenario.Render.Declared = true;
      scenario.Render.Frame = {640, 720};
      scenario.Lit.Declared = true;
      scenario.Lit.Key.Lux = 20000;
      scenario.Lit.Key.BearingDeg = 135;
      scenario.Lit.Key.ElevationDeg = 40;
      Scenario::View view;
      view.Id = "tree";
      view.Person = "first";
      view.Placement = Scenario::CameraPlacement::Local;
      view.Sees.PositionM = {{0, 11, 35}};
      view.Sees.LooksAt = true;
      view.Sees.LookAtM = {{0, 11, 0}};
      view.Sees.setProjection(Camera::Ortho{
          .XMagM = close ? 1.5 : 13, .YMagM = close ? 1.6875 : 14, .NearM = 0.1, .FarM = 100});
      scenario.Views.push_back(view);
      if (!engine.drawsInto({640, 720}) || !engine.declare(scenario) ||
          !engine.setGeometry(*geometry) || !engine.assemble() || !engine.advance() ||
          !engine.renderer().render({})) {
        Unprepared(engine.error().c_str());
        return Report();
      }
      std::filesystem::create_directories("build/tree-native");
      const char *path = close       ? "build/tree-native/birch-fine-close.png"
                         : rank == 0 ? "build/tree-native/birch-fine.png"
                                     : "build/tree-native/birch.png";
      CHECK(engine.renderer().saveScreenshot(path).has_value(),
            "the native renderer writes the tree image");
    }
  }
  return Report();
}
