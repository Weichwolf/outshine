#include "WorldCrowns.h"
#include "Live.h"
#include "SceneRenderer.h"
#include "GroundMaterials.h"
#include "VegetationTemplates.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-shared-crown-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

  struct Cleanup {
    std::filesystem::path Path;

    ~Cleanup() {
      std::error_code error;
      std::filesystem::remove_all(Path, error);
    }
  } cleanup{directory};

  std::filesystem::create_directories(directory / "species");
  constexpr auto definition = R"({"name":"shared","max_order":0,"terminal_fork":0,"foliate":0})";
  for (const auto *name : {"a.json", "b.json"}) {
    std::ofstream file(directory / "species" / name);
    file << definition;
    CHECK(file.good(), "duplicate species fixture written");
  }
  Ground::GroundMaterials materials;
  Ground::VegetationTemplates vegetation;
  CHECK(materials.Load("src/assets/world/ground-materials.json"), "materials load");
  CHECK(vegetation.Load("src/assets/world/vegetation.json", materials), "vegetation rules load");
  Generators::Shipping catalogue;
  std::string error;
  CHECK(catalogue.Stands(vegetation, (directory / "species").string(), error), "two clusters load");
  const auto *species = catalogue.TreeFor(Generators::ClusterId{0});
  const auto *second = catalogue.TreeFor(Generators::ClusterId{1});
  CHECK(species && second && species->Definition() == second->Definition(),
        "different clusters share artifact identity");
  if (!species || !second) { return Report(); }
  CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL video starts");
  const auto tree = Generators::TreePrototype::Grow(*species);
  CHECK(tree.has_value(), "fixture prototype grows");
  if (!tree) { return Report(); }
  WorldCrowns::Config config;
  config.Shape = {.Pixels = 32, .Views = 1};
  config.Prototypes = 2;
  config.Instances = 2;
  config.Cache.Store.Directory = (directory / "cache").string();
  const auto atlas = CrownAtlas::Bake(*tree, config.Shape, error);
  CHECK(atlas.has_value(), "fixture atlas captures");
  if (!atlas) { return Report(); }
  Tasks tasks(1);
  CrownCache cache(tasks, config.Cache);
  CHECK(
      cache.Publish(*atlas, CrownAtlas::ProvenanceFor(species->Definition(), config.Shape), error),
      "one shared cache artifact published");
  auto geometry = atlas->GeometryAt(0);
  CHECK(geometry.has_value(), "capture card exists");
  if (!geometry) { return Report(); }
  Render::SceneRenderer renderer;
  Core::Declaration declaration;
  declaration.InitialGeometry = &*geometry;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
  std::unique_ptr<Core::Live> live;
  CHECK(Core::Live::Open(renderer, declaration, nullptr, live, error), "resident renderer opens");
  if (!live) { return Report(); }
  const std::array<WorldInstance, 2> instances{{{.Cluster = 0}, {.Cluster = 1}}};
  auto crowns =
      WorldCrowns::Create(*live, catalogue, instances, TangentFrame::At({}), config, error);
  CHECK(crowns && crowns->Wanted() == 2, "both cluster groups are retained");
  if (!crowns) { return Report(); }
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  bool stepped = true;
  while (!crowns->Ready() && std::chrono::steady_clock::now() < deadline) {
    if (!crowns->Step({{0, 0, 10}}, false, error)) {
      stepped = false;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(stepped, "cached loading requires no preparation");
  CHECK(crowns->Ready() && crowns->Resident() == 2, "one cache result reaches both waiting groups");
  return Report();
}
