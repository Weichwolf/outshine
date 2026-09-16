#include <Outshine.h>
#include <scenario/Scenario.h>
#include "Check.h"
#include <SDL3/SDL.h>
#include <cstdlib>
#include <filesystem>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  std::string temporary =
      (std::filesystem::temp_directory_path() / "outshine-shipped-XXXXXX").string();
  const bool created = mkdtemp(temporary.data()) != nullptr;
  CHECK(created, "isolated shipped assets root created");
  if (!created) { return Report(); }
  const std::filesystem::path root(temporary);
  std::filesystem::create_directories(root / "world");
  for (const char *name : {"ground-materials.json", "vegetation.json"}) {
    std::filesystem::copy_file(std::filesystem::path("src/assets/world") / name,
                               root / "world" / name);
  }
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for public assembly path");
  {
    Engine engine;
    engine.setRoots(
        {.Assets = "src/assets/drive", .Shipped = temporary, .Cache = (root / "cache").string()});
    CHECK(engine.drawsInto({32, 32}).has_value(), "offscreen target opens");
    Scenario::Document empty;
    CHECK(engine.declare(empty) && engine.assemble(), "initial empty simulation assembles");
    const auto *previous = &engine.entities();
    Scenario::Document world;
    world.Ground.Declared = true;
    world.Ground.SightM = 1000;
    world.Ground.VegetationEnabled = true;
    world.Ground.Origin.LatitudeDeg = 49;
    world.Ground.Origin.LongitudeDeg = 10;
    CHECK(engine.declare(world).has_value(),
          "world declaration accepted before resource preparation");
    const auto assembled = engine.assemble();
    CHECK(!assembled &&
              assembled.error().find((root / "world/species").string()) != std::string::npos,
          assembled ? "assembly unexpectedly succeeded" : assembled.error().c_str());
    CHECK(&engine.entities() == previous,
          "failed world preparation preserves prior simulation ownership");
    CHECK(engine.declare(empty) && engine.assemble(), "valid declaration recovers after the error");
  }
  SDL_Quit();
  std::filesystem::remove_all(root);
  return Report();
}
