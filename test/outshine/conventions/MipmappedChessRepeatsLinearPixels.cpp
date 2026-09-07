#include <algorithm>
#include <cstdio>
#include <cmath>
#include <filesystem>
#include <numbers>
#include <vector>
#include <SDL3/SDL.h>
#include <Outshine.h>
#include <scene/Loaded.h>
#include <scenario/Scenario.h>
#include "Check.h"
#include "PreparedRoot.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const std::string path = PreparedRoot() + "/test-khronos-glTF-ABeautifulGame/scene.gltf";
  Loaded loaded;
  const bool ready = loaded.reads(path);
  CHECK(ready, "pinned Khronos chess geometry loads");
  if (!ready) { return Report(); }
  Scenario::Document scenario;
  scenario.Render.Declared = true;
  scenario.Render.Frame = {1280,720};
  scenario.Render.Outputs = {"sceneLinear"};
  Scenario::Asset asset;
  asset.Uri = path;
  asset.Kind = "gltf";
  asset.Animation = Scenario::AssetAnimation::Ignore;
  for (int part=0; part<loaded.geometry().parts(); ++part) {
    Scenario::SurfaceOverride surface;
    surface.Part = part;
    surface.KeepsMaps = true;
    surface.Row.BaseColour = {{1,1,1,1}};
    surface.Row.Unlit = true;
    asset.Surfaces.push_back(surface);
  }
  scenario.Assets.push_back(asset);
  Scenario::View view;
  view.Id = "chess";
  view.Person = "first";
  view.Sees.Placed = true;
  view.Sees.Stands.AtM = {{2.781138576118416,1.3202289998916963,1.9473741958039332}};
  view.Sees.LooksAt = true;
  view.Sees.LookAtM = {{0,0.08449789705936794,0}};
  view.Sees.setProjection(Scenario::Camera::Perspective{
      .FovDeg=0.47108996144172666*180/std::numbers::pi,
      .NearM=3.107125103623776,.FarM=4.118946968135317});
  scenario.Views.push_back(view);
  if (!SDL_Init(SDL_INIT_VIDEO)) { Unprepared(SDL_GetError()); return Report(); }
  Engine engine;
  if (!engine.drawsInto({1280,720}) || !engine.declare(scenario) || !engine.assemble() || !engine.advance()) {
    Unprepared(engine.error().c_str()); return Report();
  }
  std::vector<float> first, repeated, firstDepth, repeatedDepth;
  CHECK(engine.renderer().render({}) && engine.renderer().readPixels(Buffer::Linear, first), "first chess frame renders");
  CHECK(engine.renderer().readPixels(Buffer::Depth, firstDepth).has_value(), "first depth is read");
  CHECK(first.size() == 1280u*720u*4u, "the full linear frame is read");
  CHECK(std::all_of(first.begin(), first.end(), [](float v) { return std::isfinite(v); }), "the frame contains finite values");
  size_t lit = 0;
  for (size_t at=0; at+3<first.size(); at+=4) {
    if (first[at]>0 || first[at+1]>0 || first[at+2]>0) { ++lit; }
  }
  CHECK(lit > 0, "the chess frame contains visible geometry");
  std::filesystem::create_directories("build/native-materials");
  CHECK(engine.renderer().saveScreenshot("build/native-materials/chess-first.png").has_value(), "first chess PNG is written");
  for (int repeat=0; repeat<3; ++repeat) {
    CHECK(engine.renderer().render({}) && engine.renderer().readPixels(Buffer::Linear, repeated), "resident chess frame renders again");
    size_t changed = 0, firstChanged = first.size();
    float worst = 0;
    for (size_t at=0; at<std::min(first.size(),repeated.size()); ++at) {
      if (first[at] != repeated[at]) { ++changed; firstChanged = std::min(firstChanged,at); }
      worst = std::max(worst, std::abs(first[at]-repeated[at]));
    }
    CHECK(engine.renderer().readPixels(Buffer::Depth, repeatedDepth).has_value(), "repeated depth is read");
    size_t depthChanges=0;
    float depthWorst=0;
    for (size_t at=0; at<std::min(firstDepth.size(),repeatedDepth.size()); ++at) {
      depthChanges += firstDepth[at] != repeatedDepth[at];
      depthWorst = std::max(depthWorst, std::abs(firstDepth[at]-repeatedDepth[at]));
    }
    std::printf("depth changed=%zu worst=%g\n", depthChanges,static_cast<double>(depthWorst));
    std::printf("repeat=%d changed=%zu first=%zu worst=%g\n", repeat,changed,firstChanged,static_cast<double>(worst));
    CHECK(first == repeated, "every linear channel repeats exactly with mipmapped chess materials");
  }
  std::filesystem::create_directories("build/native-materials");
  CHECK(engine.renderer().saveScreenshot("build/native-materials/chess-repeat.png").has_value(), "chess repeatability PNG is written");
  return Report();
}
