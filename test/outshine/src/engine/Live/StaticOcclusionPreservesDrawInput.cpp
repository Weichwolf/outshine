#include <SDL3/SDL.h>
#include <import/GltfImporter.h>

#include <algorithm>
#include <array>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

#include "Check.h"
#include "Live.h"
#include "PreparedRoot.h"
#include "SceneRenderer.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  GltfImporter imported;
  const auto loaded =
      imported.load(PreparedRoot() + "/test-khronos-glTF-ABeautifulGame/scene.gltf");
  CHECK(loaded.has_value(), "pinned chess geometry loads");
  if (!loaded) { return Report(); }
  Geometry native = imported.geometry().clone();
  for (int surface = 0; surface < native.surfaces(); ++surface) {
    Material row = native.surfaceAt(MaterialInstance(surface));
    row.Unlit = true;
    CHECK(native.setSurface(MaterialInstance(surface), row), "native surface becomes unlit");
  }
  constexpr int selected = 0;
  Geometry single;
  for (int image = 0; image < native.images(); ++image) {
    const ImageView source = native.imageAt(image);
    CHECK(single.addImage(source.WidthPx, source.HeightPx, source.Rgba), "image copies");
  }
  CHECK(single.addSurface(native.surfaceNameOf(native.materialOf(selected).index()),
                          native.surfaceAt(native.materialOf(selected))),
        "selected material copies");
  const auto part = single.addPart(native.nameOf(selected), MaterialInstance(0));
  CHECK(part.has_value(), "selected part allocates");
  if (!part) { return Report(); }
  CHECK(single.setPlacement(*part, native.placementOf(selected)) &&
            single.setPositions(*part, native.positionsOf(selected)) &&
            single.setNormals(*part, native.normalsOf(selected)) &&
            single.setTexture(*part, native.textureOf(selected)) &&
            single.setTexture(*part, native.textureOf(selected, Geometry::UvSet::Uv1), 1) &&
            single.setTangents(*part, native.tangentsOf(selected)) &&
            single.setColours(*part, native.coloursOf(selected)) &&
            single.setTriangles(*part, native.trianglesOf(selected)),
        "selected part copies");
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared(SDL_GetError());
    return Report();
  }
  Core::Declaration declaration;
  declaration.InitialGeometry = &single;
  declaration.SurfaceWidthPx = 1280;
  declaration.SurfaceHeightPx = 720;
  declaration.Outputs = {"sceneLinear"};
  declaration.Stages = {"subjects"};
  Render::SceneRenderer renderer;
  std::unique_ptr<Core::Live> scene;
  std::string error;
  CHECK(Core::Live::Open(renderer, declaration, nullptr, scene, error), "native scene opens");
  if (!scene) { return Report(); }
  constexpr Vec3 eye = {{2.781138576118416, 1.3202289998916963, 1.9473741958039332}};
  constexpr Vec3 aim = {{0, 0.08449789705936794, 0}};
  auto viewpoint = Render::Viewpoint::LookAt({.EyeM = eye, .AimM = aim}, 0.0);
  CHECK(viewpoint.has_value(), "chess camera has a basis");
  if (!viewpoint) { return Report(); }
  viewpoint->YfovRad = 0.47108996144172666;
  viewpoint->ZNearM = 3.107125103623776;
  viewpoint->ZFarM = 4.118946968135317;
  scene->Eye(*viewpoint);
  std::array<Render::KeptDraws, 2> kept;
  std::array<std::vector<float>, 2> pixels;
  for (size_t frame = 0; frame < kept.size(); ++frame) {
    CHECK(scene->Draw(error), "static scene draws");
    renderer.WaitForGpu();
    CHECK(renderer.ReadKeptIndices(kept[frame]) == Render::ReadState::Ready,
          "clustered draw input reads back");
    CHECK(renderer.ReadSceneLinear(pixels[frame]) == Render::ReadState::Ready,
          "linear target reads back");
  }
  std::printf("first indices=%u batches=%u repeated indices=%u batches=%u\n",
              kept[0].Indices,
              kept[0].Batches,
              kept[1].Indices,
              kept[1].Batches);
  const size_t compared = std::min(kept[0].DrawIndex.size(), kept[1].DrawIndex.size());
  for (size_t at = 0; at < compared; ++at) {
    if (kept[0].DrawIndex[at] == kept[1].DrawIndex[at]) { continue; }
    std::printf("first index difference at %zu: %u then %u\n",
                at,
                kept[0].DrawIndex[at],
                kept[1].DrawIndex[at]);
    break;
  }
  CHECK(kept[0].Arguments == kept[1].Arguments, "static indirect arguments repeat exactly");
  CHECK(kept[0].DrawIndex == kept[1].DrawIndex, "static compacted indices repeat exactly");
  CHECK(pixels[0] == pixels[1], "static clustered pixels repeat exactly");
  SDL_Quit();
  return Report();
}
