#include <fstream>
#include <iterator>
#include <filesystem>
#include <cstdio>
#include <set>
#include <array>
#include <SDL3/SDL.h>
#include <Outshine.h>
#include <scenario/Scenario.h>
#include "TreePrototype.h"
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
  CHECK(!materialSpecies.Parse(invalid.data(), invalid.size()), "invalid roughness is refused at the data boundary");
  const auto tree = TreePrototype::Grow(species);
  CHECK(tree.has_value(), "the existing growth model creates a prototype");
  if (!tree) { return Report(); }
  CHECK(!tree->GeometryAt(tree->Ranks().size()), "an absent LOD is refused");
  const auto geometry = tree->GeometryAt(3);
  CHECK(geometry.has_value(), "the prototype produces native geometry");
  if (!geometry) { return Report(); }
  CHECK(geometry->parts() == 2 && geometry->surfaces() == 2, "bark and leaves have separate surfaces");
  for (int part = 0; part < geometry->parts(); ++part) {
    const Material &material = geometry->surfaceAt(geometry->materialOf(part));
    CHECK(material.Metalness == 0, "wood and foliage are dielectric");
    const double expected = part == 0 ? species.ShadingParams().BarkRoughness
                                       : species.ShadingParams().LeafRoughness;
    CHECK_NEAR(material.Roughness, expected, 1e-7, "ratio", "roughness retains its species declaration");
    CHECK(material.DoubleSided == (part == 1), "leaf surfaces shade both sides, bark remains a solid surface");
    CHECK(!geometry->positionsOf(part).empty() && !geometry->trianglesOf(part).empty(), "both surfaces have geometry");
    std::printf("tree part %s vertices %zu triangles %zu\n", geometry->nameOf(part).data(),
                geometry->positionsOf(part).size()/3, geometry->trianglesOf(part).size()/3);
  }
  TreeMesh blade;
  TreeLeaf::Build(species.LeafParams(), blade);
  const auto foliage = geometry->positionsOf(1);
  std::set<std::array<float, 3>> attachments;
  size_t leafCount = 0;
  for (size_t at = 3; at + 2 < foliage.size(); at += blade.LeafVertexCount() * 3) {
    attachments.insert({foliage[at], foliage[at + 1], foliage[at + 2]});
    ++leafCount;
  }
  CHECK(attachments.size() == leafCount,
        "coarse birch leaves occupy distinct attachments instead of coincident fans");
  float highest = 0;
  const auto bark = geometry->positionsOf(0);
  for (size_t at = 1; at < bark.size(); at += 3) { highest = std::max(highest, bark[at]); }
  CHECK(highest > species.HeightM() * 0.8f && highest < species.HeightM() * 1.2f,
        "native vertices are in metres rather than normalized tree units");
  if (!SDL_Init(SDL_INIT_VIDEO)) { Unprepared(SDL_GetError()); return Report(); }
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
  view.Sees.Placed = true;
  view.Sees.Stands.AtM = {{0, 11, 35}};
  view.Sees.LooksAt = true;
  view.Sees.LookAtM = {{0, 11, 0}};
  view.Sees.setProjection(Scenario::Camera::Ortho{.XMagM=13,.YMagM=14,.NearM=0.1,.FarM=100});
  scenario.Views.push_back(view);
  if (!engine.drawsInto({640,720}) || !engine.declare(scenario) || !engine.setGeometry(*geometry) ||
      !engine.assemble() || !engine.advance() || !engine.renderer().render({})) {
    Unprepared(engine.error().c_str());
    return Report();
  }
  std::filesystem::create_directories("build/tree-native");
  CHECK(engine.renderer().saveScreenshot("build/tree-native/birch.png").has_value(), "the native renderer writes the tree image");
  return Report();
}
