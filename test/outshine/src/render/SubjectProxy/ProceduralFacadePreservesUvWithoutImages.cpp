#include <array>
#include <string>
#include <SDL3/SDL.h>
#include "Check.h"
#include "SceneRenderer.h"
#include "SubjectProxy.h"

namespace {
void CheckLayouts() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  SceneRenderer renderer;
  const auto target = renderer.DrawsInto(32, 32, nullptr);
  CHECK(target.has_value(), "the native draw planner has a viewport");
  if (!target) { return; }
  constexpr std::array<float, 9> positions{-1, -1, -2, 1, -1, -2, 0, 1, -2};
  constexpr std::array<float, 6> uv{0, -1, 1, -1, 0, -2};
  constexpr std::array<uint32_t, 3> indices{0, 1, 2};
  ShapePart part;
  part.VertexCount = 3;
  part.IndexCount = 3;
  part.PositionsM = positions;
  part.Uv = uv;
  part.HasUv = true;
  Shape shape;
  shape.Parts = {&part, 1};
  shape.Indices = indices;
  shape.CarriesUv = true;
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
  for (const auto pattern : {SurfacePattern::None, SurfacePattern::Facade}) {
    SubjectMaterial material;
    material.Row.Pattern = pattern;
    CHECK(!material.ReadsAnyImage(), "the procedural material carries no image");
    CHECK(proxy.SetMaterials(slots, {&material, 1}, error), "native material binds");
    CHECK(PlanPlacement(renderer, proxy, view, scratch, error), "native draw plan succeeds");
    CHECK(scratch.Draws.Draws().size() == 1, "the triangle remains in the draw plan");
    if (scratch.Draws.Draws().size() != 1) { continue; }
    const auto expected =
        pattern == SurfacePattern::Facade ? VertexLayout::PositionUv : VertexLayout::Position;
    CHECK(scratch.Draws.Draws().front().Layout == expected,
          "facade coordinates survive without allocating UVs for ordinary materials");
  }
}
}

int main() {
  const bool initialized = SDL_Init(SDL_INIT_VIDEO);
  CHECK(initialized, "video initializes for the native draw planner");
  if (initialized) {
    CheckLayouts();
    SDL_Quit();
  }
  return outshine::Test::Report();
}
