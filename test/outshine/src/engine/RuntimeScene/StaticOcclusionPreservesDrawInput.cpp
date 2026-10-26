#include <SDL3/SDL.h>

#include <array>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "GeometryCullStage.h"

namespace {
outshine::Geometry OccludedGrid() {
  using namespace outshine;
  constexpr int side = 128;
  std::vector<float> positions;
  std::vector<uint32_t> triangles;
  positions.reserve((size_t{side} * side + 1) * 12);
  triangles.reserve((size_t{side} * side + 1) * 6);
  const auto quad = [&](float x, float y, float wide, float z) {
    const auto first = static_cast<uint32_t>(positions.size() / 3);
    positions.insert(positions.end(),
                     {x, y, z, x + wide, y, z, x + wide, y + wide, z, x, y + wide, z});
    for (const uint32_t offset : {0u, 1u, 2u, 0u, 2u, 3u}) { triangles.push_back(first + offset); }
  };
  quad(-1.5f, -1.5f, 3.0f, 0.0f);
  for (int y = 0; y < side; ++y) {
    for (int x = 0; x < side; ++x) {
      constexpr float step = 1.6f / side;
      quad(-0.8f + static_cast<float>(x) * step, -0.8f + static_cast<float>(y) * step, step, -1.0f);
    }
  }
  Geometry geometry;
  Material material;
  material.Unlit = true;
  CHECK(geometry.addSurface("occlusion", material), "unlit native material allocates");
  const auto part = geometry.addPart("layered-occluders", MaterialInstance(0));
  CHECK(part && geometry.setPositions(*part, positions) && geometry.setTriangles(*part, triangles),
        "native front occluder and hidden grid construct without imported assets");
  return geometry;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const Geometry single = OccludedGrid();
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared(SDL_GetError());
    return Report();
  }
  Core::Declaration declaration;
  declaration.InitialGeometry = &single;
  declaration.SurfaceWidthPx = 1280;
  declaration.SurfaceHeightPx = 720;
  declaration.Outputs = {"sceneLinear"};
  declaration.Stages = {"opaqueGeometry"};
  Render::SceneRenderer renderer;
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
        "native scene opens");
  if (!scene) { return Report(); }
  constexpr Vec3 eye = {{0, 0, 5}};
  constexpr Vec3 aim = {{0, 0, 0}};
  auto viewpoint = Render::Viewpoint::LookAt({.EyeM = eye, .AimM = aim}, 0.0);
  CHECK(viewpoint.has_value(), "chess camera has a basis");
  if (!viewpoint) { return Report(); }
  viewpoint->YfovRad = 0.47108996144172666;
  viewpoint->ZNearM = 0.1;
  viewpoint->ZFarM = 20;
  scene->Eye(*viewpoint);
  CHECK(scene->Draw(error), "first frame supplies complete depth for static refinement");
  std::vector<float> completeDepth;
  CHECK(renderer.ReadDepth(completeDepth) == Render::ReadState::Ready,
        "complete first-frame depth reads back");
  Render::KeptDraws initial;
  CHECK(renderer.ReadKeptIndices(initial) == Render::ReadState::Ready,
        "complete first-frame draw input reads back");
  std::array<Render::KeptDraws, 2> kept;
  std::array<std::vector<float>, 2> pixels;
  std::array<std::vector<float>, 2> depths;
  for (size_t frame = 0; frame < kept.size(); ++frame) {
    CHECK(scene->Draw(error), "static scene draws");
    renderer.WaitForGpu();
    CHECK(renderer.ReadKeptIndices(kept[frame]) == Render::ReadState::Ready,
          "clustered draw input reads back");
    CHECK(renderer.ReadSceneLinear(pixels[frame]) == Render::ReadState::Ready,
          "linear target reads back");
    CHECK(renderer.ReadDepth(depths[frame]) == Render::ReadState::Ready, "depth reads back");
  }
  CHECK(kept[0].Indices > 0 && kept[0].Indices < initial.Indices,
        "depth refinement actually removes hidden geometry while keeping its occluder");
  CHECK(completeDepth == depths[0] && depths[0] == depths[1],
        "refinement and reuse preserve every visible depth sample exactly");
  CHECK(kept[0].Visibility == kept[1].Visibility, "static cluster visibility repeats exactly");
  CHECK(kept[0].Arguments == kept[1].Arguments, "static indirect arguments repeat exactly");
  CHECK(kept[0].DrawIndex == kept[1].DrawIndex, "static compacted indices repeat exactly");
  CHECK(pixels[0] == pixels[1], "static clustered pixels repeat exactly");
  CHECK(Render::GeometryCullStage::JobsSweptTaken() == 0,
        "unchanged view reuses clustered draw input");
  viewpoint->EyeM[0] += 0.01;
  scene->Eye(*viewpoint);
  CHECK(scene->Draw(error), "moved camera draws");
  renderer.WaitForGpu();
  CHECK(Render::GeometryCullStage::JobsSweptTaken() > 0,
        "moved camera recomputes clustered draw input");
  SDL_Quit();
  return Report();
}
