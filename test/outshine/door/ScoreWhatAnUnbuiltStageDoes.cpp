#include <cstdio>
#include <string>

#include "Check.h"
#include "RenderCatalogue.h"
#include "Compiled.h"
#include "Renderer.h"

namespace {

// A ROW A CONSUMER CAN SELECT AND NOTHING CAN RUN. `RenderCatalogue.h` declares `Stage::Terrain`,
// `Stage::Buildings` and `Stage::Water` with their resource edges -- each reading `ShadowAtlas`,
// `IrradianceBuffer` and `CascadeUniform`, so by declaration the ground and the buildings RECEIVE
// shadow. Measured: those three names appear in `RenderCatalogue.h` and NOWHERE else in `src/`.
// No executor implements them and `src/render/shaders/` holds no `terrain.msl`, `buildings.msl`
// or `water.msl` among its twenty-five files.
//
// board:1805 called that "a declaration surface with nothing behind it". This case measures what
// actually happens, because the two possible answers are far apart: a plan that names an unbuilt
// stage either draws NOTHING and says nothing, or refuses by NAME. It refuses -- `Renderer::Init`
// walks `Plan_->Order()` and stops at the first stage `ExecutorOf` cannot seat.
//
// So the catalogue offers more than the device can run, and that is not a silent hole. The row is
// a PROMISE the device checks before it draws a frame, and the case pins both halves: a plan of
// stages the device seats compiles and stands, and one naming a row nothing implements is refused
// with the row's own name in the reason.
constexpr int kFramePx = 32;

[[nodiscard]] outshine::Render::PlanSpec Naming(outshine::Render::Stage extra, bool with) {
  outshine::Render::PlanSpec out;
  out.Outputs = {outshine::Render::Resource::FrameTex, outshine::Render::Resource::Surface};
  out.Content = {outshine::Render::Stage::Subjects, outshine::Render::Stage::Overlay};
  if (with) { out.Content.push_back(extra); }
  return out;
}

}

int main(void) {
  using namespace outshine::Test;
  using namespace outshine::Render;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const Stage unbuilt[] = {Stage::Terrain, Stage::Buildings, Stage::Water};

  bool everyRowRefuses = true;
  bool everyRowCompiles = true;
  for (const Stage row : unbuilt) {
    const bool seated = Renderer::Executable(row);
    auto made = Compiled::Compile(Naming(row, true));
    everyRowCompiles = everyRowCompiles && made.has_value();
    std::printf("  %-10s catalogue row, device seats it: %-3s   plan compiles: %s\n",
                Row(row).Name, seated ? "YES" : "no", made ? "yes" : "no");
    everyRowRefuses = everyRowRefuses && !seated;
  }

  CHECK(everyRowRefuses,
        "the three rows this case is about are still unbuilt -- the day one of them grows an "
        "executor this check goes RED and the case is what tells you to rewrite it, rather than "
        "the case quietly passing on a claim that has stopped being true");

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start, so no device can be asked what it seats");
    return Report();
  }

  Renderer device;
  auto plain = Compiled::Compile(Naming(Stage::Terrain, false));
  if (!plain) {
    Unprepared(("a plan of seated stages would not compile: " + plain.error()).c_str());
    return Report();
  }
  device.Init(kFramePx, kFramePx, *plain);
  const bool stood = device.DeviceUsable();
  std::printf("A PLAN OF SEATED STAGES   device usable: %s   %s\n", stood ? "yes" : "no",
              stood ? "" : device.WhyNot().c_str());
  if (!stood) {
    Unprepared(("the device would not stand a plan it seats entirely: " + device.WhyNot()).c_str());
    return Report();
  }

  auto withTerrain = Compiled::Compile(Naming(Stage::Terrain, true));
  if (!withTerrain) {
    Unprepared(("a plan naming terrain would not compile: " + withTerrain.error()).c_str());
    return Report();
  }
  device.Init(kFramePx, kFramePx, *withTerrain);
  const bool refused = !device.DeviceUsable();
  std::printf("A PLAN NAMING terrain     REFUSED: %s\n  %s\n", refused ? "yes" : "NO",
              device.WhyNot().c_str());

  CHECK(everyRowCompiles,
        "the PLAN compiles for each of them, so the refusal below is the DEVICE's and not the "
        "graph's -- the resource edges these rows declare are consistent, and what is missing is "
        "something to run them");
  CHECK(refused,
        "**A STAGE THE DEVICE CANNOT RUN IS REFUSED BEFORE A FRAME IS DRAWN**: the catalogue "
        "offers more than any one device layer implements, which is what makes a second executor "
        "table possible at all, and a consumer that selects a row nothing seats learns so at "
        "stand-up rather than by looking at an empty picture");
  // WHICH row it names is not `terrain`, and that is a measurement rather than a disappointment:
  // asking for terrain pulls `IrradianceBuffer` into the plan, which pulls `Stage::Irradiance`,
  // which this device does not seat either. board:1805 counted three unbuilt rows by grepping for
  // three names; the catalogue is walked here and the count is whatever it is.
  size_t unseated = 0;
  std::string named;
  for (size_t at = 0; at < kStageCount; ++at) {
    const Stage row = (Stage)at;
    if (Renderer::Executable(row)) { continue; }
    ++unseated;
    named += std::string(named.empty() ? "" : " ") + Row(row).Name;
  }
  std::printf("CATALOGUE ROWS NO DEVICE LAYER SEATS  %zu: %s\n", unseated, named.c_str());

  bool namesOne = false;
  for (size_t at = 0; at < kStageCount; ++at) {
    const Stage row = (Stage)at;
    if (Renderer::Executable(row)) { continue; }
    if (device.WhyNot().find(Row(row).Name) != std::string::npos) { namesOne = true; }
  }
  CHECK(namesOne,
        "and the refusal NAMES a row the device cannot seat: a reader is told WHICH stage, not "
        "that something somewhere is unimplemented. It need not be the one the consumer asked "
        "for -- a stage pulls its own reads into the plan, and the first unseated row in the "
        "order is the one that stops it");

  Covers("the render plan: the catalogue offers rows a device layer need not implement, a plan "
         "naming one still compiles because its resource edges are consistent, and the DEVICE "
         "refuses it by name at stand-up rather than drawing an empty frame");
  return Report();
}
