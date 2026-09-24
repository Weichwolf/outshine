#include <array>
#include <vector>

#include <SDL3/SDL.h>
#include <Outshine.h>
#include <scenario/Scenario.h>

#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for linear texture repetition");
  Geometry geometry;
  const std::array<uint8_t, 16> pixels{
      0, 0, 0, 255, 255, 0, 0, 255, 0, 255, 0, 255, 255, 255, 255, 255};
  const auto image = geometry.addImage(2, 2, pixels);
  CHECK(image, "linear texture fixture owns its image");
  if (!image) { return Report(); }
  Material material;
  material.Unlit = true;
  material.BaseColourMap.Image = *image;
  material.BaseColourMap.Sampler.Magnify = Filter::Linear;
  material.BaseColourMap.Sampler.Minify = Filter::Linear;
  material.BaseColourMap.Sampler.Mip = MipFilter::None;
  const auto surface = geometry.addSurface("linear", material);
  CHECK(surface, "linear texture fixture owns its surface");
  if (!surface) { return Report(); }
  const auto part = geometry.addPart("quad", *surface);
  CHECK(part, "linear texture fixture owns its quad");
  const std::array<float, 12> positions{-1, -1, 0, 1, -1, 0, 1, 1, 0, -1, 1, 0};
  const std::array<float, 12> normals{0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1};
  const std::array<float, 8> uv{0, 1, 1, 1, 1, 0, 0, 0};
  const std::array<uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
  CHECK(part && geometry.setPositions(*part, positions) && geometry.setNormals(*part, normals) &&
            geometry.setTexture(*part, uv) && geometry.setTriangles(*part, indices),
        "linear texture fixture forms a quad");
  if (!part) { return Report(); }
  Scenario::Document scene;
  scene.Render.Declared = true;
  scene.Render.Frame = {128, 128};
  scene.Render.Outputs = {"sceneLinear"};
  Scenario::View view;
  view.Id = "linear";
  view.Person = "first";
  view.Placement = Scenario::CameraPlacement::Local;
  view.Sees.PositionM = {{0, 0, 2}};
  view.Sees.setProjection(Camera::Ortho{.XMagM = 2, .YMagM = 2, .NearM = 0.1, .FarM = 10});
  scene.Views.push_back(view);
  Engine engine;
  CHECK(engine.setRenderTarget({128, 128}) && engine.declare(scene) &&
            engine.setGeometry(geometry) && engine.assemble() && engine.advance(),
        "linear texture scene assembles");
  std::vector<float> first, repeated;
  CHECK(engine.renderer().render({}) && engine.renderer().readPixels(Buffer::Linear, first),
        "first linear texture frame renders");
  CHECK(engine.renderer().render({}) && engine.renderer().readPixels(Buffer::Linear, repeated),
        "second linear texture frame renders");
  CHECK(first == repeated, "linear texture sampling repeats the first static frame exactly");
  SDL_Quit();
  return Report();
}
