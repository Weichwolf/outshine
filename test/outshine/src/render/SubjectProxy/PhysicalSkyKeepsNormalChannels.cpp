#include "Check.h"
#include "SceneRenderer.h"
#include "SubjectProxy.h"
#include <SDL3/SDL.h>
#include <array>
#include <string>

namespace {
void CheckLayouts() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  SceneRenderer renderer;
  CHECK(renderer.DrawsInto(32, 32, nullptr), "the draw planner owns a viewport");
  constexpr std::array<float, 9> positions{-1, -1, -2, 1, -1, -2, 0, 1, -2};
  constexpr std::array<float, 9> normals{0, 0, 1, 0, 0, 1, 0, 0, 1};
  constexpr std::array<uint32_t, 3> indices{0, 1, 2};
  ShapePart part;
  part.VertexCount = part.IndexCount = 3;
  part.PositionsM = positions;
  part.Normals = normals;
  part.HasNormal = true;
  Shape shape;
  shape.Parts = {&part, 1};
  shape.Indices = indices;
  shape.CarriesNormal = true;
  SubjectProxy proxy;
  proxy.ResetForShape(shape, {});
  SubjectView view;
  view.HasExplicitCamera = true;
  view.Eye.YfovRad = 1;
  view.Eye.ZNearM = 0.05;
  view.Eye.ZFarM = 10;
  SubjectScratch scratch;
  std::string error;
  constexpr std::array<uint32_t, 1> slots{0};
  for (const bool unlit : {false, true}) {
    SubjectMaterial material;
    material.Row.Unlit = unlit;
    CHECK(proxy.SetMaterials(slots, {&material, 1}, error), "the native material binds");
    for (const double skyLux : {0.0, 10000.0}) {
      SubjectEnvironment environment;
      environment.SkyLux = skyLux;
      proxy.SetEnvironment(environment);
      CHECK(proxy.Lights().empty(), "no direct light enables the normal stream");
      CHECK(PlanPlacement(renderer, proxy, view, scratch, error), "the native draw plan succeeds");
      CHECK(scratch.Draws.Draws().size() == 1, "the surface remains visible");
      if (scratch.Draws.Draws().size() != 1) { continue; }
      const auto expected =
          skyLux > 0 && !unlit ? VertexLayout::PositionNormal : VertexLayout::Position;
      CHECK(scratch.Draws.Draws().front().Layout == expected,
            "physical environment light needs normals while unlit surfaces retain their fast path");
    }
  }
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for the native draw planner");
  CheckLayouts();
  SDL_Quit();
  return outshine::Test::Report();
}
