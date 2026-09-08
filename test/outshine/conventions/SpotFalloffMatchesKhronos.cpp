#include <array>
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
  std::array<float, 3> radiance{};
  for (size_t at = 0; at < radiance.size(); ++at) {
    Geometry geometry;
    Material material;
    material.Roughness = 0.5f;
    const int part = geometry.addPart("plane", geometry.addSurface("dielectric", material));
    CHECK(
        geometry.setPositions(part, std::array<float, 12>{-1, -1, 0, 1, -1, 0, 1, 1, 0, -1, 1, 0}),
        "plane positions");
    CHECK(geometry.setNormals(part, std::array<float, 12>{0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1}),
          "plane normals");
    CHECK(geometry.setTriangles(part, std::array<uint32_t, 6>{0, 1, 2, 0, 2, 3}),
          "plane triangles");
    PunctualLight light;
    light.Kind = at == 0 ? LightKind::Point : LightKind::Spot;
    light.Position = {{0, 0, 2}};
    light.OuterConeRad = std::acos(0.5f);
    if (at == 1) { light.Direction = {{std::sqrt(1.0f - 0.75f * 0.75f), 0, -0.75f}}; }
    geometry.addLamp("measured", light, Mat4{});
    Engine engine;
    Scenario::Document declaration;
    declaration.Render.Declared = true;
    declaration.Render.Frame = {65, 65};
    declaration.Render.Outputs = {"sceneLinear"};
    declaration.Lit.Declared = true;
    declaration.Lit.Key.Lux = 0;
    Scenario::View view;
    view.Id = "parallel";
    view.Person = "first";
    view.Sees.Placed = true;
    view.Sees.Stands.AtM = {{0, 0, 2}};
    view.Sees.setProjection(
        Scenario::Camera::Ortho{.XMagM = 2, .YMagM = 2, .NearM = 0.1, .FarM = 20});
    declaration.Views.push_back(view);
    std::vector<float> frame;
    if (!engine.drawsInto({65, 65}) || !engine.declare(declaration) ||
        !engine.setGeometry(geometry) || !engine.assemble() || !engine.advance() ||
        !engine.renderer().render({}) || !engine.renderer().readPixels(Buffer::Linear, frame)) {
      Unprepared(engine.error().c_str());
      return Report();
    }
    CHECK(frame.size() == 65u * 65u * 4u, "linear image dimensions");
    if (frame.size() != 65u * 65u * 4u) { return Report(); }
    radiance[at] = frame[(32u * 65u + 32u) * 4u];
  }
  CHECK(radiance[0] > 0 && std::isfinite(radiance[0]), "point control illuminates the receiver");
  if (!(radiance[0] > 0)) { return Report(); }
  CHECK_NEAR(radiance[1] / radiance[0],
             0.25f,
             0.002f,
             "ratio",
             "Khronos cosine midpoint: ((0.75 - 0.5) / (1 - 0.5)) squared");
  CHECK_NEAR(radiance[2] / radiance[0],
             1.0f,
             0.002f,
             "ratio",
             "spot axis preserves point intensity at equal distance");
  return Report();
}
