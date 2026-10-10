#include "Check.h"
#include "SceneRenderer.h"
#include "SubjectProxy.h"
#include <SDL3/SDL.h>
#include <array>
#include <string>

namespace {
void CheckSelection() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  SceneRenderer renderer;
  const auto target = renderer.DrawsInto(32, 32, nullptr);
  CHECK(target.has_value(), "a native render target is available");
  if (!target) { return; }
  constexpr std::array<float, 9> positions{-1, -1, -2, 1, -1, -2, 0, 1, -2};
  constexpr std::array<uint32_t, 6> indices{0, 1, 2, 3, 4, 5};
  std::array<ShapePart, 2> parts;
  for (size_t at = 0; at < parts.size(); ++at) {
    parts[at].Name = at == 0 ? "other" : "streets";
    parts[at].FirstVertex = 3 * at;
    parts[at].FirstIndex = 3 * at;
    parts[at].VertexCount = 3;
    parts[at].IndexCount = 3;
    parts[at].PositionsM = positions;
  }
  Shape shape;
  shape.Parts = parts;
  shape.Indices = indices;
  SubjectProxy proxy;
  proxy.ResetForShape(shape, {});
  CHECK(proxy.ResizeInstances(2), "each original part retains two instance rows");
  SubjectMaterial material;
  constexpr std::array<uint32_t, 2> slots{0, 0};
  std::string error;
  CHECK(proxy.SetMaterials(slots, {&material, 1}, error), "native materials bind");
  SubjectView view;
  view.HasExplicitCamera = true;
  view.Eye.YfovRad = 1;
  view.Eye.ZNearM = .05;
  view.Eye.ZFarM = 10;
  SubjectScratch scratch;
  const std::array<std::string, 1> wanted{"streets"};
  proxy.SelectParts(wanted);
  CHECK(PlanPlacement(renderer, proxy, view, scratch, error), error.c_str());
  const auto &draws = scratch.Draws.Draws();
  CHECK(draws.size() == 1, "only the selected native product enters colour, depth and shadows");
  if (draws.size() == 1) {
    CHECK(draws.front().ModelSlot == 2 && draws.front().Instances == 2 &&
              draws.front().SourceFirstIndex == 3 && draws.front().IndexCount == 3,
          "selection preserves source indices and instance placement rows");
  }
  CHECK(shape.Parts.size() == 2 && shape.Indices.size() == 6 && proxy.Parts() == 2,
        "render selection preserves native geometry and logical placements");
  const std::array<std::string, 1> absent{"missing"};
  proxy.SelectParts(absent);
  CHECK(PlanPlacement(renderer, proxy, view, scratch, error) && scratch.Draws.Draws().empty(),
        "an unmatched explicit selection draws no unintended native products");
  proxy.SelectParts({});
  CHECK(PlanPlacement(renderer, proxy, view, scratch, error) && scratch.Draws.Draws().size() == 2,
        "an empty selection restores every part without rebuilding geometry");
}
}

int main() {
  const bool initialized = SDL_Init(SDL_INIT_VIDEO);
  CHECK(initialized, "video initializes for native selection");
  if (initialized) {
    CheckSelection();
    SDL_Quit();
  }
  return outshine::Test::Report();
}
