#include <array>
#include <algorithm>
#include <filesystem>
#include <cmath>
#include <vector>
#include <SDL3/SDL.h>
#include <Outshine.h>
#include <import/GltfImporter.h>
#include <scenario/Scenario.h>
#include "Check.h"
#include "PreparedRoot.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const auto accepted = [](const Result &result) {
    if (!result) { Unprepared(result.error().c_str()); }
    return result.has_value();
  };
  const std::string path = PreparedRoot() + "/test-khronos-glTF-ABeautifulGame/scene.gltf";
  GltfImporter loaded;
  const auto ready = loaded.load(path);
  CHECK(ready.has_value(), "pinned Khronos chess geometry loads as native geometry");
  if (!ready) { return Report(); }
  const Geometry native = loaded.geometry().clone();
  constexpr int selected = 0;
  const Material imported = native.surfaceAt(native.materialOf(selected));
  CHECK(imported.BaseColourMap.bound(), "selected chess part has a base-colour image");
  if (!imported.BaseColourMap.bound()) { return Report(); }
  const ImageView source = native.imageAt(imported.BaseColourMap.Image);
  Geometry geometry;
  const int image = *geometry.addImage(source.WidthPx, source.HeightPx, source.Rgba);
  Material material;
  material.BaseColour = {{1, 1, 1, 1}};
  material.Unlit = true;
  material.BaseColourMap.Image = image;
  material.BaseColourMap.Sampler.Magnify = Filter::Linear;
  material.BaseColourMap.Sampler.Minify = Filter::Linear;
  material.BaseColourMap.Sampler.Mip = MipFilter::Linear;
  material.BaseColourMap.Sampler.WrapU = Wrap::Repeat;
  material.BaseColourMap.Sampler.WrapV = Wrap::Repeat;
  const int part =
      geometry.addPart("quad", geometry.addSurface("colour", material).value()).value();
  CHECK(geometry.setPositions(part, std::array<float, 12>{-1, -1, 0, 1, -1, 0, 1, 1, 0, -1, 1, 0}),
        "positions");
  CHECK(geometry.setNormals(part, std::array<float, 12>{0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1}),
        "normals");
  const std::span<const float> importedUv = native.textureOf(selected);
  CHECK(importedUv.size() >= 8, "selected chess part supplies four UV pairs");
  if (importedUv.size() < 8) { return Report(); }
  const std::array<float, 8> quadUv = {importedUv[0],
                                       importedUv[1],
                                       importedUv[2],
                                       importedUv[3],
                                       importedUv[4],
                                       importedUv[5],
                                       importedUv[6],
                                       importedUv[7]};
  CHECK(geometry.setTexture(part, quadUv), "first four imported UV pairs bind to the quad");
  CHECK(geometry.setTriangles(part, std::array<uint32_t, 6>{0, 1, 2, 0, 2, 3}), "triangles");
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared(SDL_GetError());
    return Report();
  }
  Engine engine;
  Scenario::Document scenario;
  scenario.Render.Declared = true;
  scenario.Render.Frame = {1280, 720};
  scenario.Render.Outputs = {"sceneLinear"};
  Scenario::View view;
  view.Id = "native-map";
  view.Person = "first";
  view.Placement = Scenario::CameraPlacement::Local;
  view.Sees.PositionM = {{0, 0, 2}};
  view.Sees.setProjection(Camera::Ortho{.XMagM = 2, .YMagM = 2, .NearM = 0.1, .FarM = 10});
  view.Sees.PositionM = {{2.781138576118416, 1.3202289998916963, 1.9473741958039332}};
  view.Sees.LooksAt = true;
  view.Sees.LookAtM = {{0, 0.08449789705936794, 0}};
  view.Sees.setProjection(
      Camera::Perspective{.FovDeg = 0.47108996144172666 * 180 / std::numbers::pi,
                          .NearM = 3.107125103623776,
                          .FarM = 4.118946968135317});
  scenario.Views.push_back(view);
  std::vector<float> frame;
  if (!accepted(engine.drawsInto({1280, 720})) || !accepted(engine.declare(scenario)) ||
      !accepted(engine.setGeometry(geometry))) {
    return Report();
  }
  geometry.clear();
  if (!accepted(engine.assemble()) || !accepted(engine.advance()) ||
      !accepted(engine.renderer().render({})) ||
      !accepted(engine.renderer().readPixels(Buffer::Linear, frame))) {
    return Report();
  }
  CHECK(frame.size() == 1280u * 720u * 4u, "linear frame has the declared dimensions");
  if (frame.size() != 1280u * 720u * 4u) { return Report(); }
  size_t visible = 0;
  for (size_t at = 0; at + 3 < frame.size(); at += 4) {
    visible += frame[at] > 0.0f || frame[at + 1] > 0.0f || frame[at + 2] > 0.0f;
  }
  CHECK(visible > 0, "perspective minified imported image is visible");
  std::vector<float> repeat;
  CHECK(engine.renderer().render({}) && engine.renderer().readPixels(Buffer::Linear, repeat),
        "repeat renders");
  CHECK(frame == repeat, "perspective resident imported UVs repeat exactly");
  std::filesystem::create_directories("build/native-materials");
  CHECK(engine.renderer()
            .saveScreenshot("build/native-materials/imported-image-quad.png")
            .has_value(),
        "native imported-image PNG is written");
  return Report();
}
