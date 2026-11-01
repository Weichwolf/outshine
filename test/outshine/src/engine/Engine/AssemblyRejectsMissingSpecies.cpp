#include <Engine.h>
#include <Scenario.h>
#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr double sightM = 1000;
  constexpr double latitudeDeg = 49;
  std::string temporary =
      (std::filesystem::temp_directory_path() / "outshine-shipped-XXXXXX").string();
  const bool created = mkdtemp(temporary.data()) != nullptr;
  CHECK(created, "isolated shipped assets root created");
  if (!created) { return Report(); }
  const std::filesystem::path root(temporary);
  std::filesystem::create_directories(root / "world");
  for (const char *name : {"ground-materials.json", "vegetation.json"}) {
    std::filesystem::copy_file(std::filesystem::path("data/world") / name, root / "world" / name);
  }
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for public assembly path");
  {
    Engine engine;
    CHECK(engine.setRoots({.BuiltinDataDirectory = temporary,
                           .SourceCacheDirectory = (root / "cache").string(),
                           .Offline = false,
                           .AssetCacheDirectory = {}}),
          "isolated content roots are accepted");
    CHECK(engine.setRenderTarget({32, 32}).has_value(), "offscreen target opens");
    Scenario::Document empty;
    empty.Render.Declared = true;
    empty.Render.Frame = {.WidthPx = 32, .HeightPx = 32};
    empty.Render.Outputs = {"sceneLinear"};
    Scenario::View view;
    view.Id = "retained-camera";
    view.Person = "first";
    view.Placement = Scenario::CameraPlacement::Local;
    view.Sees.PositionM = {{0, 0, 2}};
    view.Sees.setProjection(Camera::Ortho{.XMagM = 1, .YMagM = 1, .NearM = 0.1, .FarM = 10});
    empty.Views.push_back(view);
    CHECK(engine.declare(empty) && engine.assemble(), "initial empty simulation assembles");
    Geometry geometry;
    Material material;
    material.BaseColour = {{0.8F, 0.1F, 0.2F, 1}};
    material.Unlit = true;
    const auto surface = geometry.addSurface("marker", material);
    CHECK(surface, "visible marker has a material");
    if (surface) {
      const auto part = geometry.addPart("marker", *surface);
      CHECK(part, "visible marker has a geometry part");
      if (part) {
        CHECK(geometry.setPositions(
                  *part, std::array<float, 9>{-0.8F, -0.6F, 0, 0.8F, -0.6F, 0, 0, 0.8F, 0}) &&
                  geometry.setNormals(*part, std::array<float, 9>{0, 0, 1, 0, 0, 1, 0, 0, 1}) &&
                  geometry.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}) &&
                  engine.setGeometry(geometry),
              "the active world owns a visible marker");
      }
    }
    std::vector<float> before;
    CHECK(engine.advance() && engine.renderer().render({}) &&
              engine.renderer().readPixels(Buffer::Linear, before) &&
              before.size() == size_t{32} * 32 * 4,
          "the active world renders a complete linear image");
    constexpr size_t center = (size_t{16} * 32 + 16) * 4;
    CHECK(before.size() > center + 2 && before[center] > before[center + 1],
          "the image contains the red marker rather than an empty framebuffer");
    const auto *previous = &engine.entities();
    const auto saved = engine.writeScenario();
    CHECK(saved && engine.settled(),
          "the active world has a persistent and renderable declaration");
    Scenario::Document world;
    world.Ground.Declared = true;
    world.Ground.SightM = sightM;
    world.Ground.VegetationEnabled = true;
    world.Ground.Origin.LatitudeDeg = latitudeDeg;
    world.Ground.Origin.LongitudeDeg = 10;
    CHECK(engine.declare(world).has_value(),
          "world declaration accepted before resource preparation");
    const auto assembled = engine.assemble();
    CHECK(!assembled && assembled.error().contains((root / "world/species").string()),
          assembled ? "assembly unexpectedly succeeded" : assembled.error().c_str());
    CHECK(&engine.entities() == previous,
          "failed world preparation preserves prior simulation ownership");
    const auto retained = engine.writeScenario();
    CHECK(saved && retained && *retained == *saved,
          "late catalogue failure preserves the same active declaration as the simulation");
    CHECK(engine.settled(), "late world preparation failure preserves the active renderable world");
    std::vector<float> after;
    CHECK(engine.advance() && engine.renderer().render({}) &&
              engine.renderer().readPixels(Buffer::Linear, after) && !before.empty() &&
              after == before,
          "late catalogue failure preserves the camera, geometry and rendered pixels");
    CHECK(engine.declare(empty) && engine.assemble(), "valid declaration recovers after the error");
  }
  SDL_Quit();
  std::filesystem::remove_all(root);
  return Report();
}
