#include <array>
#include <algorithm>
#include <cmath>
#include <vector>
#include <SDL3/SDL.h>
#include <Outshine.h>
#include <scenario/Scenario.h>
#include <scene/Geometry.h>
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared(SDL_GetError());
    return Report();
  }
  Geometry geometry;
  Material material;
  material.Roughness = 0.3f;
  const int part = geometry.addPart("plane", geometry.addSurface("dielectric", material).value());
  CHECK(geometry.setPositions(part, std::array<float, 12>{-1, -1, 0, 1, -1, 0, 1, 1, 0, -1, 1, 0}),
        "plane positions");
  CHECK(geometry.setNormals(part, std::array<float, 12>{0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1}),
        "plane normals");
  CHECK(geometry.setTriangles(part, std::array<uint32_t, 6>{0, 1, 2, 0, 2, 3}), "plane triangles");
  std::array<std::vector<float>, 2> frames;
  for (size_t at = 0; at < frames.size(); ++at) {
    Engine engine;
    Scenario::Document declaration;
    declaration.Render.Declared = true;
    declaration.Render.Frame = {64, 64};
    declaration.Render.Outputs = {"sceneLinear"};
    declaration.Lit.Declared = true;
    declaration.Lit.Key.Lux = 1;
    declaration.Lit.Key.BearingDeg = 180;
    Scenario::View view;
    view.Id = "parallel";
    view.Person = "first";
    view.Placement = Scenario::CameraPlacement::Local;
    view.Sees.PositionM = {{0, 0, at == 0 ? 2.0 : 8.0}};
    view.Sees.setProjection(Camera::Ortho{.XMagM = 2, .YMagM = 2, .NearM = 0.1, .FarM = 20});
    declaration.Views.push_back(view);
    if (!engine.drawsInto({64, 64}) || !engine.declare(declaration) ||
        !engine.setGeometry(geometry) || !engine.assemble() || !engine.advance() ||
        !engine.renderer().render({}) ||
        !engine.renderer().readPixels(Buffer::Linear, frames[at])) {
      Unprepared(engine.error().c_str());
      return Report();
    }
  }
  CHECK(frames[0].size() == frames[1].size() && !frames[0].empty(),
        "both linear images are readable");
  if (frames[0].size() != frames[1].size() || frames[0].empty()) { return Report(); }
  float error = 0, brightest = 0;
  for (size_t at = 0; at < frames[0].size(); ++at) {
    if (at % 4 == 3) { continue; }
    error = std::max(error, std::abs(frames[0][at] - frames[1][at]));
    brightest = std::max(brightest, frames[0][at]);
  }
  CHECK(brightest > 0, "the control is an illuminated surface, not two empty frames");
  CHECK_NEAR(error,
             0.0f,
             1e-5f,
             "linear",
             "parallel rays preserve shading when the eye moves along its axis");
  return Report();
}
